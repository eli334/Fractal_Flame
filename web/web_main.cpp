#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "../engines/web_engine.cpp"

// ---------------------------------------------------------------------------
// JS glue
// ---------------------------------------------------------------------------
// Raw <canvas> 2D context + putImageData -- no WebGL. fillPixelBuffer() already
// produces RGBA8 bytes in exactly the layout ImageData wants, so this is a
// straight byte copy out of the WASM heap, no shaders or textures involved.
//
// Pan/zoom input is read here too, but handled almost entirely in shell.html's
// inline <script> (see that file) rather than via Emscripten's native pointer/
// wheel callback API. Reason: this avoids depending on the exact current
// field names of EmscriptenPointerEvent/EmscriptenWheelEvent, which I can't
// verify against your specific Emscripten build from here (no network access
// in my sandbox, and "6.0.2" is newer than what I have confident knowledge
// of). Polling a couple of plain JS globals once per frame is a smaller
// surface to get wrong, and easy to hand-debug directly in shell.html without
// recompiling if the feel needs tuning.

EM_JS(void, js_resize_canvas, (int width, int height), {
    const canvas = document.getElementById('canvas');
    canvas.width = width;
    canvas.height = height;
    Module.ctx2d = canvas.getContext('2d');
    Module.imageData = Module.ctx2d.createImageData(width, height);
});

EM_JS(void, js_blit_pixels, (uintptr_t ptr, int byteLength), {
    const src = HEAPU8.subarray(ptr, ptr + byteLength);
    Module.imageData.data.set(src);
    Module.ctx2d.putImageData(Module.imageData, 0, 0);
});

// Each call drains and resets the corresponding JS-side accumulator -- see
// the pointerdown/pointermove/wheel listeners in shell.html. Draining once
// per frame (rather than reacting per-event) means bursts of pointermove
// events between frames get coalesced into one camera update, which is what
// we want anyway since we only redraw once per frame.
EM_JS(double, js_take_pan_dx, (), { const v = panDX; panDX = 0; return v; });
EM_JS(double, js_take_pan_dy, (), { const v = panDY; panDY = 0; return v; });
EM_JS(double, js_take_zoom_delta, (), { const v = zoomDelta; zoomDelta = 0; return v; });

// ---------------------------------------------------------------------------
// App state
// ---------------------------------------------------------------------------

struct AppState {
    std::unique_ptr<Web_Engine> engine;
    ColorState colorState;
    Camera view; // display-time pan/zoom into the histogram -- see fillPixelBuffer.
                 // This is NOT the accumulation Viewport (Engine::viewport), so
                 // panning/zooming the Camera never wipes or re-renders anything;
                 // it's a free re-crop of whatever's already been accumulated,
                 // same as the native app's left-drag/scroll-wheel behavior.
    std::vector<uint8_t> pixels;
    int width = 800;
    int height = 600;

    // Adaptive iteration budget -- see main_loop(). Starts conservative and
    // converges to whatever this device can do in ~16ms within a few frames.
    double avgFrameMs = 16.0;
    uint64_t iterationsPerFrame = 5000;

    static constexpr double targetFrameMs = 16.0; // ~60fps
    static constexpr uint64_t minIterationsPerFrame = 1000;
    static constexpr uint64_t maxIterationsPerFrame = 20'000'000;
};

// ---------------------------------------------------------------------------
// Main loop -- called once per requestAnimationFrame tick by the browser.
// ---------------------------------------------------------------------------

void main_loop(void* arg) {
    AppState* app = static_cast<AppState*>(arg);

    // 1. Drain input accumulated since last frame, apply to the display Camera.
    double panDX = js_take_pan_dx();
    double panDY = js_take_pan_dy();
    double zoomDelta = js_take_zoom_delta();

    if (panDX != 0.0 || panDY != 0.0) {
        app->view.centerX -= (panDX / app->width) / app->view.zoom;
        app->view.centerY -= (panDY / app->height) / app->view.zoom;
    }
    if (zoomDelta != 0.0) {
        app->view.zoom *= (1.0 + zoomDelta);
        app->view.zoom = std::clamp(app->view.zoom, 0.01, 1.0e6);
    }

    // 2. Run the chaos game for this frame's iteration budget, timed so we can
    //    adapt the budget for next frame.
    double t0 = emscripten_get_now();

    app->engine->runBatch(app->iterationsPerFrame);

    // 3. Re-render only if the histogram's peak density actually moved --
    //    fillPixelBuffer() already tracks this internally and returns false
    //    on frames where nothing would visibly change, so this also protects
    //    weaker devices from paying for a putImageData they don't need.
    bool changed = app->engine->fillPixelBuffer(
        app->pixels.data(),
        app->colorState.getPalettePtr(),
        app->colorState.numColors,
        app->colorState.gamma,
        app->view
    );

    if (changed) {
        js_blit_pixels(reinterpret_cast<uintptr_t>(app->pixels.data()), app->width * app->height * 4);
    }

    // 4. Adapt iterationsPerFrame toward the target frame budget. Smoothed
    //    with an EMA so one slow frame (GC pause, thermal blip) doesn't cause
    //    a wild overcorrection on the next.
    double elapsedMs = emscripten_get_now() - t0;
    app->avgFrameMs = 0.9 * app->avgFrameMs + 0.1 * elapsedMs;

    double ratio = AppState::targetFrameMs / app->avgFrameMs;
    double newTarget = static_cast<double>(app->iterationsPerFrame) * ratio;
    app->iterationsPerFrame = static_cast<uint64_t>(std::clamp(
        newTarget,
        static_cast<double>(AppState::minIterationsPerFrame),
        static_cast<double>(AppState::maxIterationsPerFrame)
    ));
}

// ---------------------------------------------------------------------------

int main() {
    // Heap-allocated and intentionally never freed: emscripten_set_main_loop_arg
    // with simulate_infinite_loop=true means main() returns control to the
    // browser immediately, but keeps getting called back into indefinitely.
    // Anything main_loop() touches has to outlive that "return".
    AppState* app = new AppState();

    app->engine = std::make_unique<Web_Engine>();

    // Reuses Engine::randomize()/calculateViewport() as-is -- same call the
    // native app's "Randomize!" button makes. Picks 2-8 random transforms,
    // statistically estimates a Viewport that frames the resulting attractor,
    // and returns a seed for a matching color palette.
    int colorSeed = app->engine->randomize();
    app->colorState.randomizeColors(static_cast<int>(app->engine->getTransforms().size()), colorSeed);

    double cssWidth = 0.0, cssHeight = 0.0;
    emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight);
    app->width  = cssWidth  > 0 ? static_cast<int>(cssWidth)  : 800;
    app->height = cssHeight > 0 ? static_cast<int>(cssHeight) : 600;

    app->engine->resize(app->width, app->height);
    js_resize_canvas(app->width, app->height);
    app->pixels.resize(static_cast<size_t>(app->width) * app->height * 4);

    app->engine->start();

    emscripten_set_main_loop_arg(main_loop, app, 0, EM_TRUE);

    return 0; // unreachable
}

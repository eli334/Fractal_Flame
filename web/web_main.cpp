#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "../engines/web_engine.cpp"

// JS glue -- raw <canvas> 2D + putImageData, no WebGL. fillPixelBuffer() returns
// RGBA8 already in ImageData layout, so the blit is just a byte copy.
// Pan/zoom is read from plain JS globals set in shell.html (not Emscripten's
// pointer/wheel callbacks) -- less surface to get wrong, tweakable without recompiling.

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

// drain + reset the JS-side accumulators (listeners in shell.html); once per
// frame, so bursts of pointermove coalesce into one update
EM_JS(double, js_take_pan_dx, (), { const v = panDX; panDX = 0; return v; });
EM_JS(double, js_take_pan_dy, (), { const v = panDY; panDY = 0; return v; });
EM_JS(double, js_take_zoom_delta, (), { const v = zoomDelta; zoomDelta = 0; return v; });

// ---------------------------------------------------------------------------
// App state
// ---------------------------------------------------------------------------

struct AppState {
    std::unique_ptr<Web_Engine> engine;
    ColorState colorState;
    Camera view; // display-time pan/zoom -- NOT the accumulation Viewport, so
                 // panning never wipes the histogram, just re-crops it
    std::vector<uint8_t> pixels;
    int width = 800;
    int height = 600;

    // adaptive iteration budget -- converges to ~16ms/frame (see main_loop)
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

    // 2. run the chaos game for this frame's budget, timed to adapt next frame
    double t0 = emscripten_get_now();

    app->engine->runBatch(app->iterationsPerFrame);

    // 3. re-render only if peak density moved -- fillPixelBuffer() returns
    //    false when nothing would visibly change, skipping the blit
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

    // 4. adapt iterationsPerFrame toward target, EMA-smoothed so one slow
    //    frame (GC pause) doesn't overcorrect
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
    // heap-allocated, never freed: simulate_infinite_loop=true returns main()
    // to the browser but keeps calling main_loop, so app must outlive it
    AppState* app = new AppState();

    app->engine = std::make_unique<Web_Engine>();

    // same as the native "Randomize!" -- 2-8 random transforms, estimated
    // viewport, returns a palette seed
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
#include <emscripten.h>
#include <emscripten/html5.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>

#include "../engines/web_engine.cpp"

// JS glue -- raw <canvas> 2D + putImageData, no WebGL. fillPixelBuffer() returns
// RGBA8 already in ImageData layout, so the blit is just a byte copy.
// Pan/zoom is read from plain JS globals set in shell.html
// Pan/zoom is read from plain JS globals set in shell.html

EM_JS(void, js_resize_canvas, (int backingW, int backingH, int cssW, int cssH), {
    const canvas = document.getElementById('canvas');
    
    canvas.width = backingW;
    canvas.height = backingH;

    canvas.style.width = cssW + 'px';
    canvas.style.height = cssH + 'px';
    
    Module.ctx2d = canvas.getContext('2d');
    Module.imageData = Module.ctx2d.createImageData(backingW, backingH);
});

EM_JS(void, js_blit_pixels, (uintptr_t ptr, int byteLength), {
    const src = HEAPU8.subarray(ptr, ptr + byteLength);
    Module.imageData.data.set(src);
    Module.ctx2d.putImageData(Module.imageData, 0, 0);
});

// read seed from url hash (#12345). returns 0 and sets *ok=0 if absent/bad.
EM_JS(int, js_read_seed, (int* ok), {
    const h = window.location.hash.slice(1);
    if (h.length === 0) { HEAP32[ok >> 2] = 0; return 0; }
    const n = parseInt(h, 10);
    if (Number.isNaN(n)) { HEAP32[ok >> 2] = 0; return 0; }
    HEAP32[ok >> 2] = 1;
    return n >>> 0; // unsigned -- hash carries a uint32
});

// write seed to url hash + the seed input field, without adding history entries
EM_JS(void, js_write_seed, (uint32_t seed), {
    history.replaceState(null, "", '#' + seed);
    const el = document.getElementById('seed');
    if (el) el.value = seed;
});

EM_JS(void, js_write_quality, (int longEdgePx), {
    const el = document.getElementById('quality');
    if (el) el.value = longEdgePx;
});

// "taker"s? from browser->C++

EM_JS(double, js_take_pan_dx, (), { 
    const v = panDX; 
    panDX = 0; 
    return v; 
});

EM_JS(double, js_take_pan_dy, (), { 
    const v = panDY; 
    panDY = 0; 
    return v; 
});

EM_JS(double, js_take_zoom_factor, (), { 
    const v = zoomFactor; 
    zoomFactor = 1; 
    return v; 
});

EM_JS(void, js_set_commit_enabled, (int enabled), {
    const el = document.getElementById('commit');
    if (el) el.disabled = !enabled;
});



struct AppState {
    std::unique_ptr<Web_Engine> engine;
    ColorState colorState;
    Camera view; // display-time pan/zoom -- not the accumulated Viewport, so
                 // panning never wipes the histogram, just re-crops it
    std::vector<uint8_t> pixels;
    int width = 800, height = 600;
    int displayW = 0, displayH = 0; // Measured at boot 
    int quality = 1200; // long edge pixels; 4:3

    // adaptive iteration budget -- converges to ~16ms/frame (see main_loop)
    double avgFrameMs = 16.0;
    uint64_t iterationsPerFrame = 5000;

    static constexpr double targetFrameMs = 16.0; // ~60fps
    static constexpr uint64_t minIterationsPerFrame = 1000;
    static constexpr uint64_t maxIterationsPerFrame = 20'000'000;

    void applySeed(int seed) {
        engine->stop();
        int colorSeed = engine->randomize(seed);
        colorState.randomizeColors(engine->getTransforms().size(), colorSeed);
        view = Camera{};
        engine->start();
        js_write_seed(static_cast<uint32_t>(seed));
    }

    void applyQuality() {
        engine->stop();

        double shortRatio = (double)std::min(displayW, displayH) / std::max(displayW, displayH);
        int longEdge  = quality;
        int shortEdge = (int)(longEdge * shortRatio + 0.5);
        if (displayW >= displayH) { 
            width = longEdge;  
            height = shortEdge; 
        } else { 
            width = shortEdge; 
            height = longEdge;  
        }

        engine->resize(width, height);              // resize to new width and height
        pixels.resize((size_t)width * height * 4);
        js_resize_canvas(width, height, displayW, displayH);
        avgFrameMs = targetFrameMs;                 // neutral prior so the fps controller re-converges
        engine->start();
        js_write_quality(quality);  
    }
    
    // commit the current display crop to the accumulation viewport, then rebuild.
    void commitViewToViewport() {
        engine->stop();

        Viewport current = engine->getViewport();       // adjust to your accessor
        double spanX = current.maxX - current.minX;
        double spanY = current.maxY - current.minY;

        // visible rect in normalized histogram space [0,1]
        double half = 0.5 / view.zoom;
        double uMin = view.centerX - half, uMax = view.centerX + half;
        double vMin = view.centerY - half, vMax = view.centerY + half;

        // normalized -> world. y is flipped: v=0 is the top row, i.e. maxY
        Viewport next;
        next.minX = current.minX + uMin * spanX;
        next.maxX = current.minX + uMax * spanX;
        next.minY = current.minY + (1.0 - vMax) * spanY;
        next.maxY = current.minY + (1.0 - vMin) * spanY;

        engine->setViewport(next);                      // adjust to your setter
        view = Camera{};                                // display crop is now the whole buffer
        avgFrameMs = targetFrameMs;                     // neutral prior, controller re-converges
        engine->start();
    }
};

static AppState* g_app = nullptr;

void main_loop(void* arg) {
    AppState* app = static_cast<AppState*>(arg); // heap allocated in main, so it doesn
    
    // 1. Drain input accumulated since last frame, apply to the display Camera.
    double panDX = js_take_pan_dx();
    double panDY = js_take_pan_dy();
    double zoomFactor = js_take_zoom_factor();

    if (panDX != 0.0 || panDY != 0.0) {
        app->view.centerX -= (panDX / app->displayW) / app->view.zoom;
        app->view.centerY -= (panDY / app->displayH) / app->view.zoom;
    }
    if (zoomFactor != 0.0) {
        app->view.zoom *= zoomFactor;
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

    // gray out commit when the view is untouched -- committing at identity
    // would wipe and re-render the same image, which reads as a bug.
    static Camera defaultView;
    static int lastEnabled = -1; // -1 forces the first write
    const double eps = 1e-9;     // pan-and-return won't land exactly on 0
    bool identity = std::abs(app->view.centerX - defaultView.centerX) < eps
                 && std::abs(app->view.centerY - defaultView.centerY) < eps
                 && std::abs(app->view.zoom    - defaultView.zoom)    < eps;
    int enabled = identity ? 0 : 1;
    if (enabled != lastEnabled) {
        js_set_commit_enabled(enabled);
        lastEnabled = enabled;
    }
}   

// ---------------------------------------------------------------------------

int main() {
    // heap-allocated, never freed
    // allocated now and fed into void* in emscripten_set_main_loop_arg
    // heap-allocated, never freed
    // allocated now and fed into void* in emscripten_set_main_loop_arg
    AppState* app = new AppState();
    g_app = app;
    
    app->engine = std::make_unique<Web_Engine>();

    // same as the native "Randomize!" -- 2-8 random transforms, estimated
    // viewport, returns a palette seed
    int colorSeed = app->engine->randomize();
    app->colorState.randomizeColors(static_cast<int>(app->engine->getTransforms().size()), colorSeed);

    double cssW = 0.0, cssH = 0.0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    app->displayW = cssW > 0 ? static_cast<int>(cssW) : 800; // if > 0, set to width. otherwise, set to 800
    app->displayH = cssH > 0 ? static_cast<int>(cssH) : 600;

    app->quality = 640;
    app->applyQuality();

    int ok = 0;
    int seed = js_read_seed(&ok);
    if (!ok) {
        std::random_device rd;
        seed = static_cast<int>(rd());
    }

    app->applySeed(seed); // does randomize + colors + start + writes url
    
    emscripten_set_main_loop_arg(main_loop, app, 0, EM_TRUE);

    return 0; // unreachable
}


extern "C" EMSCRIPTEN_KEEPALIVE void apply_seed(int seed) {
    if(g_app) g_app->applySeed(seed);
}

extern "C" EMSCRIPTEN_KEEPALIVE void set_quality(int longEdgePx) {
    if (!g_app) return;
    double shortRatio = (double)std::min(g_app->displayW, g_app->displayH) / std::max(g_app->displayW, g_app->displayH);
    double bins = (double)longEdgePx * longEdgePx * shortRatio;
    if (bins * 20.0 > 170.0 * 1024 * 1024) {    // ~20 B/bin, keep clear of the 256MB heap
        printf("quality %d too large for heap, ignoring\n", longEdgePx);
        return;
    }
    g_app->quality = longEdgePx;
    g_app->applyQuality();
}

extern "C" EMSCRIPTEN_KEEPALIVE void commit_view(void) {
    if (g_app) g_app->commitViewToViewport();
}
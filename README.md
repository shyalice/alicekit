<div align="center">

# 🎀 alicekit 2.0 | t.me/imguiseller

**imgui, but better.**

a high-level rendering framework built on top of dear imgui — designed for people who actually care about how their UI looks and feels.

`directx11` · `opengl3`

---

</div>

## ✦ what is this

alicekit is a fully custom imgui extension layer that replaces the clunky, default imgui experience with something that actually feels modern. smooth scrolling, real post-processing, drag-and-drop workflows, and a model engine with physics — all wired into a clean, flexible API.

this isn't a fork. it's a layer on top — meaning you keep full imgui compatibility while gaining access to everything below.

## ✦ features

### 🖱️ custom scroll
butter-smooth scrolling with momentum, easing curves, and per-widget scroll regions. no more janky default imgui scroll behavior.

### 🌫️ graphic effects
real-time post-processing pipeline with support for:
- **blur** — gaussian, kawase, dual-pass variants
- **glow** — bloom with configurable threshold and intensity
- **grayscaling** — per-element or full-viewport desaturation

### 🧩 full flexibility
everything is modular. swap renderers, inject custom draw commands, override any pipeline stage. alicekit stays out of your way until you need it.

### 📦 drag-n-drop esp solution
plug-and-play esp framework with a visual editor. drag elements onto your overlay, bind data sources, and ship — no boilerplate required.

### 🧬 custom model engine
lightweight 3D model renderer with skeleton support and basic physics simulation. render characters, props, or anything else — directly inside your imgui viewport.

## ✦ supported backends

| backend | status |
|---|---|
| DirectX 11 | ✅ supported |
| OpenGL 3 | ✅ supported |

## ✦ known issues

| module | description |
|---|---|
| blur | doesn't work properly on opengl, kinda ez to fix tho |

## ✦ getting started

```cpp
#include <alicekit/alicekit.hpp>

/* PLEASE NOTE: blur is made for dwm overlay rendering, and it requires copying your screen before doing that. since that why streamproof thing is used. you can easliy rewrite it without screen-copy solution */

// main
alice::core = std::make_unique< alice::c_core >( );

alice::core->assets->init_loader( []( void const* data, size_t data_size, ImTextureID* out, bool pixel_perfect ) -> void {
    int image_width = 0;
    int image_height = 0;
    ID3D11ShaderResourceView* out_d3d = nullptr;
    core->graphics->load_texture_from_memory( data, data_size, &out_d3d, &image_width, &image_height, pixel_perfect );
    *out = ( ImTextureID ) out_d3d;
} );

// after initializing imgui backend
RECT rc;
GetWindowRect( GetDesktopWindow( ), &rc );
alice::core->init_backend< alice::c_dx11_backend >(
    c_graphics::device, c_graphics::device_context,
    rc.right - rc.left, rc.bottom - rc.top );

alice::core->assets->load_premade( );
alice::core->fonts->initialize( );
alice::core->ui->initialize( );

alice::core->blur->init_duplication( );

// preferably once in render
alice::core->model_engine->load(
            "alice",
            ".../models/zenless_zone_zero/alice_thymefield/alice_thymefield_summer/model.pmx",
            std::make_unique< alice::c_alice_animator >( ) );

// render
alice::core->fonts->bind( );

core->graphics->set_streamproof( core->graphics->streamproof );

if ( core->graphics->streamproof && core->graphics->blur_effects )
    alice::core->blur->capture_desktop( );

if ( auto* alice_model = alice::core->model_engine ? alice::core->model_engine->get( "alice" ) : nullptr;
    alice_model && alice_model->loaded( ) )
{
    alice_model->render( 1024, 1024 );
    c_graphics::device_context->OMSetRenderTargets( 1, &c_graphics::render_target_view, nullptr );
}

alice::core->ui->render( );
```

## ✦ requirements

- c++20 or later
- dear imgui (v1.91.9)
- a gpu that was made after 2012

---

<div align="center">

**alicekit 2.0** — because imgui deserved better.

</div>
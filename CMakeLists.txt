add_library(alice_kit STATIC
        alice_kit.cpp
        alice_kit.hpp
        ui/ui.cpp
        ui/ui.hpp
        renderer/renderer.cpp
        renderer/renderer.hpp
        window_animator/window_animator.cpp
        window_animator/window_animator.hpp
        util/util.cpp
        util/util.hpp
        fonts/font_data_format.hpp
        fonts/font_loader.cpp
        fonts/font_loader.hpp
        fonts/fonts.cpp
        fonts/fonts.hpp
        themes/themes.cpp
        themes/themes.hpp
        themes/theme_editor.cpp
        themes/theme_editor.hpp
        ui/elem/elem.cpp
        ui/elem/elem.hpp
        standalone_elem/standalone_elem.cpp
        standalone_elem/standalone_elem.hpp
        elem_animations/elem_animations.hpp
        elem_animations/elem_animations.cpp
        esp_layout/esp_layout.hpp
        esp_layout/esp_layout.cpp
        popups/popups.cpp
        popups/popups.hpp
        popups/popup_elem/popup_elem.cpp
        popups/popup_elem/popup_elem.hpp
        smooth_scroll/smooth_scroll.cpp
        smooth_scroll/smooth_scroll.hpp
        custom_scrollbar/custom_scrollbar.cpp
        custom_scrollbar/custom_scrollbar.hpp
        assets/assets.cpp
        assets/assets.hpp
        assets/premade/logo.hpp
        assets/premade/banner.hpp
        blur/blur.hpp
        glow/glow.hpp
        grayscale/grayscale.hpp
        renderer_fx/renderer_fx.hpp
        backends/backend.hpp
        model_engine/model_engine.cpp
        model_engine/model_engine.hpp
        model_engine/types.cpp
        model_engine/types.hpp
        model_engine/skeleton.cpp
        model_engine/skeleton.hpp
        model_engine/animators/animator.hpp
        model_engine/animators/alice_animator.cpp
        model_engine/animators/alice_animator.hpp
        model_engine/animators/generic_animator.cpp
        model_engine/animators/generic_animator.hpp
        model_engine/physics/spring.cpp
        model_engine/physics/spring.hpp
        model_engine/formats/loader.cpp
        model_engine/formats/loader.hpp
        model_engine/formats/glb/glb.cpp
        model_engine/formats/glb/glb.hpp
        model_engine/formats/pmx/pmx.cpp
        model_engine/formats/pmx/pmx.hpp
        windows/windows.cpp
        windows/windows.hpp
        config_manager/config_manager.cpp
        config_manager/config_manager.hpp
        notifications/notifications.cpp
        notifications/notifications.hpp
        tooltips/tooltips.cpp
        tooltips/tooltips.hpp
        assets/premade/logo_alt.hpp
)

if(WIN32)
    target_sources(alice_kit PRIVATE
            backends/dx11/dx11_backend.cpp
            backends/dx11/dx11_backend.hpp
            backends/dx11/blur/blur.cpp
            backends/dx11/blur/blur.hpp
            backends/dx11/glow/glow.cpp
            backends/dx11/glow/glow.hpp
            backends/dx11/grayscale/grayscale.cpp
            backends/dx11/grayscale/grayscale.hpp
            backends/dx11/models/models.cpp
            backends/dx11/models/models.hpp
    )
else()
    target_sources(alice_kit PRIVATE
            backends/opengl/opengl_backend.cpp
            backends/opengl/opengl_backend.hpp
            backends/opengl/blur/blur.cpp
            backends/opengl/blur/blur.hpp
            backends/opengl/glow/glow.cpp
            backends/opengl/glow/glow.hpp
            backends/opengl/grayscale/grayscale.cpp
            backends/opengl/grayscale/grayscale.hpp
            backends/opengl/models/models.cpp
            backends/opengl/models/models.hpp
    )
endif()

target_include_directories(alice_kit
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/alice_kit
        ${CMAKE_SOURCE_DIR}/imgui
        ${CMAKE_SOURCE_DIR}/json
)

target_link_libraries(alice_kit
        PUBLIC
        imgui
)

if(WIN32)
    target_link_libraries(alice_kit PUBLIC d3dcompiler)
    target_compile_definitions(alice_kit PRIVATE NOMINMAX)
endif()

if(APPLE)
    target_link_libraries(alice_kit PUBLIC "-framework OpenGL")
endif()

target_compile_features(alice_kit PUBLIC cxx_std_23)

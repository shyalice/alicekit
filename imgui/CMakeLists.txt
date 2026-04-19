add_library(imgui STATIC
        imgui.cpp
        imgui_draw.cpp
        imgui_tables.cpp
        imgui_widgets.cpp
        imgui_demo.cpp
        #imgui_freetype.cpp
)

target_include_directories(imgui PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/alice_kit
        ${CMAKE_SOURCE_DIR}/freetype/include
)

add_library(imgui_backends STATIC
        backends/imgui_impl_win32.cpp
        backends/imgui_impl_dx11.cpp
)

target_include_directories(imgui_backends PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/backends
       # ${CMAKE_SOURCE_DIR}/freetype/include
)

target_link_libraries(imgui_backends PUBLIC
        imgui
)
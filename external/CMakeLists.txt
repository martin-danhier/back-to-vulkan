# === SDL2 ===

add_library(sdl2 INTERFACE)

find_path(sdl2_include_dir
        NAMES SDL.h
        PATHS $ENV{SDL2_PATH}/include/SDL2 /usr/include/SDL2 ${sdl2_image_DIR}/include/SDL2
        REQUIRED
        )
find_path(sdl2_lib_dir
        NAMES libSDL2.so libSDL2main.so SDL2.dll SDL2main.dll
        PATHS $ENV{SDL2_PATH}/lib /usr/lib/x86_64-linux-gnu/ ${sdl2_image_DIR}/lib
        REQUIRED)

target_include_directories(sdl2 INTERFACE ${sdl2_include_dir})
target_link_directories(sdl2 INTERFACE ${sdl2_lib_dir})

target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

# === glm ===

add_library(glm INTERFACE)
target_include_directories(glm INTERFACE glm)

# === tinyobjloader ===

add_library(tinyobjloader STATIC)
target_sources(tinyobjloader PRIVATE
        tinyobjloader/tiny_obj_loader.h
        tinyobjloader/tiny_obj_loader.cc
        )

target_include_directories(tinyobjloader PUBLIC tinyobjloader)

# === stb_image ===

add_library(stb_image INTERFACE)
target_include_directories(stb_image INTERFACE stb_image)

# === vk-bootstrap ===

add_library(vkbootstrap STATIC)
target_sources(vkbootstrap PRIVATE
        vk-bootstrap/src/VkBootstrap.h
        vk-bootstrap/src/VkBootstrap.cpp
        )

target_include_directories(vkbootstrap PUBLIC vk-bootstrap/src)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)

# === vk mem alloc ===

add_library(vma INTERFACE)
target_include_directories(vma INTERFACE vma)

# === volk ===

add_library(volk STATIC)
target_sources(volk PRIVATE
        volk/volk.c
        volk/volk.h
        )
target_include_directories(volk PUBLIC volk)
target_link_libraries(volk PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)


# === imgui ===

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE
        imgui/imgui.h
        imgui/imgui.cpp

        imgui/imgui_demo.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_widgets.cpp

        imgui/backends/imgui_impl_vulkan.cpp
        imgui/backends/imgui_impl_sdl.cpp
        )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)
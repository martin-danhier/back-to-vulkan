# === Vulkan ===

find_package(Vulkan REQUIRED)

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

# === imgui ===

#add_library(imgui STATIC)
#
#target_include_directories(imgui PUBLIC imgui)
#
#target_sources(imgui PRIVATE
#        imgui/imgui.h
#        imgui/imgui.cpp
#
#        imgui/imgui_demo.cpp
#        imgui/imgui_draw.cpp
#        imgui/imgui_widgets.cpp
#
#        imgui/imgui_impl_vulkan.cpp
#        imgui/imgui_impl_sdl.cpp
#        )
#
#target_link_libraries(imgui PUBLIC Vulkan::Vulkan sdl2)
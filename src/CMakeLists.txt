# Add sources
add_executable(the_good_one main.cpp)

# Add dependencies

# Link externals
set_property(TARGET the_good_one PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:the_good_one>")
target_include_directories(the_good_one PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(the_good_one
        glm
        tinyobjloader
        stb_image
        vkbootstrap
        vma
        volk
        imgui
        )
target_link_libraries(the_good_one
        Vulkan::Vulkan
        sdl2
        )

add_dependencies(the_good_one Shaders)
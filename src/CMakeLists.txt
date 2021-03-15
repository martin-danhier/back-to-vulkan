# Add sources
add_executable(the_good_one main.cpp)

# Add dependencies
target_include_directories(the_good_one PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# Link externals
target_link_libraries(the_good_one
        glm
        tinyobjloader
        stb_image
        vkbootstrap
        vma
        )
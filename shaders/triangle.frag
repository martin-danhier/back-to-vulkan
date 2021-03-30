#version 450

// output write
layout (location = 0) in vec3 inColor;
layout (location = 0) out vec4 outFragColor;

void main() {

    // Red
    outFragColor = vec4(1.0f, 0.0f, 0.0f, 1.0f);
}

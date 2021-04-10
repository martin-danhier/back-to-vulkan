#version 450

// output write
layout (location = 0) in vec3 inColor;
layout (location = 1) flat in uint inObjectIndex;
layout (location = 0) out vec4 outFragColor;

void main() {

    // Red
    outFragColor = vec4(inColor, 1.0f);
}

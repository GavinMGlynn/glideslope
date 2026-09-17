#version 450
// Push constants, which SDL_GPU does not have; it has uniform buffers.
layout(push_constant) uniform Constants { mat4 m; };
layout(location = 0) in vec3 position;
void main() { gl_Position = m * vec4(position, 1.0); }

#version 450
// A fragment shader's only texture at binding 1; SDL_GPU counts from 0.
layout(set = 2, binding = 1) uniform sampler2D image;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 colour;
void main() { colour = texture(image, uv); }

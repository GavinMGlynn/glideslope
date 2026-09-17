#version 450
// A texture and a sampler declared apart, which SDL_GPU's layout has no place for.
layout(set = 2, binding = 0) uniform texture2D image;
layout(set = 2, binding = 1) uniform sampler smoothing;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 colour;
void main() { colour = texture(sampler2D(image, smoothing), uv); }

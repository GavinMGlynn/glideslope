#version 450
// SDL_GPU's layout kept: two textures in set 2 from binding 0, a uniform buffer
// in set 3 at binding 0.
layout(set = 2, binding = 0) uniform sampler2D albedo;
layout(set = 2, binding = 1) uniform sampler2D detail;
layout(set = 3, binding = 0) uniform Tint { vec4 tint; };
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 colour;
void main() { colour = texture(albedo, uv) * texture(detail, uv * 8.0) * tint; }

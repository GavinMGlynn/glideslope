#version 450

// The vertex colour times the texture: a mesh with no texture of its own is
// drawn with a white one.

layout(location = 0) in vec4 v_colour;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 colour;

layout(set = 2, binding = 0) uniform sampler2D image;

void main() {
    colour = v_colour * texture(image, v_uv);
}

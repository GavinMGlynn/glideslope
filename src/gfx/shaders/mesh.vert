#version 450

// A mesh vertex, in coordinates relative to its own origin, to clip space.
// clip_from_local is worked out on the CPU in double precision relative to the
// camera - see gfx/scene.hpp. The texture coordinates are the mesh's, scaled
// and offset to the part of the texture the draw uses.

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 v_colour;
layout(location = 1) out vec2 v_uv;

layout(set = 1, binding = 0) uniform Transform {
    mat4 clip_from_local;
    vec4 uv_transform; // scale in xy, offset in zw
};

void main() {
    v_colour = colour;
    v_uv = uv * uv_transform.xy + uv_transform.zw;
    gl_Position = clip_from_local * vec4(position, 1.0);
}

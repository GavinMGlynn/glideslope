#version 450

// A mesh vertex, in coordinates relative to its own origin, to clip space.
// clip_from_local is worked out on the CPU in double precision relative to the
// camera - see gfx/scene.hpp.

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;

layout(location = 0) out vec4 v_colour;

layout(set = 1, binding = 0) uniform Transform {
    mat4 clip_from_local;
};

void main() {
    v_colour = colour;
    gl_Position = clip_from_local * vec4(position, 1.0);
}

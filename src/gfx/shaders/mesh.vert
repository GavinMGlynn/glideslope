#version 450

// A mesh vertex, in coordinates relative to its own origin, to clip space.
// clip_from_local is worked out on the CPU in double precision relative to the
// camera - see gfx/scene.hpp. The texture coordinates are the mesh's, scaled
// and offset to the part of the texture the draw uses. For the haze: the
// vertex's offset from the eye, and its height above the haze's station, from
// its place in the station's east-north-up frame, with the Earth's curvature.

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 v_colour;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_from_eye;
layout(location = 3) out float v_height;

layout(set = 1, binding = 0) uniform Transform {
    mat4 clip_from_local;
    vec4 uv_transform; // scale in xy, offset in zw
    vec4 eye;          // the camera, in the mesh's own coordinates
    mat4 station_from_local;
};

const float earth_radius = 6371000.0;

void main() {
    v_colour = colour;
    v_uv = uv * uv_transform.xy + uv_transform.zw;
    v_from_eye = position - eye.xyz;
    vec3 s = (station_from_local * vec4(position, 1.0)).xyz;
    v_height = s.z + dot(s.xy, s.xy) / (2.0 * earth_radius);
    gl_Position = clip_from_local * vec4(position, 1.0);
}

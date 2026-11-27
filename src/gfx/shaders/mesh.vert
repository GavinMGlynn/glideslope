#version 450

// A mesh vertex, in coordinates relative to its own origin, to clip space.
// clip_from_local is worked out on the CPU in double precision relative to the
// camera - see gfx/scene.hpp. The texture coordinates are the mesh's, scaled
// and offset to the part of the texture the draw uses. For the haze: the
// vertex's offset from the eye, and its height above the haze's station, from
// its place in the station's east-north-up frame, with the Earth's curvature.
//
// A mesh drawn with a sun is lit here: its colour times how much light its
// normal catches, as gfx::light_on says - 1 facing the sun as level ground
// does under it, down to 0.35 in shadow. The sun is in the mesh's own frame,
// where the normals are, so the light turns as the mesh's placement does and
// the mesh is never made again for it. Without one (light.w is 0) the colour
// is the vertex's: terrain's, whose light is already in it.

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 colour;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 normal;

layout(location = 0) out vec4 v_colour;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_from_eye;
layout(location = 3) out float v_height;

layout(set = 1, binding = 0) uniform Transform {
    mat4 clip_from_local;
    vec4 uv_transform; // scale in xy, offset in zw
    vec4 eye;          // the camera, in the mesh's own coordinates
    mat4 station_from_local;
    vec4 light;        // the sun in the mesh's own coordinates; w 1 to light
};

const float earth_radius = 6371000.0;

void main() {
    float lit = light.w > 0.5
                    ? 0.35 + 0.65 * max(0.0, dot(normal, light.xyz)) / sqrt(0.5)
                    : 1.0;
    v_colour = vec4(clamp(colour.rgb * lit, 0.0, 1.0), colour.a);
    v_uv = uv * uv_transform.xy + uv_transform.zw;
    v_from_eye = position - eye.xyz;
    vec3 s = (station_from_local * vec4(position, 1.0)).xyz;
    v_height = s.z + dot(s.xy, s.xy) / (2.0 * earth_radius);
    gl_Position = clip_from_local * vec4(position, 1.0);
}

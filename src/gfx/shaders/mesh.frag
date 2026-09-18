#version 450

// The vertex colour times the texture - a mesh with no texture of its own is
// drawn with a white one - seen through the haze.
//
// The haze is Koschmieder's law: what is seen through air of extinction s over
// a distance d keeps exp(-s d) of its contrast with the haze's colour. Below
// the haze's top the extinction is one, above it another, and along the
// straight path from the eye the share of it below the top is the share of the
// heights between them below it. Clear air has no extinction, and a colour
// passes through unchanged.

layout(location = 0) in vec4 v_colour;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_from_eye;
layout(location = 3) in float v_height;

layout(location = 0) out vec4 colour;

layout(set = 2, binding = 0) uniform sampler2D image;

layout(set = 3, binding = 0) uniform Haze {
    vec4 haze_colour;
    // Extinction per metre below the top and above it, the top's height and
    // the eye's, above the station.
    vec4 haze;
};

void main() {
    vec4 c = v_colour * texture(image, v_uv);
    float lo = min(haze.w, v_height);
    float hi = max(haze.w, v_height);
    float below = hi > lo ? clamp((haze.z - lo) / (hi - lo), 0.0, 1.0)
                          : (lo < haze.z ? 1.0 : 0.0);
    float seen = exp(-length(v_from_eye) * mix(haze.y, haze.x, below));
    colour = vec4(mix(haze_colour.rgb, c.rgb, seen), c.a);
}

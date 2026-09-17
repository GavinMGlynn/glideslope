#version 450
// A uniform buffer in set 0, where SDL_GPU wants a vertex shader's textures.
layout(set = 0, binding = 0) uniform Transform { mat4 m; };
layout(location = 0) in vec3 position;
void main() { gl_Position = m * vec4(position, 1.0); }

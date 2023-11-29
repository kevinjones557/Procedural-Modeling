#version 330

layout(location = 0) in vec3 pos;		// World-space position
layout(location = 1) in vec3 color;

uniform mat4 xform;			// World-to-clip transform matrix

out vec4 vertex_pos;
out vec3 o_color;

void main() {
	// Output clip-space position
	vertex_pos = (vec4(pos, 1.0));
	o_color = color;
	gl_Position = (xform * vec4(pos, 1.0));
}

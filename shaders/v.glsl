#version 330

layout(location = 0) in vec3 pos;		// World-space position

uniform mat4 xform;			// World-to-clip transform matrix

out vec4 vertex_pos;

void main() {
	// Output clip-space position
	vertex_pos = (vec4(pos, 1.0));
	gl_Position = (xform * vec4(pos, 1.0));
}

#version 330

out vec3 outCol;	// Final pixel color

in vec4 vertex_pos;
in vec3 o_color;

void main() {
    outCol = o_color;
    /*if (vertex_pos.z == 0) {
        outCol = vec3(0, 0, 0);
    }*/
}

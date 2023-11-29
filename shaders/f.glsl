#version 330

out vec3 outCol;	// Final pixel color

in vec4 vertex_pos;

void main() {
    outCol = vec3(0, 0.3, 0.2);
    /*if (vertex_pos.z == 0) {
        outCol = vec3(0, 0, 0);
    }*/
}

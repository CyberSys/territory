#version 130
in vec3 in_Position;
uniform mat4 dmvp;

void main(void) {
	// Set the position to the one defined in our vertex array
	gl_Position = dmvp * vec4(in_Position, 1.0f);
}

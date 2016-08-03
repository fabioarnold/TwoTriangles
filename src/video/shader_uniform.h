struct ShaderUniform {
	GLint location;
	GLchar name[64];
	GLenum type; // vec4, mat4, etc.
	GLint size; // number of elements of array e.g. 5 if declared as mat4 arr[5]
	u8 *data; // pointer into uniform data array

	int getSize(); // returns size in byte

	void gui();
	void apply();
};

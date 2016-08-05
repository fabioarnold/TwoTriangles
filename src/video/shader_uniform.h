enum ShaderUniformFlags {
	SUF_IS_COLOR = 1<<0 // only vec3 and vec4
};

struct ShaderUniform {
	GLint location;
	GLchar name[64];
	GLenum type; // vec4, mat4, etc.
	GLint size; // number of elements of array e.g. 5 if declared as mat4 arr[5]
	u8 *data; // pointer into uniform data array
	u32 flags; // ShaderUniformFlags

	size_t getTypeSize(); // size of single element in array
	size_t getSize(); // returns size in byte

	void gui();
	void apply();
};

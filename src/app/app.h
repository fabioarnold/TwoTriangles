struct MovementCommand {
	vec3 move;
	vec2 rotate;
};

struct TextureSlot {
	GLenum target = GL_TEXTURE_2D; // texture target: 1D 2D 3D or cube map
	GLuint texture = 0;
	int image_width, image_height;
	char *image_file_path = nullptr;

	void clear();
};

struct App {
	bool quit = false;
	VideoMode video;
	Camera camera;

	MovementCommand movement_command;

	char *file_path = nullptr;
	int file_mod_time = 0;
	bool file_autoreload = true;

	Shader shader;
	char *compile_error_log = nullptr;
	ShaderUniform *uniforms = nullptr;
	int uniform_count = 0;
	u8 *uniform_data = nullptr;
	size_t uniform_data_size;

	float u_time = 0.0f;
	bool anim_play = true;
	u64 frame_count = 0;

	TextureSlot texture_slots[8];

	GLuint two_triangles_vbo;
	
	bool show_uniforms_window = false;
	bool show_textures_window = false;

	void init();
	void update(float delta_time);

private:
	void parseUniforms();
	void readUniformData();
	void writeUniformData();
	void transferUniformData(ShaderUniform *old_uniforms, int old_uniform_count);
	
	void loadShader(const char *frag_file_path, bool initial);
	
	void openShaderDialog();
	void openImageDialog(TextureSlot *texture_slot, bool load_cube_cross=false);
};

struct MovementCommand {
	vec3 move;
	vec2 rotate;
};

struct TextureSlot {
	GLenum target = GL_TEXTURE_2D; // texture target: 1D 2D 3D or cube map
	GLuint texture = 0;
	int image_width, image_height;
	char *image_filepath = nullptr;

	void clear();
};

struct App {
	bool quit = false;
	bool hide_gui = false;
	VideoMode video;
	Camera camera;

	MovementCommand movement_command; // camera control

	void reloadShader();
	void recompileShader();

	char *preferences_filepath;
	char *session_filepath;
	void readPreferences();
	void writePreferences();
	void readSession();
	void writeSession();

	void newShader();
	void openShaderDialog();
	void saveShaderDialog();
	void saveShader();

	void toggleAnimation() {anim_play = !anim_play;}
	void toggleWindow(int window_index);

	void init();
	void update(float delta_time);

	void beforeQuit() {writeSession();} // will be called before application exits

private:
	char *shader_filepath = nullptr;
	int shader_file_mtime = 0;
	bool shader_file_autoreload = true;

	char *recently_used_filepaths[10] = {}; // cyclic, from most recent to less recent
	int most_recently_used_index = 0; // top of stack

	void clearRecentlyUsedFilepaths();
	void addMostRecentlyUsedFilepath(char *filepath);

	// As far as I understand ImGui::InputTextMultiline doesn't allow
	// for reallocating the text buffer during a callback.
	// That's why I go for a static size.
	char src_edit_buffer[64 << 10] = {0}; // 64 KiB

	Shader shader;
	char *compile_error_log = nullptr;
	ShaderUniform *uniforms = nullptr;
	int uniform_count = 0;
	u8 *uniform_data = nullptr;
	size_t uniform_data_size;

	void loadShader(const char *frag_file_path, bool reload=false);
	void compileShader(const char *shader_src, bool recompile=false);
	void parseUniforms();
	void readUniformData();
	void writeUniformData();
	void transferUniformData(ShaderUniform *old_uniforms, int old_uniform_count);

	float u_time = 0.0f;
	bool anim_play = true;
	u64 frame_count = 0;

	TextureSlot texture_slots[8];

	GLuint single_triangle_vbo;
	GLuint two_triangles_vbo;
	bool single_triangle_mode = true;
	
	void openImageDialog(TextureSlot *texture_slot, bool load_cube_cross=false);

	bool show_uniforms_window = false;
	bool show_textures_window = false;
	bool show_src_edit_window = false;

	void gui();
};

void TextureSlot::clear() {
	if (texture) glDeleteTextures(1, &texture);
	if (image_filepath) free(image_filepath);
	texture = 0;
	image_filepath = nullptr;
}

void App::parseUniforms() {
	// clean up
	if (uniforms)     {delete [] uniforms;     uniforms     = nullptr;}
	if (uniform_data) {delete [] uniform_data; uniform_data = nullptr;}

	// get uniform count
	glGetProgramiv(shader.getProgram(), GL_ACTIVE_UNIFORMS, &uniform_count);
	if (uniform_count == 0) return; // shader has no uniforms

	uniforms = new ShaderUniform[uniform_count];

	// 1st pass: parse uniforms and calculate data size
	uniform_data_size = 0;
	for (int uniform_index = 0; uniform_index < uniform_count; uniform_index++) {
		ShaderUniform *uniform = uniforms+uniform_index;
		GLsizei uniform_name_len;
		glGetActiveUniform(shader.getProgram(), uniform_index, (GLsizei)sizeof(uniform->name),
			&uniform_name_len, &uniform->size, &uniform->type, uniform->name);
		uniform->location = shader.getUniformLocation(uniform->name);
		uniform_data_size += uniform->getSize();

		if (strstr(uniform->name, "color")) uniform->flags |= SUF_IS_COLOR;
	}
	uniform_data = new u8[uniform_data_size];
	memset(uniform_data, 0, uniform_data_size);
	// 2nd pass: store offsets into uniform_data for each uniform
	int offset = 0;
	for (int uniform_index = 0; uniform_index < uniform_count; uniform_index++) {
		ShaderUniform *uniform = uniforms+uniform_index;
		uniform->data = uniform_data+offset;
		offset += uniform->getSize();
	}
}

// bad O(n^2) proc. but ok since in most cases there are not that many uniforms
void App::transferUniformData(ShaderUniform *old_uniforms, int old_uniform_count) {
	for (int oui = 0; oui < old_uniform_count; oui++) {
		ShaderUniform *old_uniform = old_uniforms+oui;
		for (int ui = 0; ui < uniform_count; ui++) {
			ShaderUniform *uniform = uniforms+ui;
			if (!strcmp(uniform->name, old_uniform->name)
				&& uniform->type == old_uniform->type
				&& uniform->size == old_uniform->size) {
				memcpy(uniform->data, old_uniform->data, uniform->getSize());
			}
		}
	}
}

static const char *uniformdata_ext = ".uniformdata";
static const char *uniformdata_fourcc = "UDAT";
static const u32 uniformdata_version = 1;
void App::readUniformData() {
	if (!shader_filepath) return;
	size_t uniform_filepath_len = strlen(shader_filepath)+strlen(uniformdata_ext);
	char *uniform_filepath = new char[uniform_filepath_len+1];
	strcpy(uniform_filepath, shader_filepath);
	strcat(uniform_filepath, uniformdata_ext);

	FILE *file = fopen(uniform_filepath, "rb");
	delete [] uniform_filepath;
	if (!file) return;

	u32 fourcc;  fread(&fourcc,  sizeof(u32), 1, file);
	u32 version; fread(&version, sizeof(u32), 1, file);
	if (fourcc != *((u32*)uniformdata_fourcc) || version != uniformdata_version) {
		LOGE("Not a valid uniformdata file: %s %d %d", uniform_filepath, *((u32*)uniformdata_fourcc), fourcc);
		fclose(file);
		return;
	}

	int loaded_uniform_count;
	ShaderUniform *loaded_uniforms;
	size_t loaded_uniform_data_size;
	u8 *loaded_uniform_data;

	fread(&loaded_uniform_count, sizeof(int), 1, file);
	fread(&loaded_uniform_data_size, sizeof(size_t), 1, file);
	assert(loaded_uniform_count > 0 && loaded_uniform_data_size > 0);
	loaded_uniforms = new ShaderUniform[loaded_uniform_count];
	loaded_uniform_data = new u8[loaded_uniform_data_size];
	fread(loaded_uniforms, sizeof(ShaderUniform), loaded_uniform_count, file);
	fread(loaded_uniform_data, sizeof(u8), loaded_uniform_data_size, file);
	fclose(file);

	// convert offsets to pointers
	for (int lui = 0; lui < loaded_uniform_count; lui++) {
		loaded_uniforms[lui].data = (u8*)((intptr_t)loaded_uniform_data
			+ (intptr_t)loaded_uniforms[lui].data);
	}

	transferUniformData(loaded_uniforms, loaded_uniform_count);

	delete [] loaded_uniforms;
	delete [] loaded_uniform_data;
}

void App::writeUniformData() {
	if (!shader_filepath) return;
	size_t uniform_filepath_len = strlen(shader_filepath)+strlen(uniformdata_ext);
	char *uniform_filepath = new char[uniform_filepath_len+1];
	strcpy(uniform_filepath, shader_filepath);
	strcat(uniform_filepath, uniformdata_ext);

	FILE *file = fopen(uniform_filepath, "wb");
	delete [] uniform_filepath;
	if (!file) return;

	// convert pointers into offsets
	for (int ui = 0; ui < uniform_count; ui++) {
		uniforms[ui].data = (u8*)((intptr_t)uniforms[ui].data - (intptr_t)uniform_data);
	}

	fwrite(uniformdata_fourcc, sizeof(char), 4, file);
	fwrite(&uniformdata_version, sizeof(u32), 1, file);
	fwrite(&uniform_count, sizeof(int), 1, file);
	fwrite(&uniform_data_size, sizeof(size_t), 1, file);
	fwrite(uniforms, sizeof(ShaderUniform), uniform_count, file);
	fwrite(uniform_data, sizeof(u8), uniform_data_size, file);
	fclose(file);

	// restore pointers
	for (int ui = 0; ui < uniform_count; ui++) {
		uniforms[ui].data = (u8*)((intptr_t)uniform_data + (intptr_t)uniforms[ui].data);
	}
}

// displays fullscreen red flash animation
static const char *error_frag_src =
	"uniform float u_time, u_error_time;"
	"void main() {"
	"	float alpha = pow(2.0, -10.0*(u_time - u_error_time));"
	"	vec3 red = vec3(0.9, 0.1, 0.08);"
	"	gl_FragColor = vec4(red, alpha);"
	"}";

void App::reloadShader() {
	loadShader(shader_filepath, /*reload*/true);
}

void App::loadShader(const char *frag_shader_filepath, bool reload) {
	// clean up
	if (compile_error_log) {
		delete [] compile_error_log;
		compile_error_log = nullptr;
	}

	char *shader_src = readStringFromFile(frag_shader_filepath);
	if (!shader_src) return; // couldn't read from file TODO: feedback
	// copy src into editor buffer
	strncpy(src_edit_buffer, shader_src, sizeof(src_edit_buffer));
	// zero terminate just in case so we don't overflow in writeStringToFile
	src_edit_buffer[sizeof(src_edit_buffer)-1] = '\0';

	// compile newly loaded shader
	bool is_compiled = shader.compileAndAttach(GL_FRAGMENT_SHADER, shader_src);
	delete [] shader_src;

	if (!is_compiled) {
		compile_error_log = shader.getShaderCompileErrorLog(GL_FRAGMENT_SHADER);
		assert(compile_error_log); // should be logically eq. to !is_compiled

		// use error shader
		shader.compileAndAttach(GL_FRAGMENT_SHADER, error_frag_src);
		shader.link();
		shader.use();
		glUniform1f(shader.getUniformLocation("u_error_time"), u_time);
		return; // keep old uniforms
	}
	shader.link();

	if (reload) {
		ShaderUniform *old_uniforms = uniforms; uniforms = nullptr;
		u8 *old_uniform_data = uniform_data; uniform_data = nullptr;
		int old_uniform_count = uniform_count;

		parseUniforms();

		if (old_uniform_count) {
			transferUniformData(old_uniforms, old_uniform_count);
			assert(old_uniforms); delete [] old_uniforms;
			assert(old_uniform_data); delete [] old_uniform_data;
		}
	} else {
		// store filepath
		if (shader_filepath) delete [] shader_filepath;
		shader_filepath = new char[strlen(frag_shader_filepath)+1];
		strcpy(shader_filepath, frag_shader_filepath);

		parseUniforms();
		// load uniform values from disk
		readUniformData();

		// update most recently used filepaths
		char *found = nullptr; // check if path is already in list from least recent to most recent
		for (int i = ARRAY_COUNT(recently_used_filepaths)-1; i >= 0; i--) {
			int index = (most_recently_used_index+i) % ARRAY_COUNT(recently_used_filepaths);
			if (found) { // shift entries down to overwrite found entry
				int prev = (index+1) % ARRAY_COUNT(recently_used_filepaths);
				recently_used_filepaths[prev] = recently_used_filepaths[index];
			} else if (recently_used_filepaths[index] && !strcmp(recently_used_filepaths[index], shader_filepath)) {
				found = recently_used_filepaths[index];
			}
		}
		if (found) {
			recently_used_filepaths[most_recently_used_index] = found;
		} else {
			most_recently_used_index = (ARRAY_COUNT(recently_used_filepaths)+most_recently_used_index-1) % ARRAY_COUNT(recently_used_filepaths);
			if (recently_used_filepaths[most_recently_used_index]) {
				delete [] recently_used_filepaths[most_recently_used_index];
			}
			recently_used_filepaths[most_recently_used_index] = new char[strlen(shader_filepath)+1];
			strcpy(recently_used_filepaths[most_recently_used_index], shader_filepath);
		}
	}
}

void App::clearRecentlyUsedFilepaths() {
	for (int i = 0; i < ARRAY_COUNT(recently_used_filepaths); i++) {
		if (recently_used_filepaths[i]) {
			delete [] recently_used_filepaths[i];
			recently_used_filepaths[i] = nullptr;
		}
	}
}

enum IniVarType {
	INI_VAR_BOOL,
	INI_VAR_INT,
	INI_VAR_FLOAT,
	INI_VAR_STRING
};

struct IniVar {
	const char *name;
	IniVarType type;
	//int array_size;
	void *pointer;

	void setValue(char *str_value) {
		switch (type) {
			case INI_VAR_BOOL: case INI_VAR_INT: *(int*)pointer = atoi(str_value); break;
			case INI_VAR_FLOAT: *(float*)pointer = atof(str_value); break;
			case INI_VAR_STRING: {
				char **str = (char**)pointer;
				if (*str) delete [] *str;
				*str = new char[strlen(str_value)+1];
				strcpy(*str, str_value);
				break;
			}
			default: assert(!"invalid var type");
		}
	}
};

void parseIniString(char *ini_str, IniVar *vars, size_t var_count) {
	char *key = ini_str;
	char *value = nullptr;
	bool parse_value = false; // else parse key
	for (char *c = ini_str; *c; c++) {
		if (*c == '\n') {
			if (parse_value) { // we have seen an equal sign in this line
				*c = '\0'; // terminate value string
				for (int vi = 0; vi < var_count; vi++) {
					if (!strcmp(vars[vi].name, key)) {
						vars[vi].setValue(value);
						break;
					}
				}
			}
			parse_value = false;
			key = c+1;
		} else if (*c == '=') {
			*c = '\0'; // terminate name string
			parse_value = true;
			value = c+1;
		}
	}
}

void App::readPreferences() {
	if (!preferences_filepath) return;
	char *preferences_str = readStringFromFile(preferences_filepath);
	if (!preferences_str) return;

	IniVar preferences_vars[] = {
		{"shader_file_autoreload", INI_VAR_BOOL, &shader_file_autoreload},
		{"single_triangle_mode", INI_VAR_BOOL, &single_triangle_mode}
	};
	parseIniString(preferences_str, preferences_vars, ARRAY_COUNT(preferences_vars));

	delete [] preferences_str;
}

void App::writePreferences() {
	if (!preferences_filepath) return;
	FILE *file = fopen(preferences_filepath, "w");
	if (!file) {
		LOGW("Could not write '%s'.", preferences_filepath);
		return;
	}

	fprintf(file, "shader_file_autoreload=%d\n", shader_file_autoreload);
	fprintf(file, "single_triangle_mode=%d\n", single_triangle_mode);

	fclose(file);
}

void App::readSession() {
	if (!session_filepath) return;
	char *session_str = readStringFromFile(session_filepath);
	if (!session_str) return;

	char *recently_used_str = nullptr; // "filepath0","filepath1","filepath2"
	IniVar session_vars[] = {
		{"recently_used", INI_VAR_STRING, &recently_used_str},
		{"video_width", INI_VAR_INT, &video.width},
		{"video_height", INI_VAR_INT, &video.height},
	};
	parseIniString(session_str, session_vars, ARRAY_COUNT(session_vars));

	if (recently_used_str) {
		// free old stuff
		clearRecentlyUsedFilepaths();

		int recently_used_count = 0;
		char *filepath = nullptr;
		for (char *c = recently_used_str; *c; c++) {
			if (*c == '"') { // TODO: what about quotes in path
				if (!filepath) filepath = c+1;
				else {
					size_t filepath_len = c-filepath;
					if (filepath_len > 0) {
						recently_used_filepaths[recently_used_count] = new char[filepath_len+1];
						strncpy(recently_used_filepaths[recently_used_count], filepath, filepath_len);
						recently_used_filepaths[recently_used_count][filepath_len] = '\0';
						recently_used_count++;
						if (recently_used_count == ARRAY_COUNT(recently_used_filepaths)) break;
					} // else ignore empty string
					filepath = nullptr;
				}
			}
		}

		delete [] recently_used_str;
	}

	delete [] session_str;
}

void App::writeSession() {
	if (!session_filepath) return;
	FILE *file = fopen(session_filepath, "w");
	if (!file) {
		LOGW("Could not write '%s'.", session_filepath);
		return;
	}

	fprintf(file, "recently_used=");
	for (int i = 0; i < ARRAY_COUNT(recently_used_filepaths); i++) {
		int index = (most_recently_used_index+i) % ARRAY_COUNT(recently_used_filepaths);
		if (recently_used_filepaths[index]) {
			fprintf(file, "\"%s\",", recently_used_filepaths[index]);
		} else break;
	}
	fprintf(file, "\n");
	fprintf(file, "video_width=%d\n", video.width);
	fprintf(file, "video_height=%d\n", video.height);

	fclose(file);
}

void App::openShaderDialog() {
	char *out_filepath = nullptr;
	nfdresult_t result = NFD_OpenDialog("frag,glsl,fsh,txt", nullptr, &out_filepath);
	SDL_RaiseWindow(sdl_window); // workaround: focus window again after dialog closes
	
	if (result == NFD_OKAY) {
		struct stat attr;
		if (!stat(out_filepath, &attr)) { // file exists
			shader_file_mtime = attr.st_mtime;
			loadShader(out_filepath);
		}
		free(out_filepath);
	}
}

void App::saveShaderDialog() {
	char *out_filepath = nullptr;
	nfdresult_t result = NFD_SaveDialog("frag,glsl,fsh,txt", nullptr, &out_filepath);
	SDL_RaiseWindow(sdl_window); // workaround: focus window again after dialog closes

	if (result == NFD_OKAY) {
		if (shader_filepath) free(shader_filepath);
		shader_filepath = out_filepath;

		writeStringToFile(shader_filepath, src_edit_buffer);
		struct stat attr;
		if (!stat(shader_filepath, &attr)) { // file exists
			shader_file_mtime = 0; // force autoreload
		}
	}
}

void App::openImageDialog(TextureSlot *texture_slot, bool load_cube_cross) {
	char *out_filepath = nullptr;
	nfdresult_t result = NFD_OpenDialog("tga,png,bmp,jpg,hdr", nullptr, &out_filepath);
	SDL_RaiseWindow(sdl_window); // workaround: focus window again after dialog closes

	if (result == NFD_OKAY) {
		if (load_cube_cross) {
			int out_size;
			GLuint loaded_texture_cube = loadTextureCubeCross(out_filepath,
				/*build_mipmaps*/true, &out_size);
			if (loaded_texture_cube) {
				texture_slot->clear();

				texture_slot->target = GL_TEXTURE_CUBE_MAP;
				texture_slot->texture = loaded_texture_cube;
				texture_slot->image_width = out_size;
				texture_slot->image_height = out_size;
				texture_slot->image_filepath = out_filepath;
			}
		} else {
			int out_width, out_height;
			GLuint loaded_texture = loadTexture2D(out_filepath,
				/*build_mipmaps*/true, &out_width, &out_height);
			if (loaded_texture) { // loading successful
				texture_slot->clear();

				texture_slot->target = GL_TEXTURE_2D;
				setWrapTexture2D(GL_REPEAT, GL_REPEAT);
				texture_slot->texture = loaded_texture;
				texture_slot->image_width = out_width;
				texture_slot->image_height = out_height;
				texture_slot->image_filepath = out_filepath;
			}
		}
	}
}

void App::init() {
	quit = false;

	camera.location = v3(0.0f, -4.0f, 2.0f);
	camera.euler_angles.x = 0.1f * M_PI;

	static vec2 positions[4] = {
		{{-1.0f, -1.0f}},
		{{ 1.0f, -1.0f}},
		{{-1.0f,  1.0f}},
		{{ 1.0f,  1.0f}}
	};
	glGenBuffers(1, &two_triangles_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, two_triangles_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	const char *vert_src =
		"attribute vec4 va_position;"
		"void main() {gl_Position = va_position;}";
	shader.compileAndAttach(GL_VERTEX_SHADER, vert_src);
	const char *frag_src =
		"void main() {gl_FragColor = vec4(0.0);}";
	shader.compileAndAttach(GL_FRAGMENT_SHADER, frag_src);
	shader.bindVertexAttrib("va_position", VAT_POSITION);
	shader.link();

	// set imgui style
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 3.0f;
	style.FrameRounding = 3.0f;
	style.GrabRounding = 3.0f;
	style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.83f, 0.90f, 0.80f, 0.40f);
	style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.70f, 0.90f, 0.65f, 0.45f);
	style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.27f, 0.44f, 0.54f, 0.83f);
	style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.40f, 0.70f, 0.80f, 0.20f);
	style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.32f, 0.51f, 0.63f, 0.87f);
	style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.40f, 0.47f, 0.55f, 0.80f);
	style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.40f, 0.64f, 0.80f, 0.30f);
	style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.40f, 0.63f, 0.80f, 0.40f);
	style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.58f, 0.80f, 0.50f, 0.40f);
	style.Colors[ImGuiCol_SliderGrab]            = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
	style.Colors[ImGuiCol_Button]                = ImVec4(0.44f, 0.67f, 0.40f, 0.60f);
	style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.44f, 0.67f, 0.40f, 1.00f);
	style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.55f, 0.80f, 0.50f, 1.00f);
	style.Colors[ImGuiCol_Header]                = ImVec4(0.34f, 0.72f, 0.77f, 0.45f);
	style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.35f, 0.62f, 0.69f, 0.80f);
	style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.47f, 0.68f, 0.78f, 0.80f);
	style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.62f, 0.70f, 0.60f, 1.00f);
	style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.76f, 0.90f, 0.70f, 1.00f);
	style.Colors[ImGuiCol_CloseButton]           = ImVec4(0.50f, 0.81f, 0.90f, 0.50f);
	style.Colors[ImGuiCol_CloseButtonHovered]    = ImVec4(0.70f, 0.86f, 0.90f, 0.60f);
	style.Colors[ImGuiCol_CloseButtonActive]     = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
#if 0
	ImGuiIO& io = ImGui::GetIO();
	float size_pixels = 15.0f;
	ImFontConfig config;
	config.OversampleH = 2;
	config.OversampleV = 2;
	io.Fonts->AddFontFromFileTTF("lib/imgui/extra_fonts/Cousine-Regular.ttf", size_pixels, &config);
#endif
}

// builtin uniform names
static char u_time_name[64]       = "u_time";
static char u_resolution_name[64] = "u_resolution";
static char u_view_mat_name[64]   = "u_view_mat";

void App::gui() {
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Open...", io.OSXBehaviors ? "Cmd+O" : "Ctrl+O")) {
				openShaderDialog();
			}
			if (ImGui::BeginMenu("Open Recent")) {
				for (int i = 0; i < ARRAY_COUNT(recently_used_filepaths); i++) {
					int index = (most_recently_used_index+i) % ARRAY_COUNT(recently_used_filepaths);
					if (recently_used_filepaths[index]) {
						char *menuitem_label = recently_used_filepaths[index];
						// we might include libgen.h and use basename(3) but there is no windows support...
						// TODO: test this on windows
						char *basename = strrchr(menuitem_label, '/');
						if (basename) menuitem_label = basename+1;
						if (ImGui::MenuItem(menuitem_label)) {
							loadShader(recently_used_filepaths[index]);
						}
						if (basename && ImGui::IsItemHovered()) {
							ImGui::SetTooltip("%s", recently_used_filepaths[index]);
						}
					} else break;
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Clear Items")) {
					clearRecentlyUsedFilepaths();
				}
				ImGui::EndMenu();
			}
			// TODO: move this to settings pane eventually
			if (ImGui::MenuItem("Autoreload", nullptr, shader_file_autoreload)) {
				shader_file_autoreload = !shader_file_autoreload;
				writePreferences();
			}
			if (ImGui::MenuItem("Save", io.OSXBehaviors ? "Cmd+S" : "Ctrl+S", false, !!shader_filepath)) {
				writeStringToFile(shader_filepath, src_edit_buffer);
			}
			if (ImGui::IsItemHovered() && shader_filepath) {
				ImGui::SetTooltip("%s", shader_filepath);
			}
			if (ImGui::MenuItem("Save As...", io.OSXBehaviors ? "Cmd+Shift+S" : "Ctrl+Shift+S", false, !!src_edit_buffer[0])) {
				saveShaderDialog();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Quit", "Esc")) {
				quit = true;
			}
 			ImGui::EndMenu();
		}
		// TODO: recently used files
		if (ImGui::BeginMenu("Animation")) {
			if (ImGui::MenuItem(anim_play ? "Pause" : "Play")) {
				anim_play = !anim_play;
			}
			if (ImGui::MenuItem("Reset")) {
				frame_count = 0;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Window")) {
			if (ImGui::MenuItem("Uniforms", nullptr, show_uniforms_window)) {
				show_uniforms_window = !show_uniforms_window;
			}
			if (ImGui::MenuItem("Textures", nullptr, show_textures_window)) {
				show_textures_window = !show_textures_window;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Source editor", nullptr, show_src_edit_window)) {
				show_src_edit_window = !show_src_edit_window;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (show_uniforms_window) {
		if (ImGui::Begin("Uniforms", &show_uniforms_window)) {
			if (ImGui::CollapsingHeader("Built-in uniform names")) {
				ImGui::InputText("Time", u_time_name, sizeof(u_time_name));
				ImGui::InputText("Resolution", u_resolution_name, sizeof(u_resolution_name));
				ImGui::InputText("View Matrix", u_view_mat_name, sizeof(u_view_mat_name));
			}
			ImGui::Separator();
	
			if (!compile_error_log) {
				ImGui::AlignFirstTextHeightToWidgets();
				ImGui::Text("Data"); ImGui::SameLine();
				if (ImGui::Button("Clear")) {
					if (uniform_data) {
						memset(uniform_data, 0, uniform_data_size);
					}
				} ImGui::SameLine();
				if (ImGui::Button("Save")) {
					writeUniformData();
				} ImGui::SameLine();
				if (ImGui::Button("Load")) {
					readUniformData();
				}
				for (int i = 0; i < uniform_count; i++) {
					// skip builtin uniforms
					if (!strcmp(u_time_name,       uniforms[i].name)) continue;
					if (!strcmp(u_resolution_name, uniforms[i].name)) continue;
					if (!strcmp(u_view_mat_name,   uniforms[i].name)) continue;
					uniforms[i].gui();
				}
			}
		}
		ImGui::End();
	}

	if (show_textures_window) {
		if (ImGui::Begin("Textures", &show_textures_window, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Columns(2);
			for (int tsi = 0; tsi < ARRAY_COUNT(texture_slots); tsi++) {
				TextureSlot *texture_slot = texture_slots+tsi;
				
				ImGui::BeginGroup();
				ImGui::PushID(tsi);
				ImGui::Text("%d:", tsi);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, ImColor::HSV(0.0f, 0.6f, 0.6f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImColor::HSV(0.0f, 0.7f, 0.7f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImColor::HSV(0.0f, 0.8f, 0.8f));
				if (ImGui::SmallButton("x")) texture_slot->clear();
				ImGui::PopStyleColor(3);
				if (ImGui::Button(" 2D ")) openImageDialog(texture_slot);
				if (ImGui::Button("Cube")) openImageDialog(texture_slot, /*load_cube_cross*/true);
				ImGui::PopID();
				ImGui::EndGroup();

				ImGui::SameLine();
				//ImTextureID im_tex_id = (ImTextureID)(intptr_t)texture_slot->texture;
				ImGui::Image((void*)texture_slot, ImVec2(64, 64));
				if (ImGui::IsItemHovered() && texture_slot->image_filepath) {
					ImGui::SetTooltip("%s\n%dx%d", texture_slot->image_filepath,
						texture_slot->image_width, texture_slot->image_height);
				}

				if ((tsi&1) && tsi+1 != ARRAY_COUNT(texture_slots)) {
					ImGui::Separator();
				} else {
					ImGui::SameLine(); ImGui::Spacing();
				}
				ImGui::NextColumn();
			}
			ImGui::Columns(1);
		}
		ImGui::End();
	}

	if (show_src_edit_window) {
		if (ImGui::Begin("Source editor", &show_src_edit_window)) {
			// TODO: add horizontal scrollbar
			ImGui::InputTextMultiline("##Text buffer", src_edit_buffer, sizeof(src_edit_buffer)-1,
				/*fullwidth, fullheight*/ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_AllowTabInput);
		}
		ImGui::End();
	}

	// overlay messages
	int overlay_flags = ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoSavedSettings;
	if (!shader_filepath) {
		ImGui::SetNextWindowPosCenter();
		ImGui::Begin("Overlay", nullptr, ImVec2(0, 0), 0.3f, overlay_flags);
		ImGui::AlignFirstTextHeightToWidgets(); // valign text to button
		ImGui::Text("No fragment shader");
		ImGui::SameLine();
		if (ImGui::Button("Open")) {
			openShaderDialog();
		}
		ImGui::End();
	} else if (compile_error_log) {
		ImGui::SetNextWindowPosCenter();
		ImGui::Begin("Overlay", nullptr, ImVec2(0, 0), 0.3f, overlay_flags);
		ImGui::TextUnformatted(compile_error_log);
		ImGui::End();
	}
}

struct BindShader {
	BindShader(Shader &shader) {shader.use();}
	~BindShader() {glUseProgram(0);}
};

struct BindArrayBuffer {
	BindArrayBuffer(GLuint buffer) {glBindBuffer(GL_ARRAY_BUFFER, buffer);}
	~BindArrayBuffer() {glBindBuffer(GL_ARRAY_BUFFER, 0);}
};

void App::update(float delta_time) {
	if (anim_play) frame_count++;

	if (!hide_gui) gui();

	// autoreload frag shader (every 60 frames)
	if (shader_filepath && shader_file_autoreload && (frame_count % 60) == 0) {
		struct stat attr;
		if (!stat(shader_filepath, &attr)) { // file exists
			if (attr.st_mtime > shader_file_mtime) { // file has been modified
				shader_file_mtime = attr.st_mtime;
				reloadShader();
			}
		}
	}

	// update camera
	camera.euler_angles += 2.0f*delta_time
		* v3(movement_command.rotate.x, movement_command.rotate.y, 0.0f);
	mat3 rot_y = rotationMatrix(v3(0.0f, 0.0f, 1.0f), -camera.euler_angles.y);
	camera.location += rot_y * (8.0f*delta_time*movement_command.move);
	camera.updateViewMatrix();

	// bind textures
	for (int tsi = 0; tsi < ARRAY_COUNT(texture_slots); tsi++) {
		glActiveTexture(GL_TEXTURE0+tsi);
		glBindTexture(texture_slots[tsi].target, texture_slots[tsi].texture);
	}

	// draw two triangles
	glClearColor(0.2, 0.21, 0.22, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);
	{ BindShader bind_shader(shader);
		if (!compile_error_log) {
			for (int i = 0; i < uniform_count; i++) {
				uniforms[i].apply();
			}
		}

		// apply builtin uniforms
		u_time = (float)frame_count / 60.0f;
		glUniform1f(shader.getUniformLocation(u_time_name), u_time);
		vec2 u_resolution = v2(video.pixel_scale*video.width, video.pixel_scale*video.height);
		glUniform2fv(shader.getUniformLocation(u_resolution_name), 1, u_resolution.e);
		mat4 u_inv_view_mat = camera.makeInverseViewMatrix();
		int u_view_mat_loc = shader.getUniformLocation(u_view_mat_name);
		glUniformMatrix4fv(u_view_mat_loc, 1, GL_FALSE, u_inv_view_mat.e);

		{ BindArrayBuffer bind_array_buffer(two_triangles_vbo);
			glEnableVertexAttribArray(VAT_POSITION);
			glVertexAttribPointer(VAT_POSITION, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)0);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glDisableVertexAttribArray(VAT_POSITION);
		}
	}
}

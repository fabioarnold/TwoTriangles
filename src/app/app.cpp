void TextureSlot::clear() {
	if (texture) glDeleteTextures(1, &texture);
	if (image_file_path) free(image_file_path);
	texture = 0;
	image_file_path = nullptr;
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

// bad O(n^2) proc. but ok since in most cases there are less than 64 uniforms
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
	if (!file_path) return;
	size_t uniform_file_path_len = strlen(file_path)+strlen(uniformdata_ext);
	char *uniform_file_path = new char[uniform_file_path_len+1];
	strcpy(uniform_file_path, file_path);
	strcat(uniform_file_path, uniformdata_ext);

	FILE *file = fopen(uniform_file_path, "rb");
	delete [] uniform_file_path;
	if (!file) return;

	u32 fourcc;  fread(&fourcc,  sizeof(u32), 1, file);
	u32 version; fread(&version, sizeof(u32), 1, file);
	if (fourcc != *((u32*)uniformdata_fourcc) || version != uniformdata_version) {
		LOGE("Not a valid uniformdata file: %s %d %d", uniform_file_path, *((u32*)uniformdata_fourcc), fourcc);
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
	if (!file_path) return;
	size_t uniform_file_path_len = strlen(file_path)+strlen(uniformdata_ext);
	char *uniform_file_path = new char[uniform_file_path_len+1];
	strcpy(uniform_file_path, file_path);
	strcat(uniform_file_path, uniformdata_ext);

	FILE *file = fopen(uniform_file_path, "wb");
	delete [] uniform_file_path;
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

void App::loadShader(const char *frag_file_path, bool initial) {
	// clean up
	if (compile_error_log) {
		delete [] compile_error_log;
		compile_error_log = nullptr;
	}

	char *shader_src = readStringFromFile(frag_file_path);
	if (!shader_src) return; // couldn't read from file
	// copy src into editor buffer
	strncpy(src_edit_buffer, shader_src, sizeof(src_edit_buffer));

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

	if (initial) {
		parseUniforms();
		// load uniform values from disk
		readUniformData();
	} else {
		ShaderUniform *old_uniforms = uniforms; uniforms = nullptr;
		u8 *old_uniform_data = uniform_data; uniform_data = nullptr;
		int old_uniform_count = uniform_count;

		parseUniforms();

		if (old_uniform_count) {
			transferUniformData(old_uniforms, old_uniform_count);
			assert(old_uniforms); delete [] old_uniforms;
			assert(old_uniform_data); delete [] old_uniform_data;
		}
	}
}

void App::openShaderDialog() {
	char *out_file_path = nullptr;
	nfdresult_t result = NFD_OpenDialog("frag,glsl,fsh,txt", nullptr, &out_file_path);
	SDL_RaiseWindow(sdl_window); // workaround: focus window again after dialog closes
	
	if (result == NFD_OKAY) {
		if (file_path) free(file_path);
		file_path = out_file_path;

		struct stat attr;
		if (!stat(file_path, &attr)) { // file exists
			file_mod_time = attr.st_mtime;
			loadShader(file_path, /*initial*/true);
		}
	}
}

void App::saveShaderDialog() {
	char *out_file_path = nullptr;
	nfdresult_t result = NFD_SaveDialog("frag,glsl,fsh,txt", nullptr, &out_file_path);
	SDL_RaiseWindow(sdl_window); // workaround: focus window again after dialog closes

	if (result == NFD_OKAY) {
		if (file_path) free(file_path);
		file_path = out_file_path;

		writeStringToFile(file_path, src_edit_buffer);
		struct stat attr;
		if (!stat(file_path, &attr)) { // file exists
			file_mod_time = 0; // force autoreload
		}
	}
}

void App::openImageDialog(TextureSlot *texture_slot, bool load_cube_cross) {
	char *out_file_path = nullptr;
	nfdresult_t result = NFD_OpenDialog("tga,png,bmp,jpg,hdr", nullptr, &out_file_path);
	SDL_RaiseWindow(sdl_window); // workaround: focus window again after dialog closes

	if (result == NFD_OKAY) {
		if (load_cube_cross) {
			int out_size;
			GLuint loaded_texture_cube = loadTextureCubeCross(out_file_path,
				/*build_mipmaps*/true, &out_size);
			if (loaded_texture_cube) {
				texture_slot->clear();

				texture_slot->target = GL_TEXTURE_CUBE_MAP;
				texture_slot->texture = loaded_texture_cube;
				texture_slot->image_width = out_size;
				texture_slot->image_height = out_size;
				texture_slot->image_file_path = out_file_path;
			}
		} else {
			int out_width, out_height;
			GLuint loaded_texture = loadTexture2D(out_file_path,
				/*build_mipmaps*/true, &out_width, &out_height);
			if (loaded_texture) { // loading successful
				texture_slot->clear();

				texture_slot->target = GL_TEXTURE_2D;
				setWrapTexture2D(GL_REPEAT, GL_REPEAT);
				texture_slot->texture = loaded_texture;
				texture_slot->image_width = out_width;
				texture_slot->image_height = out_height;
				texture_slot->image_file_path = out_file_path;
			}
		}
	}
}

void App::init() {
	quit = false;

	camera.location = v3(0.0f, -4.0f, 2.0f);
	camera.euler_angles.x = 0.1f * M_PI;

	static vec2 positions[4] = {
		{-1.0f, -1.0f},
		{1.0f, -1.0f},
		{-1.0f, 1.0f},
		{1.0f, 1.0f}
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
	ImGuiIO& io = ImGui::GetIO();
#if 0
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
			if (ImGui::MenuItem("Open fragment shader", io.OSXBehaviors ? "Cmd+O" : "Ctrl+O")) {
				openShaderDialog();
			}
			if (ImGui::MenuItem("Save fragment shader", io.OSXBehaviors ? "Cmd+S" : "Ctrl+S", false, !!file_path)) {
				writeStringToFile(file_path, src_edit_buffer);
			}
			if (ImGui::IsItemHovered() && file_path) {
				ImGui::SetTooltip("%s", file_path);
			}
			if (ImGui::MenuItem("Save fragment shader as...", io.OSXBehaviors ? "Cmd+Shift+S" : "Ctrl+Shift+S", false, !!src_edit_buffer[0])) {
				saveShaderDialog();
			}
			// TODO: move this to settings eventually
			if (ImGui::MenuItem("Autoreload", nullptr, file_autoreload)) {
				file_autoreload = !file_autoreload;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Quit", "Esc")) {
				quit = true;
			}
 			ImGui::EndMenu();
		}
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
				if (ImGui::IsItemHovered() && texture_slot->image_file_path) {
					ImGui::SetTooltip("%s\n%dx%d", texture_slot->image_file_path,
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
			ImGui::InputTextMultiline("##Text buffer", src_edit_buffer, sizeof(src_edit_buffer),
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
	if (!file_path) {
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
	if (file_path && file_autoreload && (frame_count % 60) == 0) {
		struct stat attr;
		if (!stat(file_path, &attr)) { // file exists
			if (attr.st_mtime > file_mod_time) { // file has been modified
				file_mod_time = attr.st_mtime;
				loadShader(file_path, /*initial*/false);
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

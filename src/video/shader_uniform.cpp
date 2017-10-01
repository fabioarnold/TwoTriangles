size_t ShaderUniform::getTypeSize() {
	switch (type) {
		case GL_FLOAT:      case GL_INT:      return  4;
		case GL_FLOAT_VEC2: case GL_INT_VEC2: return  8;
		case GL_FLOAT_VEC3: case GL_INT_VEC3: return 12;
		case GL_FLOAT_VEC4: case GL_INT_VEC4: return 16;
		//GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4
		case GL_FLOAT_MAT2: return 16;
		case GL_FLOAT_MAT3: return 36;
		case GL_FLOAT_MAT4: return 64;
		case GL_SAMPLER_2D: case GL_SAMPLER_CUBE: return 4;
		default: LOGE("ShaderUniform: unknown type 0x%X", type); return 0;
	}
}

size_t ShaderUniform::getSize() {
	return size*getTypeSize();
}

void ShaderUniform::gui() {
	char name_buf[64];

	ImGui::PushID(location);

	ImGui::BeginGroup();
	for (int i = 0; i < size; i++) {
		ImGui::PushID(i);
		u8 *datai = data + i*getTypeSize();
		switch (type) {
			case GL_FLOAT:      ImGui::DragFloat (name, (float*)datai); break;
			case GL_FLOAT_VEC2: ImGui::DragFloat2(name, (float*)datai); break;
			case GL_FLOAT_VEC3:
				if (flags&SUF_IS_COLOR) ImGui::ColorEdit3(name, (float*)datai);
				else                    ImGui::DragFloat3(name, (float*)datai);
				break;
			case GL_FLOAT_VEC4:
				if (flags&SUF_IS_COLOR) ImGui::ColorEdit4(name, (float*)datai);
				else                    ImGui::DragFloat4(name, (float*)datai);
				break;
			case GL_INT:        ImGui::DragInt   (name, (int  *)datai); break;
			case GL_INT_VEC2:   ImGui::DragInt2  (name, (int  *)datai); break;
			case GL_INT_VEC3:   ImGui::DragInt3  (name, (int  *)datai); break;
			case GL_INT_VEC4:   ImGui::DragInt4  (name, (int  *)datai); break;
			//GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4
			case GL_FLOAT_MAT2: {
				sprintf(name_buf, "%s[0]", name);
				ImGui::DragFloat2(name_buf, (float*)(datai));
				sprintf(name_buf, "%s[1]", name);
				ImGui::DragFloat2(name_buf, (float*)(datai+8));
			} break;
			case GL_FLOAT_MAT3: {
				sprintf(name_buf, "%s[0]", name);
				ImGui::DragFloat3(name_buf, (float*)(datai));
				sprintf(name_buf, "%s[1]", name);
				ImGui::DragFloat3(name_buf, (float*)(datai+12));
				sprintf(name_buf, "%s[2]", name);
				ImGui::DragFloat3(name_buf, (float*)(datai+24));
			} break;
			case GL_FLOAT_MAT4: {
				sprintf(name_buf, "%s[0]", name);
				ImGui::DragFloat4(name_buf, (float*)(datai));
				sprintf(name_buf, "%s[1]", name);
				ImGui::DragFloat4(name_buf, (float*)(datai+16));
				sprintf(name_buf, "%s[2]", name);
				ImGui::DragFloat4(name_buf, (float*)(datai+32));
				sprintf(name_buf, "%s[3]", name);
				ImGui::DragFloat4(name_buf, (float*)(datai+48));
			} break;
			case GL_SAMPLER_2D: case GL_SAMPLER_CUBE: ImGui::InputInt(name_buf, (int*)(datai)); break;
			default: assert(!"ShaderUniform: unhandled type");
		}
		ImGui::PopID();
	}
	ImGui::EndGroup();

	if (type == GL_FLOAT_VEC3 || type == GL_FLOAT_VEC4) {
		if (ImGui::BeginPopupContextItem("flags")) {
			ImGui::CheckboxFlags("is color", &flags, SUF_IS_COLOR);
			ImGui::EndPopup();
		}
	}

	ImGui::PopID();
}

void ShaderUniform::apply() {
	switch (type) {
		case GL_FLOAT:      glUniform1fv(location, size, (float*)data); break;
		case GL_FLOAT_VEC2: glUniform2fv(location, size, (float*)data); break;
		case GL_FLOAT_VEC3: glUniform3fv(location, size, (float*)data); break;
		case GL_FLOAT_VEC4: glUniform4fv(location, size, (float*)data); break;
		case GL_INT:        glUniform1iv(location, size, (int  *)data); break;
		case GL_INT_VEC2:   glUniform2iv(location, size, (int  *)data); break;
		case GL_INT_VEC3:   glUniform3iv(location, size, (int  *)data); break;
		case GL_INT_VEC4:   glUniform4iv(location, size, (int  *)data); break;
		//GL_BOOL, GL_BOOL_VEC2, GL_BOOL_VEC3, GL_BOOL_VEC4
		case GL_FLOAT_MAT2: glUniformMatrix2fv(location, size, GL_FALSE, (float*)data); break;
		case GL_FLOAT_MAT3: glUniformMatrix3fv(location, size, GL_FALSE, (float*)data); break;
		case GL_FLOAT_MAT4: glUniformMatrix4fv(location, size, GL_FALSE, (float*)data); break;
		case GL_SAMPLER_2D: case GL_SAMPLER_CUBE: glUniform1iv(location, size, (int *)data); break;
		default: assert(!"ShaderUniform: unhandled type");
	}
}

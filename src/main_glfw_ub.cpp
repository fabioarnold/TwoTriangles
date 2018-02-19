/*
Unity build file using GLFW
 */



#include <assert.h>
#include <float.h> // for FLT_MAX
#include <math.h>
#include <stdio.h> // for printf
#include <stdlib.h> // for atoi
#include <time.h> // used by log

#include <sys/stat.h> // fstat

#ifdef __APPLE__
	#include <OpenGL/gl.h>
#else
	#include <GL/glew.h>
	#include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>

#define ORG_NAME "FabioWare"
#define APP_NAME "TwoTriangles"
#define WINDOW_TITLE "Two Triangles"
GLFWwindow* glfw_window;

bool windowIsFullscreen();
void windowToggleFullscreen();

// ImGui
#include <imgui.h>
#include "imgui_impl_glfw_gl2.h" // the impl we want to use

// nativfiledialog
#include <nfd.h>

// stb_image
#include <stb_image.h>



#include "system/defines.h"
#include "system/log.h"
#include "system/files.h"
#include "system/frametime.h"

#include "math/vector_math.h"
#include "math/transform.h"

#include "video/shader.h"
#include "video/image.h"
#include "video/texture.h"
#include "video/model_mdl.h"
#include "video/font_bitmap.h"
#include "video/video_mode.h"

#include "video/shader_uniform.h"
#include "app/app.h"



#include "system/log.cpp"
#include "system/files.cpp"

#include "math/transform.cpp"

#include "video/shader.cpp"
#include "video/image.cpp"
#include "video/texture.cpp"
//#include "video/texture_dds.cpp"
//#include "video/model_mdl.cpp"
#include "video/font_bitmap.cpp"
//#include "video/renderer.cpp"

#include "video/shader_uniform.cpp"
#include "app/app.cpp"



App *app;

void errorCallback(int error, const char* description) {
	LOGE("Error: %s", description);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	ImGuiIO& io = ImGui::GetIO();

	if (!io.WantCaptureKeyboard) {
		if (key == GLFW_KEY_ESCAPE) {
			app->toggleAnimation();
		}
	}

	// handle hotkeys
	bool ctrl_key_down = io.OptMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
	bool shift_key_down = io.KeyShift;
	if (ctrl_key_down) {
		switch (key) {
			case GLFW_KEY_1: case GLFW_KEY_2: case GLFW_KEY_3: case GLFW_KEY_4:
				app->toggleWindow(key-GLFW_KEY_1);
				break;
			case GLFW_KEY_B: // build / compile
				app->recompileShader();
				break;
			case GLFW_KEY_F: // fullscreen
				windowToggleFullscreen();
				break;
			case GLFW_KEY_H: // hide gui
				if (!io.OptMacOSXBehaviors || shift_key_down) app->hide_gui = !app->hide_gui; // toggle imgui
				break;
			case GLFW_KEY_N: // new
				app->newShader();
				break;
			case GLFW_KEY_O: // open
				app->openShaderDialog();
				break;
			case GLFW_KEY_Q: // quit
				app->quit = true;
				break;
			case GLFW_KEY_S: // save
				if (shift_key_down) app->saveShaderDialog();
				else app->saveShader();
				break;
			default: break;
		}
	}
	if (key == GLFW_KEY_F11) {
		windowToggleFullscreen();
	}
	
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void loop(float delta_time);
void windowSizeCallback(GLFWwindow* window, int width, int height) {
	loop(0.0f);
}

void windowToggleFullscreen() {
	int window_width, window_height;
	glfwGetWindowSize(glfw_window, &window_width, &window_height);
	if (!windowIsFullscreen()) {
		glfwSetWindowMonitor(glfw_window,
			glfwGetPrimaryMonitor(),
			GLFW_DONT_CARE, GLFW_DONT_CARE,
			window_width, window_height,
			GLFW_DONT_CARE);
	} else {
		glfwSetWindowMonitor(glfw_window,
			nullptr,
			GLFW_DONT_CARE, GLFW_DONT_CARE,
			window_width, window_height,
			GLFW_DONT_CARE);
	}
}

bool windowIsFullscreen() {
	return glfwGetWindowMonitor(glfw_window) != nullptr;
}

void init(VideoMode *video) {
	glfwSetErrorCallback(errorCallback);
	if (!glfwInit()) exit(EXIT_FAILURE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfw_window = glfwCreateWindow(video->width, video->height, WINDOW_TITLE, NULL, NULL);
	if (!glfw_window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwSetKeyCallback(glfw_window, keyCallback);
	glfwSetWindowSizeCallback(glfw_window, windowSizeCallback);
	glfwMakeContextCurrent(glfw_window);
	//gladLoadGLLoader((GLADloadproc) glfwGetProcAddress); // glew
	glfwSwapInterval(1);
}

void destroy() {
	glfwDestroyWindow(glfw_window);
	glfwTerminate();
}

void loop(float delta_time) {
	ImGui_ImplGlfwGL2_NewFrame();

	int width, height;
	glfwGetFramebufferSize(glfw_window, &width, &height);
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT);

	app->update(delta_time);

	ImGui::Render();
	ImGui_ImplGlfwGL2_RenderDrawData(ImGui::GetDrawData());

	glfwSwapBuffers(glfw_window);
}

int main(void) {
	app = new App();
	app->video.width = 1024;
	app->video.height = 640;
	app->video.pixel_scale = 1.0f;

	// set preferences_filepath and read preferences
	char *pref_path = strdup("./"); //getPrefPath(ORG_NAME, APP_NAME);

	size_t preferences_ini_str_len = strlen(pref_path)+strlen("preferences.ini")+1;
	app->preferences_filepath = new char[preferences_ini_str_len];
	strcpy(app->preferences_filepath, pref_path);
	strcat(app->preferences_filepath, "preferences.ini");
	size_t session_ini_str_len = strlen(pref_path)+strlen("session.ini")+1;
	app->session_filepath = new char[session_ini_str_len];
	strcpy(app->session_filepath, pref_path);
	strcat(app->session_filepath, "session.ini");
	size_t imgui_ini_str_len = strlen(pref_path)+strlen("imgui.ini")+1;
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	char *imgui_ini_filepath = new char[imgui_ini_str_len];
	strcpy(imgui_ini_filepath, pref_path);
	strcat(imgui_ini_filepath, "imgui.ini");
	io.IniFilename = imgui_ini_filepath;

	free(pref_path);
	app->readPreferences();
	app->readSession();

	init(&app->video);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	ImGui_ImplGlfwGL2_Init(glfw_window, /*install_callbacks*/true);
	ImGui::StyleColorsDark();

	app->init();

	double glfw_time_old = glfwGetTime();
	while (!glfwWindowShouldClose(glfw_window) && !app->quit)
	{
		glfwPollEvents();
		double glfw_time = glfwGetTime();
		loop(glfw_time - glfw_time_old);
		glfw_time_old = glfw_time;
	}

	destroy();
	exit(EXIT_SUCCESS);
}

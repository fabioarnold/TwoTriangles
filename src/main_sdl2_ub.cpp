/*
Unity build file using SDL2
 */



#include <assert.h>
#include <time.h> // used by log
#include <float.h> // for FLT_MAX
#include <stdio.h> // for printf
#include <stdlib.h> // for atoi

#include <sys/stat.h> // fstat

#include <SDL.h>
#ifndef __APPLE__
	#include <GL/glew.h>
#endif
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#define COMPANY_NAME "FabioWare"
#define APPLICATION_NAME "TwoTriangles"
#define WINDOW_TITLE "Two Triangles"
SDL_Window *sdl_window;
SDL_GLContext sdl_gl_context;

bool windowIsFullscreen();
void windowToggleFullscreen();

// ImGui
#include <imgui.h>
#include "imgui_impl_sdl2_gl2.h" // the impl we want to use

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
#include "video/camera.h"
//#include "video/renderer.h"

#include "video/shader_uniform.h"
#include "app/app.h"



#include "system/log.cpp"
#include "system/files.cpp"
#include "system/frametime_sdl2.cpp"

#include "math/transform.cpp"

#include "video/shader.cpp"
#include "video/image.cpp"
#include "video/texture.cpp"
//#include "video/texture_dds.cpp"
//#include "video/model_mdl.cpp"
#include "video/font_bitmap.cpp"
#include "video/camera.cpp"
//#include "video/renderer.cpp"

#include "video/shader_uniform.cpp"
#include "app/app.cpp"



/* inits sdl and creates an opengl window */
static void initSDL(VideoMode *video) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
		LOGE("Failed to init SDL2: %s", SDL_GetError());
		exit(1);
	}

#ifdef _WIN32
	float ddpi;
	if (!SDL_GetDisplayDPI(0, &ddpi, nullptr, nullptr)) {
		const float WINDOWS_DEFAULT_DPI = 96.0f;
		video->pixel_scale = ddpi / WINDOWS_DEFAULT_DPI;
	}
#endif

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

#ifdef EMSCRIPTEN
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
						SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
						SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif

	int window_flags =
		  SDL_WINDOW_SHOWN
		| SDL_WINDOW_RESIZABLE
		| SDL_WINDOW_OPENGL
		| SDL_WINDOW_ALLOW_HIGHDPI;

	sdl_window = SDL_CreateWindow(WINDOW_TITLE,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		video->pixel_scale * video->width, video->pixel_scale * video->height,
		window_flags);
	if (!sdl_window) {
		LOGE("Failed to create an OpenGL window: %s", SDL_GetError());
		exit(1);
	}

	sdl_gl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_gl_context) {
		LOGE("Failed to create an OpenGL context: %s", SDL_GetError());
		exit(1);
	}

	int drawable_width, drawable_height;
	SDL_GL_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
	if (drawable_width != video->width) {
		LOGI("Created high DPI window. (%dx%d)", drawable_width, drawable_height);
		video->pixel_scale = (float)drawable_width / (float)video->width;
	} else {
		video->pixel_scale = 1.0f;
	}

	if (SDL_GL_SetSwapInterval(1) == -1) { // sync with monitor refresh rate
		LOGW("Could not enable VSync.");
	}

	#ifndef __APPLE__
	glewInit();
	#endif
}

void quitSDL() {
	SDL_DestroyWindow(sdl_window);
	SDL_GL_DeleteContext(sdl_gl_context);
	SDL_Quit();
}



void FrameTime::drawInfo() {
	char fps_text[64];
	sprintf(fps_text, "FPS: %d\nframe time: %.3f ms", frames_per_second, 1000.0*smoothed_frame_time);
	bitmapfont.drawText(fps_text, 8, 8);
}

App *app = nullptr;

bool windowIsFullscreen() {
	return !!(SDL_GetWindowFlags(sdl_window)&SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void windowToggleFullscreen() {
	SDL_SetWindowFullscreen(sdl_window,
		windowIsFullscreen() ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

u64 mouse_timer = 0;

void mainLoop() {
	ImGuiIO& io = ImGui::GetIO();
	SDL_Event sdl_event;
	while (SDL_PollEvent(&sdl_event)) {
		ImGui_ImplSdlGL2_ProcessEvent(&sdl_event);
		switch (sdl_event.type) {
			case SDL_WINDOWEVENT:
				switch (sdl_event.window.event) {
		        	case SDL_WINDOWEVENT_SIZE_CHANGED: {
						app->video.width  = sdl_event.window.data1;
						app->video.height = sdl_event.window.data2;
						// update opengl viewport
						int drawable_width, drawable_height;
						SDL_GL_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
						glViewport(0, 0, drawable_width, drawable_height);
						break;
					}
				}
				break;
			case SDL_MOUSEMOTION:
				mouse_timer = 0;
				break;
			case SDL_QUIT:
				app->quit = true;
				break;
			case SDL_KEYDOWN: {
				if (!io.WantCaptureKeyboard) {
					if (sdl_event.key.keysym.sym == SDLK_SPACE) {
						app->toggleAnimation();
					}
				}

				// handle hotkeys
				bool ctrl_key_down = io.OSXBehaviors ? io.KeySuper : io.KeyCtrl;
				bool shift_key_down = io.KeyShift;
				if (ctrl_key_down) {
					switch (sdl_event.key.keysym.sym) {
						case SDLK_1: case SDLK_2: case SDLK_3:
							app->toggleWindow(sdl_event.key.keysym.sym-SDLK_1);
							break;
						case SDLK_b: // build / compile
							app->recompileShader();
							break;
						case SDLK_f: // fullscreen
							windowToggleFullscreen();
							break;
						case SDLK_h: // hide gui
							if (!io.OSXBehaviors || shift_key_down) app->hide_gui = !app->hide_gui; // toggle imgui
							break;
						case SDLK_o: // open
							app->openShaderDialog();
							break;
						case SDLK_q: // quit
							app->quit = true;
							break;
						case SDLK_s: // save
							if (shift_key_down) app->saveShaderDialog();
							else app->saveShader();
							break;
						default: break;
					}
				}
				if (sdl_event.key.keysym.sym == SDLK_F11) {
					windowToggleFullscreen();
				}
				break;
			}
		}
	}

	// TODO: make use of mouse_timer
	// doesn't seam to work on OS X?
	SDL_ShowCursor(app->hide_gui ? SDL_DISABLE : SDL_ENABLE);

	// TODO: better keyboard handling
	// update input commands
	// TODO: move this to some function
	// this also depends on the game state
	// when do we want a movement command?
	const Uint8 *key_state = SDL_GetKeyboardState(NULL);
	app->movement_command.move = v3(
		- (key_state[SDL_SCANCODE_A] == SDL_PRESSED ? 1.0f : 0.0f)
		+ (key_state[SDL_SCANCODE_D] == SDL_PRESSED ? 1.0f : 0.0f),
		- (key_state[SDL_SCANCODE_S] == SDL_PRESSED ? 1.0f : 0.0f)
		+ (key_state[SDL_SCANCODE_W] == SDL_PRESSED ? 1.0f : 0.0f),
		- (key_state[SDL_SCANCODE_Q] == SDL_PRESSED ? 1.0f : 0.0f)
		+ (key_state[SDL_SCANCODE_E] == SDL_PRESSED ? 1.0f : 0.0f));
	app->movement_command.rotate = v2(
		- (key_state[SDL_SCANCODE_UP   ] == SDL_PRESSED ? 1.0f : 0.0f)
		+ (key_state[SDL_SCANCODE_DOWN ] == SDL_PRESSED ? 1.0f : 0.0f),
		- (key_state[SDL_SCANCODE_LEFT ] == SDL_PRESSED ? 1.0f : 0.0f)
		+ (key_state[SDL_SCANCODE_RIGHT] == SDL_PRESSED ? 1.0f : 0.0f));

	if (!app->hide_gui && io.WantCaptureKeyboard) { // null movement
		app->movement_command.move = v3(0.0f);
		app->movement_command.rotate = v2(0.0f);
	}

	ImGui_ImplSdlGL2_NewFrame();

	app->update((float)frametime.smoothed_frame_time);

	ImGui::Render();

	SDL_GL_SwapWindow(sdl_window);
	frametime.update();
}

int main(int argc, char *argv[]) {
	app = new App();
	app->video.width = 1024;
	app->video.height = 640;
	app->video.pixel_scale = 1.0f;

	// set preferences_filepath and read preferences
	char *pref_path = SDL_GetPrefPath(COMPANY_NAME, APPLICATION_NAME);

	size_t preferences_ini_str_len = strlen(pref_path)+strlen("preferences.ini")+1;
	app->preferences_filepath = new char[preferences_ini_str_len];
	strcpy(app->preferences_filepath, pref_path);
	strcat(app->preferences_filepath, "preferences.ini");
	size_t session_ini_str_len = strlen(pref_path)+strlen("session.ini")+1;
	app->session_filepath = new char[session_ini_str_len];
	strcpy(app->session_filepath, pref_path);
	strcat(app->session_filepath, "session.ini");
	size_t imgui_ini_str_len = strlen(pref_path)+strlen("imgui.ini")+1;
	ImGuiIO& io = ImGui::GetIO();
	char *imgui_ini_filepath = new char[imgui_ini_str_len];
	strcpy(imgui_ini_filepath, pref_path);
	strcat(imgui_ini_filepath, "imgui.ini");
	io.IniFilename = imgui_ini_filepath;

	SDL_free(pref_path);
	app->readPreferences();
	app->readSession();

	initSDL(&app->video);

	ImGui_ImplSdlGL2_Init(sdl_window);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	app->init();

	// init this last for sake of last_ticks
	frametime.init();

	do {
		Uint32 beginTicks = SDL_GetTicks();
		mainLoop();
		// hack for when vsync isn't working
		Uint32 elapsedTicks = SDL_GetTicks() - beginTicks;
		if (elapsedTicks < 16) {
			SDL_Delay(16 - elapsedTicks);
		}
	} while(!app->quit);

	app->beforeQuit();

	ImGui_ImplSdlGL2_Shutdown();
	quitSDL();

	return 0;
}

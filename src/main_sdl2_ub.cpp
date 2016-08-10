/*
Unity build file using SDL2
 */



#include <cassert>
#include <ctime> // used by log
#include <cfloat> // for FLT_MAX
#include <cstdio> // for printf

#include <sys/stat.h> // fstat

#include <SDL.h>
#ifndef __APPLE__
	#include <GL/glew.h>
#endif
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#define WINDOW_TITLE "Two Triangles"
SDL_Window *sdl_window;
SDL_GLContext sdl_gl_context;

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
		//| SDL_WINDOW_ALLOW_HIGHDPI
		;

	sdl_window = SDL_CreateWindow(WINDOW_TITLE,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		video->width, video->height, window_flags);
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

void mainLoop() {
	SDL_Event sdl_event;
	while (SDL_PollEvent(&sdl_event)) {
		ImGui_ImplSdlGL2_ProcessEvent(&sdl_event);
		switch (sdl_event.type) {
		case SDL_WINDOWEVENT:
			switch (sdl_event.window.event) {
        	case SDL_WINDOWEVENT_SIZE_CHANGED:
				app->video.width  = sdl_event.window.data1;
				app->video.height = sdl_event.window.data2;
				{ // update opengl viewport
					int drawable_width, drawable_height;
					SDL_GL_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
					glViewport(0, 0, drawable_width, drawable_height);
				}
				break;
			}
			break;
		case SDL_QUIT:
			app->quit = true;
			break;
		case SDL_KEYDOWN:
			if (app->hide_gui || !ImGui::GetIO().WantCaptureKeyboard) {
				app->quit = app->quit || sdl_event.key.keysym.sym == SDLK_ESCAPE;
				if (sdl_event.key.keysym.sym == SDLK_h) {
					// toggle imgui
					app->hide_gui = !app->hide_gui;
				}
			}
			if (sdl_event.key.keysym.sym == SDLK_F11) {
				// toggle fullscreen
				u32 fullscreen_flag = SDL_GetWindowFlags(sdl_window)&SDL_WINDOW_FULLSCREEN_DESKTOP;
				SDL_SetWindowFullscreen(sdl_window,
					fullscreen_flag ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
			}
			break;
		}
	}

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

	if (!app->hide_gui && ImGui::GetIO().WantCaptureKeyboard) { // null movement
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

	ImGui_ImplSdlGL2_Shutdown();
	quitSDL();

	return 0;
}

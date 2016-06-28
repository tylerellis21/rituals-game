/* 
Copyright (c) 2016 William Bundy

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * rituals_win32.cpp
 * main file for windows platform
 *
 */ 

/* TODO(will) features
 *	- Improve physics system
 *		- Fixing the broadphase?
 *		- Real friction?
 *	- UI controls
 *		- Mostly artwork.
 *	- Main Menu
 *		- done, but not needed yet, really
 *  - Sound effects, music
 *  - Make a new palette, do some art
 *  - Game_Registry for all the <thing>_info types
 *  	- also to catalogue event handlers
 *  - Inventory management
 *  - Item entities
 *  - Different types of world
 *  - Game loop (die, respawn)
 *  - Rituals! They need to be in the game at some point.
 *
 *
 * - Thank you for watching the stream
 *
 * TODO(will) logical fixes
 *  - current/last time/accumulator need to belong to simulation
 */



//platform imports
#include <windows.h>

//CRT imports
#define _CRT_SECURE_NO_WARNINGS 1
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <stddef.h>
#include <errno.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>

//3rd party imports
#include <SDL.h>
#include "thirdparty.h"

//Some defines
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

//TODO(will): use this as all floating point sizes
typedef real32 real;

typedef ptrdiff_t isize;
typedef size_t usize;

#define Math_Sqrt2    1.414213f
#define Math_InvSqrt2 0.7071067f
#define Math_Pi       3.141592f
#define Math_Tau      6.283185f
#define Math_Rad2Deg  57.29577f
#define Math_Deg2Rad  0.01745329f

#define Allocate(type, count) ((type*)calloc(sizeof(type), count))
#define StackAlloc(type, count) ((type*)_alloca(sizeof(type) * count))

#define isizeof(t) ((isize)sizeof(t))
#define isz(t) (isizeof(t))

#define MixerNumberOfChannels (64)

#define FilePathMaxLength (4096)

#define PathSeparator ("\\")
#define PathSeparatorChar ('\\')

#define Min(x,y) ((x<y)?x:y)
#define Max(x,y) ((x>y)?x:y)


#define Kilobytes(b) (b * UINT64_C(1024))
#define Megabytes(b) (Kilobytes(b) * UINT64_C(1024))
#define Gigabytes(b) (Megabytes(b) * UINT64_C(1024))

//local imports
#include "rituals_math.cpp"
#include "rituals_game.cpp"
typedef struct World World;
struct Play_State
{
	usize current_time = 0, prev_time = 0;
	real accumulator = 0;
	World* world;

	Vec2i world_xy;
};
Play_State* play_state;

#include "rituals_renderer.cpp"
#include "rituals_gui.cpp"

#include "rituals_game_info.cpp"
#include "rituals_game_registry.cpp"

#include "rituals_inventory.cpp"

#include "rituals_tilemap.cpp"

#include "rituals_simulation.cpp"

#include "rituals_world_area.cpp"
#include "rituals_world.cpp"

#include "rituals_play_state.cpp"

void main_menu_update()
{
	game_set_scale(2.0f);
	renderer_start();

	Sprite s;
	init_sprite(&s);
	s.size = v2(15 * 16, 4 * 16);
	s.position = v2(16 + s.size.x / 2, Game->size.y / 2 - s.size.y / 2);
	s.texture = Get_Texture_Coordinates(
			Renderer->texture_width - s.size.x,
			2 * 16, 
			s.size.x, s.size.y);
	renderer_push_sprite(&s);

	if(gui_add_button(v2(40 + 16, s.position.y + s.size.y / 2 + 16), "Start")) {
		Game->state = Game_State_Play;
	}
	renderer_draw();
}

Item_Info* item_types;
isize item_types_count;
Inventory inventory;

void load_test_assets()
{
	
	init_inventory(&inventory, 9, 6, Game->play_arena);
	Item_Stack* stack = new_item_stack(item_types + 1, Game->play_arena);
	printf("%0x \n", (usize)stack);
	inventory_add_item(&inventory, &stack);
	printf("%0x \n", (usize)stack);
}

void test_update()
{
	game_set_scale(2.0f);
	renderer_start();
	render_inventory(&inventory, v2(16, 16));
	renderer_draw();
}

void update()
{
	switch(Game->state) {
		case Game_State_None:
#if DEBUG
			test_update();
#endif
			break;
		case Game_State_Menu:
			main_menu_update();
			break;
		case Game_State_Play:
			play_state_update();
			break;
		default:
			break;
	}
}

void load_assets()
{
	isize w, h;
	Renderer->texture = ogl_load_texture("data/graphics.png", &w, &h);
	Renderer->texture_width = w;
	Renderer->texture_height = h;

	Game->body_font = load_spritefont("data/gohufont-14.glyphs", 
			v2i(2048 - 1142, 0));
	Body_Font = Game->body_font;

	init_game_registry(Registry, Game->registry_arena); 
	register_everything_in_rituals();
	finalize_game_registry();

	Game->state = Game_State_Play;
	play_state_init();
	play_state_start();
#if DEBUG
	//load_test_assets();
	//Game->state = Game_State_None;
#endif
}


void update_screen()
{
	SDL_GetWindowSize(Game->window, &Game->window_size.x, &Game->window_size.y);
	glViewport(0, 0, Game->window_size.x, Game->window_size.y);
	Game->size = v2(Game->window_size) * Game->scale;
}

int main(int argc, char** argv)
{
	//stbi_set_flip_vertically_on_load(1);

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		Log_Error("Could not init SDL"); 
		Log_Error(SDL_GetError());
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	

	int32 window_display_index = 0;
#if 1
	window_display_index = 1;
#endif
	SDL_Window* window = SDL_CreateWindow("Rituals", 
			SDL_WINDOWPOS_CENTERED_DISPLAY(window_display_index), 
			SDL_WINDOWPOS_CENTERED_DISPLAY(window_display_index),
			1280, 720, 
			SDL_WINDOW_OPENGL | 
			SDL_WINDOW_RESIZABLE |
			SDL_WINDOW_MOUSE_FOCUS |
			SDL_WINDOW_INPUT_FOCUS);

	if(window == NULL) {
		Log_Error("Could not create window");
		Log_Error(SDL_GetError());
		return 1;
	}

	printf("%s \n", SDL_GetError());
	SDL_GLContext glctx = SDL_GL_CreateContext(window);

	if(ogl_LoadFunctions() == ogl_LOAD_FAILED) {
		Log_Error("Could not load OpenGL 3.3 functions...");
		return 1;
	}

	int ret = SDL_GL_SetSwapInterval(-1);

	{
#define _check_gl_attribute(attr, val) int _##attr##_val; \
	int _##attr##_success = SDL_GL_GetAttribute(attr, &_##attr##_val); \
	gl_checks[gl_check_count++] = _##attr##_val == val; \
	gl_names[gl_check_count - 1] = #attr; \
	gl_vals[gl_check_count - 1] = _##attr##_val; \
	gl_exp_vals[gl_check_count - 1] = val; 
			 
		//check if we got everything
		bool gl_checks[64];
		char* gl_names[64];
		int gl_vals[64];
		int gl_exp_vals[64];
		isize gl_check_count = 0;

		_check_gl_attribute(SDL_GL_RED_SIZE, 8);
		_check_gl_attribute(SDL_GL_GREEN_SIZE, 8);
		_check_gl_attribute(SDL_GL_BLUE_SIZE, 8);
		_check_gl_attribute(SDL_GL_ALPHA_SIZE, 8);
		_check_gl_attribute(SDL_GL_DOUBLEBUFFER, 1);
		_check_gl_attribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		_check_gl_attribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		_check_gl_attribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);
		_check_gl_attribute(SDL_GL_ACCELERATED_VISUAL, 1);
		_check_gl_attribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

		for(isize i = 0; i < gl_check_count; ++i) {
			printf("%s %s: wanted %d, got %d \n", 
					gl_names[i], 
					gl_checks[i] ? "succeeeded" : "failed", 
					gl_exp_vals[i], 
					gl_vals[i]);
		}

	}	

	// Game initializiation
	Game = Allocate(Game_Main, 1);
	{
		Game->window = window;
		Game->state = Game_State_None;
		Game->meta_arena = Allocate(Memory_Arena, 1);
		init_memory_arena(Game->meta_arena, isz(Memory_Arena) * 10);
		Game->game_arena = new_memory_arena(Kilobytes(64), Game->meta_arena);
		Game->asset_arena = new_memory_arena(Megabytes(512), Game->meta_arena);
		Game->temp_arena = new_memory_arena(Megabytes(64), Game->meta_arena);
		Game->play_arena = new_memory_arena(Megabytes(512), Game->meta_arena);
		Game->renderer_arena = new_memory_arena(Megabytes(32), Game->meta_arena);
		Game->registry_arena = new_memory_arena(Megabytes(2), Game->meta_arena);

		Game->base_path = SDL_GetBasePath();
		Game->base_path_length = strlen(Game->base_path);

		Game->input = arena_push_struct(Game->game_arena, Game_Input);
		Game->input->scancodes = arena_push_array(Game->game_arena, int8, SDL_NUM_SCANCODES);
		Game->input->keycodes = arena_push_array(Game->game_arena, int8, SDL_NUM_SCANCODES);
		Game->input->mouse = arena_push_array(Game->game_arena, int8, 16);

		init_random(&Game->r, time(NULL));
		//TODO(will) load window settings from file
		Game->window_size = v2i(1280, 720);
		Game->scale = 1.0f;

		Game->renderer = arena_push_struct(Game->game_arena, OpenGL_Renderer);
		renderer_init(Game->renderer, Game->renderer_arena);
		
		Game->registry = arena_push_struct(Game->game_arena, Game_Registry);

		Registry = Game->registry;
		Renderer = Game->renderer;
		Input = Game->input;
	}

	load_assets();

	bool running = true;
	SDL_Event event;
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	//glClearColor(1, 1, 1, 1);

	play_state->current_time = SDL_GetTicks();
	play_state->prev_time = play_state->current_time;
	while(running) {
		uint64 start_ticks = SDL_GetTicks();

		if(Game->input->num_keys_down < 0) Game->input->num_keys_down = 0;
		if(Game->input->num_mouse_down < 0) Game->input->num_mouse_down = 0;

		if(Game->input->num_keys_down > 0)
		for(int64 i = 0; i < SDL_NUM_SCANCODES; ++i) {
			int8* t = Game->input->scancodes + i;
			if(*t == State_Just_Released) {
				*t = State_Released;
			} else if(*t == State_Just_Pressed) {
				*t = State_Pressed;
			}
			t = Game->input->keycodes + i;
			if(*t == State_Just_Released) {
				*t = State_Released;
			} else if(*t == State_Just_Pressed) {
				*t = State_Pressed;
			}
		}
		if(Game->input->num_mouse_down > 0)
		for(int64 i = 0; i < 16; ++i) {
			int8* t = Game->input->mouse + i;
			if(*t == State_Just_Released) {
				*t = State_Released;
			} else if(*t == State_Just_Pressed) {
				*t = State_Pressed;
			}
		}


		while(SDL_PollEvent(&event)) {
			//TODO(will) handle text input
			switch(event.type) {
				case SDL_QUIT:
					running = false;
					break;
				case SDL_WINDOWEVENT:
					update_screen();
					break;
				case SDL_KEYDOWN:
					Game->input->num_keys_down++;
					if(!event.key.repeat) {
						Game->input->scancodes[event.key.keysym.scancode] = State_Just_Pressed;
						if(event.key.keysym.sym < SDL_NUM_SCANCODES) {
							Game->input->keycodes[event.key.keysym.sym] = State_Just_Pressed;
						}
					}
					break;
				case SDL_KEYUP:
					Game->input->num_keys_down--;
					if(!event.key.repeat) {
						Game->input->scancodes[event.key.keysym.scancode] = State_Just_Released;
						if(event.key.keysym.sym < SDL_NUM_SCANCODES) {
							Game->input->keycodes[event.key.keysym.sym] = State_Just_Released;
						}
					}
					break;
				case SDL_MOUSEBUTTONDOWN:
					Game->input->num_mouse_down++;
					Game->input->mouse[event.button.button] = State_Just_Pressed;
					break;
				case SDL_MOUSEBUTTONUP:
					Game->input->num_mouse_down--;
					Game->input->mouse[event.button.button] = State_Just_Released;
					break;
			}
		}
	
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		Input->mouse_x = mx;
		Input->mouse_y = my;

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		update();

		SDL_GL_SwapWindow(window);
		uint64 frame_ticks = SDL_GetTicks() - start_ticks;
		//if(frame_ticks > 18) printf("Slow frame! %d\n", frame_ticks);
	}

	SDL_Quit();
	return 0;
}


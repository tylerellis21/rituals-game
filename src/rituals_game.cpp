/*
 * rituals_game.cpp
 * Contains a bunch of misc stuff and game struct.
 *  - Macros for sort/search functions: DIY hashtables
 *  - Memory allocators: linear (Memory_Arena), pool? free list?
 *  - Random functions: one of the xorshift style ones, splitmix too
 *  - ButtonState enum
 *  - Game_Input struct: stores all game input stuff
 *  - Game_Assets typedef: DECLARE THIS YOURSELF LATER
 *  - Game struct: where everything is kept
 */


// Sort/Search macros
#define Generate_Quicksort_For_Type(func_name, T, Member_Macro) \
void func_name(T* array, isize count) \
{ \
	if(count > 1) { \
		T tmp = array[0]; \
		array[0] = array[count / 2]; \
		array[count / 2] = tmp; \
		isize pivot = 0; \
		for(isize i = 1; i < count; ++i) { \
			if(Member_Macro(array[i]) < Member_Macro(array[0])) { \
				tmp = array[++pivot]; \
				array[pivot] = array[i]; \
				array[i] = tmp; \
			} \
		} \
		tmp = array[0]; \
		array[0] = array[pivot]; \
		array[pivot] = tmp; \
		func_name(array, pivot); \
		func_name(array + pivot + 1, count - (pivot + 1)); \
	} \
}


// Returns -1 on fail to find.
#define Generate_Binary_Search_For_Type(func_name, T, K, Member_Key_Macro) \
isize func_name(K key, T* array, isize count) \
{ \
	if(count == 0) return -1; \
	isize min = 0, max = count - 1, mid = 0; \
	K localkey; \
	while(min <= max) { \
		mid = (min + max) / 2; \
		localkey = Member_Key_Macro(array[mid]); \
		if(localkey == key) { \
			return mid; \
		} else if(localkey < key) { \
			min = mid + 1; \
		} else { \
			max = mid - 1; \
		} \
	} \
	return -1; \
} 
 

#define _passthru_macro(x) (x) 
#define _generate_sort_and_search_for_numeric_type(t) \
	Generate_Quicksort_For_Type(t##_sort, t, _passthru_macro) \
	Generate_Binary_Search_For_Type(t##_search, t, t, _passthru_macro) 
_generate_sort_and_search_for_numeric_type(real);
_generate_sort_and_search_for_numeric_type(real32);
_generate_sort_and_search_for_numeric_type(real64);
_generate_sort_and_search_for_numeric_type(uint8);
_generate_sort_and_search_for_numeric_type(uint16);
_generate_sort_and_search_for_numeric_type(uint32);
_generate_sort_and_search_for_numeric_type(uint64);
_generate_sort_and_search_for_numeric_type(int8);
_generate_sort_and_search_for_numeric_type(int16);
_generate_sort_and_search_for_numeric_type(int32);
_generate_sort_and_search_for_numeric_type(int64);
_generate_sort_and_search_for_numeric_type(usize);
_generate_sort_and_search_for_numeric_type(isize);


// Generic error logging define
// TODO(will) implement better error logging
#define Log_Error(e) printf("There was an error: %s \n", e);


// Linear allocator
// TODO(will) implement a pool allocator

#define Arena_Push_Struct(arena, type) ((type*)arena_push(arena, sizeof(type)))
#define Arena_Push_Array(arena, type, count) ((type*)arena_push(arena, sizeof(type) * count))

struct Memory_Arena
{
	uint8* data;
	isize capacity, head;
	isize temp_head;
	Memory_Arena* next;
};

static inline isize mem_align_4(isize p)
{
	usize mod = p & 3;
	return (mod ? p + 4 - mod : p);
}

void init_memory_arena(Memory_Arena* arena, usize size)
{
	arena->data = (uint8*)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	arena->capacity = (isize)size;
	arena->head = (isize)arena->data;
	arena->temp_head = -1;
	arena->next = NULL;
}

uint8* arena_push(Memory_Arena* arena, isize size)
{
	isize old_head = arena->head;
	isize new_head = mem_align_4(size + arena->head);
	if((new_head - (isize)arena->data) > arena->capacity) {
		Log_Error("An arena was filled");
		return NULL;
	}
	arena->head = new_head;
	return (uint8*) old_head;

}

void start_temp_arena(Memory_Arena* arena)
{
	arena->temp_head = arena->head;
}

void end_temp_arena(Memory_Arena* arena)
{
	VirtualAlloc((char*)arena->temp_head, 
			arena->head - arena->temp_head, 
			MEM_RESET, PAGE_EXECUTE_READWRITE);
	arena->head = arena->temp_head;
	arena->temp_head = -1;
}

Memory_Arena* new_memory_arena(usize size, Memory_Arena* src)
{
	Memory_Arena* arena = (Memory_Arena*)arena_push(src, sizeof(Memory_Arena));
	init_memory_arena(arena, size);
	return arena;
}

// Random number generators
static inline uint64 _splitmix64(uint64* x)
{
	*x += UINT64_C(0x9E3779B97F4A7C15);
	uint64 z = *x;
	z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
	z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
	return z ^ (z >> 31);	
}

static inline uint64 _rotate_left(const uint64 t, int64 k)
{
	return (t << k) | (t >> (64 - k));
}

struct Random
{
	uint64 x, y;
};

#define Random_Max (UINT64_MAX)
uint64 next_random_uint64(Random* r)
{
	uint64 a = r->x;
	uint64 b = r->y;
	uint64 result = a + b;

	b ^= a;
	r->x = _rotate_left(a, 55) ^ b ^ (b << 14);
	r->y = _rotate_left(b, 36);
	return result;
}

void init_random(Random* r, uint64 seed) 
{
	_splitmix64(&seed);
	r->x = _splitmix64(&seed);
	r->y = _splitmix64(&seed);
	next_random_uint64(r);
}

real64 next_random_double(Random* r)
{
	uint64 x = next_random_uint64(r);
	return (real64)x / UINT64_MAX;
}

real32 next_random_float(Random* r)
{
	return (real32)(next_random_double(r));
}

real next_random(Random* r) 
{
	return (real)(next_random_double(r));
}

real rand_range(Random* r, real min, real max)
{
	return next_random_double(r) * (max - min) + min;
}

// Input stuff
enum Button_State
{
	State_Just_Released = -1,
	State_Released = 0,
	State_Pressed = 1,
	State_Just_Pressed = 2,

	Button_State_Count
};

struct Game_Input
{
	int8* scancodes;
	int8* keycodes;
	int8* mouse;
	int32 mouse_x;
	int32 mouse_y;
};

// Game struct
typedef struct Game_Assets Game_Assets;
typedef struct Debug_Log Debug_Log;
struct Game
{
	SDL_Window* window;
	SDL_Renderer* renderer;
	int32 width;
	int32 height;

	Memory_Arena* meta_arena;
	Memory_Arena* asset_arena;
	Memory_Arena* game_arena;
	Memory_Arena* temp_arena;

	Memory_Arena* play_arena;

	const char* base_path;
	isize base_path_length;

	Random r;

	Debug_Log* log;
	Game_Input* input;
	Game_Assets* assets;
};

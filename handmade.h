#if !defined(HANDMADE_H)
#define HANDMADE_H

// #include <math.h>
#include <stdint.h>
#include <stdbool.h>

/*
  Constants / typedefs
*/
#define internal_function static
#define local_persist static
#define global_variable static
#define Pi32 3.14159265359f

#define Kilobytes(value) ((value)*1024)
#define Megabytes(value) (Kilobytes(value)*1024)
#define Gigabytes(value) (Megabytes(value)*1024)
#define Terabytes(value) (Gigabytes(value)*1024)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float f32;
typedef double f64;

/*
  HANDMADE_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  HANDMADE_SLOW:
    0 - no slow code allowed
    1 - slow code is allowed
*/


/* Macros */

// Gets number of items in array
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

// Assert macro for when compiling in "slow"
// if HANDMADE_SLOW
// #define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}

// GCC Docs: This function causes the program to exit abnormally. GCC implements this function by using a target-dependent mechanism
// (such as intentionally executing an illegal instruction) or by calling abort.
// The mechanism used may vary from release to release so you should not rely on any particular implementation.
#define Assert(Expression) if(!(Expression)) {__builtin_trap();}

// #else
// #define Assert(Expression)
// #endif

/*
  Data Structures
*/

struct GameOffScreenBuffer {
	void *memory;
	int width;
	int height;
	int pitch;
  int bytes_per_pixel;
};

struct GameSoundOutputBuffer {
  int samples_per_second;
  int sample_count;
  i16 *samples;
};

struct GameButtonState {
  // Indicates how many times a button transistioned from on->off or off->on
  int half_transition_count;

  // Indicates if the button's last state is down. This initial state can be calculated
  // from this information with the half_transition_count
  bool ended_down;
};

struct GameControllerInput {
  bool is_analog;
  bool is_connected;
  float avg_stick_x;
  float avg_stick_y;

  union {
    GameButtonState buttons[12];
    struct {
      GameButtonState move_up;
      GameButtonState move_down;
      GameButtonState move_left;
      GameButtonState move_right;
      GameButtonState action_up;
      GameButtonState action_down;
      GameButtonState action_left;
      GameButtonState action_right;
      GameButtonState left_shoulder;
      GameButtonState right_shoulder;
      GameButtonState start;
      GameButtonState back;
    };
  };
};

struct GameInput {
  /* Mouse stuff for debugging */
  GameButtonState mouse_buttons[5];
  i32 mouse_x, mouse_y, mouse_z;

  f32 target_seconds_per_frame;

  GameControllerInput controllers[5];
};

struct GameState {
  f32 player_x;
  f32 player_y;

  i32 player_tile_map_x;
  i32 player_tile_map_y;
};

struct TileMap {
  u32 *tiles;
};

struct WorldPosition {
  i32 tile_map_x;  
  i32 tile_map_y;
  i32 tile_x;
  i32 tile_y;

  // Tile relative x/y
  f32 x;
  f32 y;
};

struct RawPosition {
  i32 tile_map_x;  
  i32 tile_map_y;

  // tile-map relative x/y
  f32 x;
  f32 y;
};

struct World {
  i32 tile_map_count_x;
  i32 tile_map_count_y;

  i32 count_x;
  i32 count_y;
  
  f32 upper_left_x;
  f32 upper_left_y;
  f32 tile_width;
  f32 tile_height;

  TileMap *tile_maps;
};

/* Debug Specific */
struct DebugFileReadResult {
  u32 contents_size;
  void *contents;
};


// Calling back into the platform layer will need this (free file memory, etc..)
// Not every platform does a good job of returning what thread you're on.
struct ThreadContext {
  int placeholder;
};


// How this works...
// 1 - Define the macro and what it expands to.
//     the (name) portion allows you to put a custom name that will expand when it is used in the definition.
//     The definition here is just a function signature where the name of the function is whatever you put into the macro.
// 2 - the typedef actually invokes the macro. This means it'll expand the macro into code at compile time with whatever you defined.
//     In this case, it takes 'debug_platform_free_file_memory' and then turns it into...
//     'void debug_platform_free_file_memory(void *memory)' to be used as a function declaration.
// 3 - Any code using this header can use that typedef'd function signature to implement the function .
#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(ThreadContext *thread_ctx, void *memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) DebugFileReadResult name(ThreadContext *thread_ctx, const char *file_name)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(ThreadContext *thread_ctx, const char *file_name, u32 memory_size, void *memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

struct GameMemory {
  u64 permanent_storage_size;
  u64 transient_storage_size;
  void *permanent_storage;
  void *transient_storage;

  // pointers to shared sebug file writing code.
  // The type is the output of a macro which creates the function signatures
  // The macros are used in platform code to define the functions and the function
  // pointers are stored in the game memory so the same function can be used
  // by both the game and the platform layer. Now the signature can be managed by the macros.
  debug_platform_free_file_memory *dbg_platform_free_file_memory;
  debug_platform_read_entire_file *dbg_platform_read_entire_file;
  debug_platform_write_entire_file *dbg_platform_write_entire_file;

  bool is_initialized;
};



/*
  Functions
*/
// TODO - Why do I have these in the handmade header?
// static DebugFileReadResult debug_platform_read_entire_file(char *file_name);
// static void debug_platform_free_file_memory(void *memory);
// static bool debug_platform_write_entire_file(char *file_name, u32 memory_size, void *memory);

// Gets the controller from the GameInput and asserts the controller index
// is in bounds
inline GameControllerInput *get_controller(GameInput *game_input, int controller_idx) {
  Assert(controller_idx < ArrayCount(game_input->controllers));
  GameControllerInput *controller = &game_input->controllers[controller_idx];
  return controller;
}

inline u32 u64_safe_truncate_to_u32(u64 value) {
  Assert(value <= 0xFFFFFFFF);
  u32 result = (u32)value;
  return result;
}

/*
  Function stubs
*/



#define GAME_UPDATE_AND_RENDER(name) void name(ThreadContext *thread_ctx, GameMemory *memory, GameInput *input,GameOffScreenBuffer *buffer)
typedef GAME_UPDATE_AND_RENDER(PtrGameUpdateAndRender);
GAME_UPDATE_AND_RENDER(game_update_and_render_stub) {}

#define GAME_GET_SOUND_SAMPLES(name) void name(ThreadContext *thread_ctx, GameMemory *game_memory, GameSoundOutputBuffer *sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(PtrGameGetSoundSamples);
GAME_GET_SOUND_SAMPLES(game_get_sound_samples_stub) {}

#endif

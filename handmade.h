#if !defined(HANDMADE_H)
#define HANDMADE_H

#include <math.h>
#include <stdint.h>

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

//#include "handmade.h"
//#include "handmade.cpp"

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
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
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
  GameControllerInput controllers[5];
};

struct GameState {
  int tone_hz;
  int blue_offset;
  int green_offset;
  f32 sine;

  int player_x;
  int player_y;
  f32 jump_timer;
};

struct GameMemory {
  u64 permanent_storage_size;
  u64 transient_storage_size;
  void *permanent_storage;
  void *transient_storage;
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
  // Assert(controller_idx < ArrayCount(game_input->controllers));
  GameControllerInput *controller = &game_input->controllers[controller_idx];
  return controller;
}

#define GAME_UPDATE_AND_RENDER(name) void name(GameMemory *memory,GameInput *input,GameOffScreenBuffer *buffer)
typedef GAME_UPDATE_AND_RENDER(PtrGameUpdateAndRender);
GAME_UPDATE_AND_RENDER(game_update_and_render_stub) {}

#define GAME_GET_SOUND_SAMPLES(name) void name(GameMemory *game_memory,GameSoundOutputBuffer *sound_buffer)
typedef GAME_GET_SOUND_SAMPLES(PtrGameGetSoundSamples);
GAME_GET_SOUND_SAMPLES(game_get_sound_samples_stub) {}

#endif

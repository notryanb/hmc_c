#include <math.h>
#include <stdint.h>

#define internal_function static
#define local_persist static
#define global_variable static
#define Pi32 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

//#include "handmade.h"
//#include "handmade.cpp"

#if !defined(HANDMADE_H)

// Macros
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

struct DebugFileReadResult {
  uint32_t contents_size;
  void *contents;
};

static DebugFileReadResult debug_platform_read_entire_file(char *file_name);
static void debug_platform_free_file_memory(void *memory);
static bool debug_platform_write_entire_file(char *file_name, uint32_t memory_size, void *memory);

// Data Structures
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
  int16_t *samples;
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
  float sine;

  int player_x;
  int player_y;
  float jump_timer;
};

struct GameMemory {
  uint64_t permanent_storage_size;
  uint64_t transient_storage_size;
  void *permanent_storage;
  void *transient_storage;
  bool is_initialized;
};

// Fuctions

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

#define HANDMADE_H
#endif

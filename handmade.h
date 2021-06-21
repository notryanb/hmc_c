#if !defined(HANDMADE_H)

// Macros
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

struct DebugFileReadResult {
  uint32_t contents_size;
  void *contents;
};

DebugFileReadResult debug_platform_read_entire_file(char *file_name);
void debug_platform_free_file_memory(void *memory);
bool debug_platform_write_entire_file(char *file_name, uint32_t memory_size, void *memory);

// Data Structures
struct GameOffScreenBuffer {
	void *memory;
	int width;
	int height;
	int pitch;
};

struct GameSoundOutputBuffer {
  int samples_per_second;
  int sample_count;
  int16_t *samples;
};

struct GameButtonState {
  int half_transition_count;
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
};

struct GameMemory {
  bool is_initialized;
  uint64_t permanent_storage_size;
  void *permanent_storage;
  uint64_t transient_storage_size;
  void *transient_storage;
};

// Fuctions

// Gets the controller from the GameInput and asserts the controller index
// is in bounds
inline GameControllerInput *get_controller(GameInput *game_input, int controller_idx) {
  // Assert(controller_idx < ArrayCount(game_input->controllers));
  GameControllerInput *controller = &game_input->controllers[controller_idx];
  return controller;
}

static void game_update_and_render(
    GameMemory *game_memory,
    GameInput *game_input,
    GameOffScreenBuffer *buffer
);

static void game_get_sound_samples(
    GameMemory *game_memory,
    GameSoundOutputBuffer *sound_buffer
);

#define HANDMADE_H
#endif

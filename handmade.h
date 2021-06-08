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

  float start_x;
  float start_y;
  
  float min_x;
  float min_y;
 
  float max_x;
  float max_y;
 
  float end_x;
  float end_y;

  union {
    GameButtonState buttons[6];
    struct {
      GameButtonState up;
      GameButtonState down;
      GameButtonState left;
      GameButtonState right;
      GameButtonState left_shoulder;
      GameButtonState right_shoulder;
    };
  };
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

struct GameInput {
  GameControllerInput controllers[5];
};


// Fuctions

static void game_update_and_render(
    GameMemory *game_memory,
    GameInput *game_input,
    GameOffScreenBuffer *buffer,
    GameSoundOutputBuffer *sound_buffer
);


#define HANDMADE_H
#endif

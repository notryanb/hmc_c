#if !defined(HANDMADE_H)

// Macros
#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))

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

struct GameInput {
  GameControllerInput controllers[4];
};


// Fuctions

static void game_update_and_render(
    GameInput *game_input,
    GameOffScreenBuffer *buffer,
    GameSoundOutputBuffer *sound_buffer
);


#define HANDMADE_H
#endif

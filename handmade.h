#if !defined(HANDMADE_H)


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

/*
 * Needs timing, user input, bitmap buffer, and sound buffer.
 */
static void game_update_and_render(
    GameOffScreenBuffer *buffer,
    GameSoundOutputBuffer *sound_buffer,
    int toneHz
);


#define HANDMADE_H
#endif

#if !defined(HANDMADE_H)


struct GameOffScreenBuffer {
	void *memory;
	int width;
	int height;
	int pitch;
};

/*
 * Needs timing, user input, bitmap buffer, and sound buffer.
 */
internal_function void game_update_and_render(GameOffScreenBuffer *buffer);
#define HANDMADE_H
#endif

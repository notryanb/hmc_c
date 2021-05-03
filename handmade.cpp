#include "handmade.h"

internal_function void render_weird_gradient(GameOffScreenBuffer *buffer, int x_offset, int y_offset) {
	// ensure void* is cast to unsigned 8-bit int so we can do pointer arithmetic
	uint8 *row = (uint8 *)buffer->memory;

	for(int y = 0; y < buffer->height; ++y) {
		uint32 *pixel = (uint32 *)row;

		for(int x = 0; x < buffer->width; ++x) {
			/* 
			 * This is little endian and swapped by Windows developers
			 * Index: 3  2  1  0  <-- Smallest byte
			 * Bytes: 00 00 00 00
			 * Repr:  xx RR GG BB
			 */
			uint8 blue = (x + x_offset);
			uint8 green = (y + y_offset); 
			*pixel++ = ((green << 8) | blue);
		}

		row += buffer->pitch;
	}

}

internal_function void game_update_and_render(GameOffScreenBuffer *buffer) {
	int blue_offset = 0;
	int green_offset = 0;
	render_weird_gradient(buffer, blue_offset, green_offset);
};

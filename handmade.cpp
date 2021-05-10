#include "handmade.h"





static void render_weird_gradient(GameOffScreenBuffer *buffer, int x_offset, int y_offset) {
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

static void output_sound(GameSoundOutputBuffer *sound_buffer, int toneHz) {
    static float tSine; 
    int16_t tone_volume = 3000;
	  //int toneHz = 256;
    int wave_period = sound_buffer->samples_per_second / toneHz;

		int16_t *sample_out = sound_buffer->samples;

		for(int sample_index = 0; sample_index < sound_buffer->sample_count; ++sample_index) {
			float sine_value = sinf(tSine);
			int16_t sample_value = (int16)(sine_value * tone_volume);
			*sample_out++ = sample_value;
			*sample_out++ = sample_value;

      tSine += 2.0f * Pi32 * (1.0f / (float)wave_period);
		}
}

static void game_update_and_render(
    GameOffScreenBuffer *buffer, 
    GameSoundOutputBuffer *sound_buffer,
    int toneHz
) {
  output_sound(sound_buffer, toneHz);

	int blue_offset = 0;
	int green_offset = 0;
	render_weird_gradient(buffer, blue_offset, green_offset);
};

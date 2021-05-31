#include "handmade.h"

static void 
render_weird_gradient(GameOffScreenBuffer *buffer, int x_offset, int y_offset) {
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

static void 
output_sound(GameSoundOutputBuffer *sound_buffer, int toneHz) {
    static float tSine; 
    int16_t tone_volume = 3000;
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

static void 
game_update_and_render(
    GameMemory *memory,
    GameInput *input,
    GameOffScreenBuffer *buffer, 
    GameSoundOutputBuffer *sound_buffer
) {
  GameState *game_state = (GameState *)memory->permanent_storage;

  if (!memory->is_initialized) {
    char *file_name = "D:/Programming/handmade_hero/README.md";
    DebugFileReadResult file_result = debug_platform_read_entire_file(file_name);

    if (file_result.contents) {
      debug_platform_write_entire_file(
          "D:/Programming/handmade_hero/test.out", 
          file_result.contents_size,
          file_result.contents
      );

      debug_platform_free_file_memory(file_result.contents);
    }


    game_state->blue_offset = 0;
    game_state->green_offset = 0;
    game_state->tone_hz = 256;

    memory->is_initialized = true;
  }

  GameControllerInput *input0 = &input->controllers[0];

  if(input0->is_analog) {
    game_state->blue_offset += (int)(4.0f * (input0->end_x));
    game_state->tone_hz = 256 + (int)(128.0f * (input0->end_y));
  } else {

  }

  if(input0->down.ended_down) {
    game_state->green_offset += 1;
  }

  output_sound(sound_buffer, game_state->tone_hz);
	render_weird_gradient(buffer, game_state->blue_offset, game_state->green_offset);
};

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
output_sound(GameState *game_state, GameSoundOutputBuffer *sound_buffer, int toneHz) {
    int16_t tone_volume = 3000;
    int wave_period = sound_buffer->samples_per_second / toneHz;

		int16_t *sample_out = sound_buffer->samples;

		for(int sample_index = 0; sample_index < sound_buffer->sample_count; ++sample_index) {
			float sine_value = sinf(game_state->sine);
			int16_t sample_value = (int16)(sine_value * tone_volume);
			*sample_out++ = sample_value;
			*sample_out++ = sample_value;

      game_state->sine += 2.0f * Pi32 * (1.0f / (float)wave_period);
      if (game_state->sine > 2.0f * Pi32) {
        game_state->sine -= 2.0f * Pi32;
      }
		}
}

// Renders a small square as the player
static void
render_player(GameOffScreenBuffer *buffer, int player_x, int player_y) {
  uint8_t *end_of_buffer = (uint8_t *)buffer->memory + 
    buffer->pitch * 
    buffer->height;


  uint32_t color = 0xFFFFFFFF;
  int top = player_y;
  int bottom = player_y + 10;

  for(int x = player_x; x < player_x + 10; ++x) {
    uint8_t *pixel =  (uint8_t *)buffer->memory + 
        (x * buffer->bytes_per_pixel) + 
        (top * buffer->pitch);

    for(int y = top; y < bottom; ++y) {
      if ((pixel >= buffer->memory) && ((pixel + 4) <= end_of_buffer)) {
        *(uint32_t *)pixel = color; 
      }

      pixel += buffer->pitch;
    }
  }
}

// the declspec ensures this function is exported in the DLL
extern "C" __declspec(dllexport) GAME_UPDATE_AND_RENDER(game_update_and_render)
{
  GameState *game_state = (GameState *)memory->permanent_storage;

  if (!memory->is_initialized) {
    /* Debug file reading/writing
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
    */


    game_state->blue_offset = 0;
    game_state->green_offset = 0;
    game_state->tone_hz = 256;
    game_state->sine = 0.0f;

    game_state->player_x = 100;
    game_state->player_y = 100;

    memory->is_initialized = true;
  }

  for(int controller_idx = 0;
      controller_idx < ArrayCount(input->controllers);
      ++controller_idx) 
  {
    GameControllerInput *controller = &input->controllers[controller_idx];
    
    if(controller->is_analog) {
      // TODO: Analog movement tuning
      game_state->blue_offset += (int)(4.0f * (controller->avg_stick_x));
      game_state->tone_hz = 256 + (int)(128.0f * (controller->avg_stick_y));
    } else {
      if (controller->move_left.ended_down) {
        game_state->blue_offset -= 1;
      }
      
      if (controller->move_right.ended_down) {
        game_state->blue_offset += 1;
      }
      // TODO - Digital movement tuning
    }


    game_state->player_x += (int)(4.0f * controller->avg_stick_x);
    game_state->player_y -= (int)(4.0f * controller->avg_stick_y);
   
    if (game_state->jump_timer > 0) {
      game_state->player_y -= (int)(10.f * sinf(game_state->jump_timer));
    }

    if(controller->action_down.ended_down) {
      //game_state->green_offset += 1;
      game_state->jump_timer = 1.0;
      game_state->player_y -= 10;
    }
    game_state->jump_timer -= 0.033f;
  }

	render_weird_gradient(buffer, game_state->blue_offset, game_state->green_offset);
  render_player(buffer, game_state->player_x, game_state->player_y);
};


extern "C" __declspec(dllexport) GAME_GET_SOUND_SAMPLES(game_get_sound_samples)
{
  GameState *game_state = (GameState *)game_memory->permanent_storage;
  output_sound(game_state, sound_buffer, game_state->tone_hz);
}


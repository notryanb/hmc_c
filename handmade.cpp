#include "handmade.h"


// The sound_buffer is interleaved LRLRLR...
static void output_sound(ThreadContext *thread_ctx, GameState *game_state, GameSoundOutputBuffer *sound_buffer, int toneHz) {
    i16 tone_volume = 2500;
    int wave_period = sound_buffer->samples_per_second / toneHz;

		i16 *sample_out = sound_buffer->samples;

		for(int sample_index = 0; sample_index < sound_buffer->sample_count; ++sample_index) {
#if 0
			f32 sine_value = sinf(game_state->sine);
			i16 sample_value = (i16)(sine_value * tone_volume);
#else
			i16 sample_value = 0;
#endif
			*sample_out++ = sample_value;
			*sample_out++ = sample_value;

#if 0
      game_state->sine += 2.0f * Pi32 * (1.0f / (f32)wave_period);
      if (game_state->sine > 2.0f * Pi32) {
        game_state->sine -= 2.0f * Pi32;
      }
#endif
		}
}

static i32 f32_round_to_i32(f32 val) {
  i32 result = (i32)(val + 0.5f);
  return result;
}

static u32 f32_round_to_u32(f32 val) {
  u32 result = (u32)(val + 0.5f);
  return result;
}

// Renders a small square as the player
static void draw_rectangle(
  GameOffScreenBuffer *buffer,
  f32 min_x, f32 min_y,
  f32 max_x, f32 max_y,
  u32 color
) {
  i32 x_min = f32_round_to_i32(min_x);
  i32 y_min = f32_round_to_i32(min_y);
  i32 x_max = f32_round_to_i32(max_x);
  i32 y_max = f32_round_to_i32(max_y);

  if (x_min < 0) { x_min = 0; }
  if (y_min < 0) { y_min = 0; }
  if (x_max > buffer->width) { x_max = buffer->width; }
  if (y_max > buffer->height) { y_max = buffer->height; }
  
  u8 *end_of_buffer = (u8 *)buffer->memory + buffer->pitch * buffer->height;
  // u32 color = 0xFFFFFFFF;

  // Advance the pointer to the top left of the rectangle we're about to draw
  u8 *row=  (u8 *)buffer->memory + 
      (x_min * buffer->bytes_per_pixel) + 
      (y_min * buffer->pitch);

  for(int y = y_min; y < y_max; ++y) {
    u32 *pixel = (u32 *)row;

    for(int x = x_min; x < x_max; ++x) {
      *pixel++ = color; 
    }

    row += buffer->pitch;
  }
}

// the declspec ensures this function is exported in the DLL
extern "C" __declspec(dllexport) GAME_UPDATE_AND_RENDER(game_update_and_render) {
  GameState *game_state = (GameState *)memory->permanent_storage;

  if (!memory->is_initialized) {
    memory->is_initialized = true;
  }

  for (int controller_idx = 0;
      controller_idx < ArrayCount(input->controllers);
      ++controller_idx) 
  {
    GameControllerInput *controller = get_controller(input, controller_idx);
    
    if (controller->is_analog) {
      // NOTE: Analog Movement Tuning
    } else {
      // NOTE: Digital Movement Tuning
    }
  }

  draw_rectangle(buffer, 0.0f, 0.0f, (f32)buffer->width, (f32)buffer->height, 0x00FF00FF);
  draw_rectangle(buffer, 50.0f, 50.0f, 100.0f, 100.0f, 0xFFFFFFFF);
};


// Gets sound samples from the game state which is initialized / allocatred in 
// the Windows specific code.
extern "C" __declspec(dllexport) GAME_GET_SOUND_SAMPLES(game_get_sound_samples)
{
  GameState *game_state = (GameState *)game_memory->permanent_storage;
  output_sound(thread_ctx, game_state, sound_buffer, 400);
}

// DEBUG
// static void  render_weird_gradient(GameOffScreenBuffer *buffer, int x_offset, int y_offset) {
// 	// ensure void* is cast to unsigned 8-bit int so we can do pointer arithmetic
// 	u8 *row = (u8 *)buffer->memory;

// 	for(int y = 0; y < buffer->height; ++y) {
//     u32 *pixel = (u32 *)row;

// 		for(int x = 0; x < buffer->width; ++x) {
// 			/* 
// 			 * This is little endian and swapped by Windows developers
// 			 * Index: 3  2  1  0  <-- Smallest byte
// 			 * Bytes: 00 00 00 00
// 			 * Repr:  xx RR GG BB
// 			 */
// 			u8 blue = (x + x_offset);
// 			u8 green = (y + y_offset); 
// 			*pixel++ = ((green << 8) | blue);
// 		}

// 		row += buffer->pitch;
// 	}
// }

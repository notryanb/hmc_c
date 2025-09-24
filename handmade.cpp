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

inline i32 f32_round_to_i32(f32 val) {
  i32 result = (i32)(val + 0.5f);
  return result;
}

inline u32 f32_round_to_u32(f32 val) {
  u32 result = (u32)(val + 0.5f);
  return result;
}

inline i32 f32_truncate_to_i32(f32 val) {
  i32 result = (i32)val;
  return result;
}

#include<math.h> // TODO - eventually remove 
inline i32 f32_floor_to_i32(f32 val) {
  i32 result = (int)floorf(val);
  return result;
}

inline TileMap * world_get_tile_map(World *world, i32 tile_map_x, i32 tile_map_y) {
  TileMap *tile_map = 0;

  if ((tile_map_x >= 0) && (tile_map_x < world->tile_map_count_x) &&
      (tile_map_y >= 0) && (tile_map_y < world->tile_map_count_y)) {
    tile_map = &world->tile_maps[tile_map_y * world->tile_map_count_x + tile_map_x];
  }

  return tile_map;
}

inline u32 tile_map_get_unchecked_tile_value(World *world, TileMap *tile_map, i32 x, i32 y) {
  Assert(tile_map);
  Assert((x >= 0) && (x < world->count_x) && (y >= 0) && (y < world->count_y));
  
  u32 val = tile_map->tiles[y * world->count_x + x];
  return val;
}

inline bool tile_map_is_point_empty(World *world, TileMap *tile_map, i32 x, i32 y) {
  bool empty = false;

  if (tile_map) {
    if ((x >= 0) && (x < world->count_x) &&
        (y >= 0) && (y < world->count_y))
    {
      u32 tile_map_val = tile_map_get_unchecked_tile_value(world, tile_map, x, y);
      empty = tile_map_val == 0;
    }
  }

  return empty;
}

static WorldPosition get_canonical_position(World *world, RawPosition raw_pos) {
  WorldPosition world_pos;
  world_pos.tile_map_x = raw_pos.tile_map_x;
  world_pos.tile_map_y = raw_pos.tile_map_y;

  f32 x = raw_pos.x - world->upper_left_x;
  f32 y = raw_pos.y - world->upper_left_y;
  world_pos.tile_x = f32_floor_to_i32(x / world->tile_width);
  world_pos.tile_y = f32_floor_to_i32(y / world->tile_height);
  world_pos.x = x - world_pos.tile_x * world->tile_width;
  world_pos.y = y - world_pos.tile_y * world->tile_height;

  Assert(world_pos.x >= 0);
  Assert(world_pos.y >= 0);
  Assert(world_pos.x < world->tile_width);
  Assert(world_pos.y < world->tile_height);

  if (world_pos.tile_x < 0) {
    world_pos.tile_x = world->count_x + world_pos.tile_x;
    --world_pos.tile_map_x;
  }

  if (world_pos.tile_y < 0) {
    world_pos.tile_y = world->count_y + world_pos.tile_y;
    --world_pos.tile_map_y;
  }

  if (world_pos.tile_x >= world->count_x) {
    world_pos.tile_x = world_pos.tile_x - world->count_x;
    ++world_pos.tile_map_x;
  }

  if (world_pos.tile_y >= world->count_y) {
    world_pos.tile_y = world_pos.tile_y - world->count_y;
    ++world_pos.tile_map_y;
  }

  return world_pos;
}

inline bool world_is_point_empty(World *world, RawPosition test_pos) {
  bool empty = false;

  WorldPosition canonical_pos = get_canonical_position(world, test_pos);
  TileMap *tile_map = world_get_tile_map(world, canonical_pos.tile_map_x, canonical_pos.tile_map_y);
  empty = tile_map_is_point_empty(world, tile_map, canonical_pos.tile_x, canonical_pos.tile_y);

  return empty;
}


// Renders a small square as the player
static void draw_rectangle(
  GameOffScreenBuffer *buffer,
  f32 min_x, f32 min_y,
  f32 max_x, f32 max_y,
  f32 r, f32 g, f32 b
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
  u32 color = ((f32_round_to_u32(r * 255.0f) << 16) |
               (f32_round_to_u32(g * 255.0f) << 8) |
               (f32_round_to_u32(b * 255.0f) << 0));

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


#define TILE_MAP_COUNT_X 17
#define TILE_MAP_COUNT_Y 9

  u32 tiles_00[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
    { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 1, 1,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 1, 1, 0,  1,  0, 0, 1, 1,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  1,  0, 0, 0, 0,  0, 0, 0, 0 },
    { 1, 1, 0, 0,  0, 0, 0, 0,  1,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 1, 1, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 1, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  1, 1, 1, 1 },
    { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 }
  };

  u32 tiles_01[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
    { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 }
  };

  u32 tiles_10[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
    { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 }
  };

  u32 tiles_11[TILE_MAP_COUNT_Y][TILE_MAP_COUNT_X] = {
    { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 0, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 0, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 0, 0, 1 },
    { 1, 1, 1, 1,  1, 1, 1, 1,  1,  1, 1, 1, 1,  1, 1, 1, 1 }
  };

  TileMap tile_maps[2][2];
  tile_maps[0][0].tiles = (u32 *)tiles_00;
  tile_maps[0][1].tiles = (u32 *)tiles_10;
  tile_maps[1][0].tiles = (u32 *)tiles_01;
  tile_maps[1][1].tiles = (u32 *)tiles_11;

  World world;
  world.count_x = TILE_MAP_COUNT_X;
  world.count_y = TILE_MAP_COUNT_Y;
  world.upper_left_x = -30;
  world.upper_left_y = 0;
  world.tile_width = 60.0f;
  world.tile_height = 60.0f;
  world.tile_map_count_x = 2;
  world.tile_map_count_y = 2;
  world.tile_maps = (TileMap *)tile_maps;

  f32 player_width  = world.tile_width * 0.75;
  f32 player_height = world.tile_height;

  TileMap *tile_map = world_get_tile_map(&world, game_state->player_tile_map_x, game_state->player_tile_map_y);
  Assert(tile_map);
  
  if (!memory->is_initialized) {
    game_state->player_x = 130.0f;
    game_state->player_y = 130.0f;

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
      f32 player_x_delta = 0.0f; // pixels per second
      f32 player_y_delta = 0.0f; // pixels per second

      if (controller->move_up.ended_down) {
        player_y_delta = -1.0f;        
      }
      if (controller->move_down.ended_down) {
        player_y_delta = 1.0f;        
      }
      if (controller->move_left.ended_down) {
        player_x_delta = -1.0f;        
      }
      if (controller->move_right.ended_down) {
        player_x_delta = 1.0f;        
      }

      // Multiply by 128 pixels per second
      player_x_delta *= 128.0f;
      player_y_delta *= 128.0f;

      f32 new_player_x = game_state->player_x + input->target_seconds_per_frame * player_x_delta;
      f32 new_player_y = game_state->player_y + input->target_seconds_per_frame * player_y_delta;

      RawPosition player_pos = {
        .tile_map_x = game_state->player_tile_map_x,
        .tile_map_y = game_state->player_tile_map_y,
        .x = new_player_x,
        .y = new_player_y,
      };

      RawPosition player_left_pos = player_pos;
      player_left_pos.x -= 0.5f * player_width;

      RawPosition player_right_pos = player_pos;
      player_right_pos.x += 0.5f * player_width;
      
      if (world_is_point_empty(&world, player_pos) &&
          world_is_point_empty(&world, player_left_pos) &&
          world_is_point_empty(&world, player_right_pos)
      ) {
        WorldPosition canonical_pos = get_canonical_position(&world, player_pos);
        game_state->player_tile_map_x = canonical_pos.tile_map_x;
        game_state->player_tile_map_y = canonical_pos.tile_map_y;

        game_state->player_x = world.upper_left_x + world.tile_width * canonical_pos.tile_x + canonical_pos.x;
        game_state->player_y = world.upper_left_y + world.tile_height * canonical_pos.tile_y + canonical_pos.y;
      }
    }
  }


  // Clear screen to magenta
  draw_rectangle(buffer, 0.0f, 0.0f, (f32)buffer->width, (f32)buffer->height, 1.0f, 0.0f, 1.0f);

  /* Draw tile map */
  for (int row = 0; row < TILE_MAP_COUNT_Y; ++row) {
    for (int col = 0; col < TILE_MAP_COUNT_X; ++col) {
      u32 tile_id = tile_map_get_unchecked_tile_value(&world, tile_map, col, row);
      f32 gray = 0.3f;

      if (tile_id == 1) {
        gray = 1.0f;
      }

      f32 min_x = world.upper_left_x + (f32)col * world.tile_width;
      f32 min_y = world.upper_left_y + (f32)row * world.tile_height;
      f32 max_x = min_x + world.tile_width;
      f32 max_y = min_y + world.tile_height;
      draw_rectangle(buffer, min_x, min_y, max_x, max_y, gray, gray, gray);
    }
  }

  /* Draw Player */
  f32 player_red = 1.0f;
  f32 player_green = 1.0f;
  f32 player_blue = 0.0f;

  f32 player_left = game_state->player_x - 0.5f * player_width;
  f32 player_top = game_state->player_y - player_height;

  draw_rectangle(buffer,
    player_left, player_top,
    player_left + player_width, player_top + player_height,
    player_red, player_green, player_blue
  );
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

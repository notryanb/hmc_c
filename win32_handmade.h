#if !defined(WIN32_HANDMADE_H)
#define WIN32_HANDMADE_H

/*
  Data Structures
*/
// struct DebugFileReadResult {
//   u32 contents_size;
//   void *contents;
// };

// Holds function pointers imported for live-loading game code.
struct Win32GameCode {
  // The shared dll where these function pointers are defined and implemented
  HMODULE dll;

  // The last time this file was written to.
  FILETIME last_write_time;

  // function pointer
  PtrGameUpdateAndRender *update_and_render;
  
  // function pointer
  PtrGameGetSoundSamples *get_sound_samples;

  // Indicates if the function pointers were loaded
  bool is_valid;
};

struct Win32DebugSoundCursor {
  DWORD output_play_cursor;
  DWORD output_write_cursor;
  DWORD output_location;
  DWORD output_byte_count;
  DWORD flip_play_cursor;
  DWORD flip_write_cursor;
  DWORD expected_flip_play_cursor;
};

struct Win32SoundOutput {
	DWORD secondary_buffer_size;
  DWORD safety_bytes;
	int samples_per_second;
	int bytes_per_sample;
  int latency_sample_count;
	u32 running_sample_index;
};


struct Win32OffScreenBuffer {
	BITMAPINFO info;
	void *memory;
	int width;
	int height;
	int pitch;
	int bytes_per_pixel;
};

struct Win32WindowDimension {
	int width;
	int height;
};

struct Win32State {
  // indicates the size of the game memory chunk
  u64 game_memory_total_size;

  // pointer to the game memory address
  void *game_memory;

  // File handle to where the state gets persisted
  HANDLE recording_handle;

  // Indicates position into the recording state
  int input_recording_index;
  
  // File handle to where the state is read from
  HANDLE playback_handle;

  // Indicates position into the playback state
  int input_playback_index;
};



/*
   Functions
*/
// static DebugFileReadResult debug_platform_read_entire_file(char *file_name);
// static void debug_platform_free_file_memory(void *memory);
// static bool debug_platform_write_entire_file(char *file_name, u32 memory_size, void *memory);


#endif

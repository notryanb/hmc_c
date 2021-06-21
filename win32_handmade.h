
#if !defined(WIN32_HANDMADE_H)

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
	int samples_per_second;
	uint32_t running_sample_index;
	int bytes_per_sample;
	DWORD secondary_buffer_size;
  DWORD safety_bytes;
  int latency_sample_count;
};


struct  Win32OffScreenBuffer {
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


#define WIN32_HANDMADE_H
#endif

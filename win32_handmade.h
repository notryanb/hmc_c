
#if !defined(WIN32_HANDMADE_H)

struct Win32DebugSoundCursor {
  DWORD play_cursor;
  DWORD write_cursor;
};

struct Win32SoundOutput {
	int samples_per_second;
	uint32_t running_sample_index;
	int bytes_per_sample;
	DWORD secondary_buffer_size;
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

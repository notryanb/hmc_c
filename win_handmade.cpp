#include "handmade.h"
#include <windows.h>

#include "win32_handmade.h"

#include <stdio.h> // for _snprintf_s logging
#include <xinput.h> // Device input like joypads
#include <dsound.h>


// Macro to create function prototypes
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)

// typedef macro to expand out functions with the names given as input
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);

// Create function stubs with bodies
X_INPUT_GET_STATE(XInputGetStateStub) { return(0); }
X_INPUT_SET_STATE(XInputSetStateStub) { return(0); }

// Set variables to function pointers
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

// Aliases the real function name to the function stub
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_


global_variable bool Running = false;
global_variable Win32OffScreenBuffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondarySoundBuffer;

// QueryPerformanceFrequency indicates how many clocks we go through in one second.
// This counter is fixed at system boot and consistent across all processors
global_variable int64_t GlobalPerfCounterFrequency;

static void win32_begin_recording_input(Win32State *win32_state, int recording_index) {
  win32_state->input_recording_index = recording_index;

  win32_state->recording_handle = CreateFileA(
    "debug_playback.hmh",
    GENERIC_WRITE,
    0,
    0,
    CREATE_ALWAYS,
    0,
    0
  );

  // Windows has a size limit of how much memory can be written to a file at once.
  // The game state is going to eventually become large enough to hit this limit.
  // If we exceed 4GB, we need to change this to a for loop and add the memory incrementally.
  // Assert(win32_state->game_memory_total_size < 0xFFFFFFFF);

  DWORD bytes_to_write = (DWORD)win32_state->game_memory_total_size;
  DWORD bytes_written = 0;
  WriteFile(
      win32_state->recording_handle,
      win32_state->game_memory,
      bytes_to_write,
      &bytes_written,
      0
  );
}

static void win32_end_recording_input(Win32State *win32_state) {
  CloseHandle(win32_state->recording_handle);
  win32_state->input_recording_index = 0;
}

static void win32_begin_playback_input(Win32State *win32_state, int playback_index) {
  win32_state->input_playback_index = playback_index;

  win32_state->playback_handle = CreateFileA(
    "debug_playback.hmh",
    GENERIC_READ,
    FILE_SHARE_READ,
    0,
    OPEN_EXISTING,
    0,
    0
  );
  
  DWORD bytes_to_read = (DWORD)win32_state->game_memory_total_size;
  DWORD bytes_read = 0;
  ReadFile(
      win32_state->playback_handle,
      win32_state->game_memory,
      bytes_to_read,
      &bytes_read,
      0
  );
}

static void win32_end_playback_input(Win32State *win32_state) {
  CloseHandle(win32_state->playback_handle);
  win32_state->input_playback_index = 0;
}

// write playback to file
static void win32_record_input(Win32State *win32_state, GameInput *new_input) {
  DWORD bytes_written;
  WriteFile(
      win32_state->recording_handle,
      new_input,
      sizeof(*new_input),
      &bytes_written,
      0
  );
}

// read playback from file
static void win32_playback_input(Win32State *win32_state, GameInput *new_input) {
  DWORD bytes_read = 0;
  if(ReadFile(
      win32_state->playback_handle,
      new_input,
      sizeof(*new_input),
      &bytes_read,
      0
  )) {
    // All of the input was read. Go back to the beginning.
    if (bytes_read == 0) {
      int playing_index = win32_state->input_playback_index;
      win32_end_playback_input(win32_state);
      win32_begin_playback_input(win32_state, playing_index);
    }
  };
}


DEBUG_PLATFORM_FREE_FILE_MEMORY(dbg_platform_free_file_memory) {
  if(memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(dbg_platform_read_entire_file) {
  DebugFileReadResult result = {};

  HANDLE file_handle = CreateFileA(
    file_name, 
    GENERIC_READ,
    FILE_SHARE_READ,
    0,
    OPEN_EXISTING,
    0,
    0
  );

  if (file_handle == INVALID_HANDLE_VALUE) {
    // TODO - Log error
    return result;
  }

  LARGE_INTEGER file_size;

  if(!GetFileSizeEx(file_handle, &file_size)) {
    // TODO - Log error
    return result;
  }

  u32 file_size_32 = u64_safe_truncate_to_u32(file_size.QuadPart);
  result.contents = VirtualAlloc(
    0, 
    file_size_32, 
    MEM_RESERVE | MEM_COMMIT, 
    PAGE_READWRITE
  );

  if (!result.contents) {
    // TODO - Log error
    return result;
  }

  DWORD bytes_read;

  // ReadFile's 3rd arg is a u32. If the QuadPart is > 4GB, then it'll fail, but this is unlikely
  if (ReadFile(file_handle, result.contents, file_size_32, &bytes_read, 0) && file_size_32 == bytes_read) {
    // TODO - implement success file read
    result.contents_size = (u32)file_size.QuadPart;
  } else {
    dbg_platform_free_file_memory(thread_ctx, result.contents);
    result.contents = 0;
  }

  CloseHandle(file_handle);

  return(result);
}


DEBUG_PLATFORM_WRITE_ENTIRE_FILE(dbg_platform_write_entire_file) {
  bool result = false;

  HANDLE file_handle = CreateFileA(
    file_name, 
    GENERIC_WRITE,
    FILE_SHARE_READ,
    0,
    CREATE_ALWAYS,
    0,
    0
  );

  if (file_handle != INVALID_HANDLE_VALUE) {
    DWORD bytes_written;
    if (WriteFile(
      file_handle,
      memory,
      memory_size,
      &bytes_written,
      0
    )) {
      result = bytes_written == memory_size;
    }
    else {
      // TODO - log error
    }

    CloseHandle(file_handle);
  }
  else {
    // TODO - log error
  }

  return(result);
}


// TODO - Figure out how to deal with passing const char * string literals
// into functions
inline FILETIME win32_get_last_write_time(char *file_path) {
  FILETIME last_write_time = {};

  WIN32_FILE_ATTRIBUTE_DATA file_data;
  if (GetFileAttributesEx(file_path, GetFileExInfoStandard, &file_data)) {
    last_write_time = file_data.ftLastWriteTime;
  }

  return last_write_time;
}

static Win32GameCode win32_load_game_code(char *source_dll_name, char *temp_dll_name) {
  Win32GameCode function_pointers = {};
  CopyFile(source_dll_name, temp_dll_name, FALSE);
  function_pointers.dll = LoadLibraryA(temp_dll_name);
  function_pointers.last_write_time = win32_get_last_write_time(source_dll_name);

  if (function_pointers.dll) {
    function_pointers.update_and_render = 
      (PtrGameUpdateAndRender *)GetProcAddress(function_pointers.dll, "game_update_and_render");
    function_pointers.get_sound_samples = 
      (PtrGameGetSoundSamples *)GetProcAddress(function_pointers.dll, "game_get_sound_samples");

    function_pointers.is_valid = (
        function_pointers.update_and_render &&
        function_pointers.get_sound_samples
    );
  }

  // Default to stub functions on failure
  if (!function_pointers.is_valid) {
    function_pointers.update_and_render = game_update_and_render_stub;
    function_pointers.get_sound_samples = game_get_sound_samples_stub;
  }

  return function_pointers;
}

// Unloads the live-loaded game code dll
static void win32_unload_game_code(Win32GameCode *game_code) {
  if (game_code->dll) {
    FreeLibrary(game_code->dll);
    game_code->dll = 0;
  }

  game_code->is_valid = false;
  game_code->update_and_render = game_update_and_render_stub;
  game_code->get_sound_samples = game_get_sound_samples_stub;
}

static void win32_load_xinput(void) {
  HMODULE xinput_lib = LoadLibraryA("xinput1_3.dll");
  if (xinput_lib) {
    XInputGetState = (x_input_get_state *)GetProcAddress(xinput_lib, "XInputGetState");
    XInputSetState = (x_input_set_state *)GetProcAddress(xinput_lib, "XInputSetState");
  }
}


static void win32_process_keyboard_message(
    GameButtonState *new_button_state,
    bool is_down
) {
  new_button_state->ended_down = is_down;
  ++new_button_state->half_transition_count;
}


static void win32_process_x_input_button(
    DWORD x_input_button_state,
    DWORD button_bit, 
    GameButtonState *old_button_state,
    GameButtonState *new_button_state
) {
  new_button_state->ended_down = (x_input_button_state & button_bit) == button_bit;
  new_button_state->half_transition_count = 
    (old_button_state->ended_down != new_button_state->ended_down) ? 1 : 0;
}


/* Zeroes out the sound buffer */
static void Win32ClearSoundBuffer(Win32SoundOutput *sound_output) {
	VOID *region1;
	DWORD region1_size;
	VOID *region2;
	DWORD region2_size;

	HRESULT buffer_lock_result = GlobalSecondarySoundBuffer->Lock(
			0, sound_output->secondary_buffer_size,
			&region1, &region1_size,
			&region2, &region2_size,
			0
	);

	if(SUCCEEDED(buffer_lock_result)) {
		u8 *destination_sample = (u8 *)region1;

		for(DWORD byte_index = 0; byte_index < region1_size; ++byte_index) {
			*destination_sample++ = 0;
		}
		
    destination_sample = (u8 *)region2;

		for(DWORD byte_index = 0; byte_index < region2_size; ++byte_index) {
			*destination_sample++ = 0;
		}
		
    GlobalSecondarySoundBuffer->Unlock(
			region1, region1_size,
			region2, region2_size
		);
	}
}



static void Win32FillSoundBuffer(
    Win32SoundOutput *sound_output, 
    DWORD byte_to_lock, // This is essentially the write pointer
    DWORD bytes_to_write,
    GameSoundOutputBuffer *source_buffer
) {
	VOID *region1;
	DWORD region1_size;
	VOID *region2;
	DWORD region2_size;


  // When you lock the circiular secondary sound buffer, Windows returns either one
  // or two chunks of memory. If the locked region hits the "end" of the circular buffer
  // and wraps around to the "beginning", you'll get two regions. Otherwise you're locking
  // a region without wrapping.
	HRESULT buffer_lock_result = GlobalSecondarySoundBuffer->Lock(
			byte_to_lock,
			bytes_to_write,
			&region1, &region1_size,
			&region2, &region2_size,
			0
	);

	if(SUCCEEDED(buffer_lock_result)) {
		DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;

		i16 *destination_sample = (i16 *)region1;
    i16 *source_sample = source_buffer->samples;

		for(DWORD sample_index = 0; sample_index < region1_sample_count; ++sample_index) {
			*destination_sample++ = *source_sample++;
			*destination_sample++ = *source_sample++;

			++sound_output->running_sample_index;
		}
		

		DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
		destination_sample = (i16 *)region2;

		for(DWORD sample_index = 0; sample_index < region2_sample_count; ++sample_index) {
			*destination_sample++ = *source_sample++;
			*destination_sample++ = *source_sample++;

			++sound_output->running_sample_index;
		}

		GlobalSecondarySoundBuffer->Unlock(
			region1, region1_size,
			region2, region2_size
		);
	}
}


// DIRECTSOUND pointer is written into as an OUT param as the second parameter of
// DirectSoundCreate().
static void Win32InitDirectSound(HWND window, i32 samples_per_second, i32 buffer_size) {
	LPDIRECTSOUND direct_sound;

	if(SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0))) {
		WAVEFORMATEX wave_format = {};
		wave_format.wFormatTag = WAVE_FORMAT_PCM;
		wave_format.nChannels = 2;
		wave_format.wBitsPerSample = 16;
		wave_format.nSamplesPerSec = samples_per_second;
		wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
		wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
		wave_format.cbSize = 0;

		if(!SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
			LPDIRECTSOUNDBUFFER primary_buffer;

			DSBUFFERDESC buffer_description = {};
			buffer_description.dwSize = sizeof(buffer_description);
			buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;

			if(SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &primary_buffer, 0))) {

				if(SUCCEEDED(primary_buffer->SetFormat(&wave_format))) {
					// Primary buffer is now set up.

				} else { 
					// TODO - diagnostic
				}

			
			} else {
				// TODO - diagnostic
			}

		} else {
			// TODO - failed to set direct sound buffer format
		}

		DSBUFFERDESC secondary_buffer_description = {};
		secondary_buffer_description.dwSize = sizeof(secondary_buffer_description);
		secondary_buffer_description.dwFlags = 0;
		secondary_buffer_description.dwBufferBytes = buffer_size;
		secondary_buffer_description.lpwfxFormat = &wave_format;

		HRESULT secondary_buffer_result = direct_sound->CreateSoundBuffer(
				&secondary_buffer_description,
				&GlobalSecondarySoundBuffer,
				0
			);
		
		if(SUCCEEDED(secondary_buffer_result)) {

		} else {
			// TODO - diagnostic
		}
	} else {
		// TODO - failed to init direct sound
	}
}


Win32WindowDimension GetWindowDimension(HWND window) {
	Win32WindowDimension result;

	RECT client_rect;
	GetClientRect(window, &client_rect);
	result.width = client_rect.right - client_rect.left;
	result.height = client_rect.bottom - client_rect.top;

	return(result);
};


// Resize Device Independent BitMap Section
static void Win32ResizeDIBSection(Win32OffScreenBuffer *buffer, int width, int height) {

	if (buffer->memory) {
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	} 

	buffer->width = width;
	buffer->height = height;
	buffer->bytes_per_pixel = 4;
	
	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height; // Make this negative so the bitmap origin is top left
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32; // Adding for 32 bits even though we only need 24 for RGB
	buffer->info.bmiHeader.biCompression = BI_RGB;

	int bitmap_memory_size = (buffer->width * buffer->height) * buffer->bytes_per_pixel;
	buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = width * buffer->bytes_per_pixel;
}

static void Win32DisplayBufferInWindow(
		Win32OffScreenBuffer *buffer,
		HDC device_context, 
		int window_width,
		int window_height
) {
	StretchDIBits(
		device_context,
		0, 0, buffer->width, buffer->height,
		0, 0, buffer->width, buffer->height,
		buffer->memory,
		&buffer->info,
		DIB_RGB_COLORS,
		SRCCOPY
	);
}


LRESULT CALLBACK  Win32MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{
	LRESULT Result = 0;

	switch(Message) {
		case WM_SIZE: {
		} break;
		case WM_DESTROY: {
			Running = false;
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		case WM_CLOSE: {
			Running = false;
			OutputDebugStringA("WM_CLOSE\n");
		} break;
		case WM_ACTIVATEAPP: {
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
      // Might want to assert that keyboard input came in through a non-standard event
      // so the following code can be moved.
		} break;
		case WM_PAINT: {
			PAINTSTRUCT Paint;
			HDC device_context = BeginPaint(Window, &Paint);

			Win32WindowDimension dimension = GetWindowDimension(Window);
			Win32DisplayBufferInWindow(
				&GlobalBackBuffer,
				device_context, 
				dimension.width,
				dimension.height
			);
			
			EndPaint(Window, &Paint);
		} break;
		default: { 
			Result = DefWindowProcA(Window, Message, WParam, LParam);
		} break;
	}

	return(Result);
};

static void win32_process_pending_messages(Win32State *win32_state, GameControllerInput *keyboard_controller) {
  MSG message;

  while(PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
    switch(message.message) {
      case WM_QUIT: {
        Running = false;
        break;
      }
      case WM_SYSKEYDOWN:
      case WM_SYSKEYUP:
      case WM_KEYDOWN:
      case WM_KEYUP: {
        u32 keycode = (u32)message.wParam;
        bool was_down = ((message.lParam & (1 << 30)) != 0); // Get bit 30 of LParam
        bool is_down = ((message.lParam & (1 << 31)) == 0); // Get bit 31 of LParam

        if (was_down != is_down){
          if (keycode == 'W') {
            win32_process_keyboard_message(
              &keyboard_controller->move_up,
              is_down
            );
          } else if (keycode == 'A') {
            win32_process_keyboard_message(
              &keyboard_controller->move_left,
              is_down
            );
          } else if (keycode == 'S') {
            win32_process_keyboard_message(
              &keyboard_controller->move_down,
              is_down
            );
          } else if (keycode == 'D') {
            win32_process_keyboard_message(
              &keyboard_controller->move_right,
              is_down
            );
          } else if (keycode == 'Q') {
            win32_process_keyboard_message(
              &keyboard_controller->left_shoulder,
              is_down
            );
          } else if (keycode == 'E') {
            win32_process_keyboard_message(
              &keyboard_controller->right_shoulder,
              is_down
            );
          } else if (keycode == VK_UP) {
            win32_process_keyboard_message(
              &keyboard_controller->action_up,
              is_down
            );
          } else if (keycode == VK_LEFT) {
            win32_process_keyboard_message(
              &keyboard_controller->action_left,
              is_down
            );
          } else if (keycode == VK_DOWN) {
            win32_process_keyboard_message(
              &keyboard_controller->action_down,
              is_down
            );
          } else if (keycode == VK_RIGHT) {
            win32_process_keyboard_message(
              &keyboard_controller->action_right,
              is_down
            );
          } else if (keycode == VK_ESCAPE) {
            Running = false;
          } else if (keycode == VK_SPACE) {
          } else if (keycode == 'L') {
            if (is_down) {
              if (win32_state->input_recording_index == 0) {
                win32_begin_recording_input(win32_state, 1);
              } else {
                win32_end_recording_input(win32_state);
                win32_begin_playback_input(win32_state, 1);
              }
            }
          }
        }

        // Check for Alt+ F4 exit code
        bool alt_key_was_down = (message.lParam & (1 << 29));
        if ((keycode == VK_F4) && alt_key_was_down)
        {
          Running = false;
        }
      } break;

      default: {
        TranslateMessage(&message);	
        DispatchMessageA(&message);
        break;
      }
    }
  }
}



// Draws a vertical tic-mark at the x position between two y positions in the buffer.
// Kind of like a line, but not exactly.
static void win32_debug_draw_vertical(
    Win32OffScreenBuffer *back_buffer, 
    int x, 
    int top, 
    int bottom,
    u32 color
  ) {
  if (top <= 0) {
    top = 0;
  }
  
  if (bottom >= back_buffer->height) {
    bottom = back_buffer->height;
  }

  if (x >= 0 && x < back_buffer->width) {
    uint8_t *pixel =  (uint8_t *)back_buffer->memory + 
        (x * back_buffer->bytes_per_pixel) + 
        (top * back_buffer->pitch);

    for(int y = top; y < bottom; ++y) 
    {
      *(uint32_t *)pixel = color; 
      pixel += back_buffer->pitch;
    }
  }
}

// Draws a line for a sound cursor at a given x.
inline void win32_draw_sound_buffer_line(
  Win32OffScreenBuffer *back_buffer,
  Win32SoundOutput *sound_output,
  float coefficient,
  int pad_x, 
  int top,
  int bottom,
  DWORD cursor_pos,
  u32 color
) {
    f32 x = coefficient * (f32)cursor_pos;
    int x_with_pad = pad_x + (int)x;
    win32_debug_draw_vertical(back_buffer, x_with_pad, top, bottom, color);
}

// Draw lines where sound play and write cursors occur
static void win32_debug_sync_display(
  Win32OffScreenBuffer *back_buffer,
  int sound_cursor_count,
  int current_cursor_index,
  Win32DebugSoundCursor *debug_sound_cursors,
  Win32SoundOutput *sound_output,
  f32 target_seconds_per_frame
) {

  int pad_x = 16;
  int pad_y = 16;
  int line_height = 64;

  // Maps sound buffer position to video pixels with some padding 
  f32 coefficient = (f32)(back_buffer->width - 2 * pad_x) / (f32)sound_output->secondary_buffer_size;

  for(int sound_cursor_idx = 0; sound_cursor_idx < sound_cursor_count; ++sound_cursor_idx) 
  {
    Win32DebugSoundCursor *current_sound_cursor = &debug_sound_cursors[sound_cursor_idx];

    Assert(current_sound_cursor->output_play_cursor < sound_output->secondary_buffer_size);
    Assert(current_sound_cursor->output_write_cursor< sound_output->secondary_buffer_size);
    Assert(current_sound_cursor->output_location < sound_output->secondary_buffer_size);
    Assert(current_sound_cursor->output_byte_count < sound_output->secondary_buffer_size);
    Assert(current_sound_cursor->flip_play_cursor < sound_output->secondary_buffer_size);
    Assert(current_sound_cursor->flip_write_cursor < sound_output->secondary_buffer_size);

    DWORD play_color = 0xFFFFFFFF;
    DWORD write_color = 0xFFFF0000;
    DWORD expected_frame_clip_color = 0xFFFFFF00;

    int top = pad_y;
    int bottom = pad_y + line_height;
    
    if (sound_cursor_idx == current_cursor_index) {
      top += pad_y + line_height;
      bottom += pad_y + line_height;
      int first_top = top;

      win32_draw_sound_buffer_line(
          back_buffer, 
          sound_output, 
          coefficient, 
          pad_x, 
          top, 
          bottom, 
          current_sound_cursor->output_play_cursor,
          play_color
      );

      win32_draw_sound_buffer_line(
          back_buffer, 
          sound_output, 
          coefficient, 
          pad_x, 
          top, 
          bottom, 
          current_sound_cursor->output_write_cursor,
          write_color
      );

      top += pad_y + line_height;
      bottom += pad_y + line_height;

      win32_draw_sound_buffer_line(
          back_buffer, 
          sound_output, 
          coefficient, 
          pad_x, 
          top, 
          bottom, 
          current_sound_cursor->output_location,
          play_color
      );

      win32_draw_sound_buffer_line(
          back_buffer, 
          sound_output, 
          coefficient, 
          pad_x, 
          top, 
          bottom, 
          current_sound_cursor->output_location + current_sound_cursor->output_byte_count,
          write_color
      );
      
      top += pad_y + line_height;
      bottom += pad_y + line_height;

      win32_draw_sound_buffer_line(
          back_buffer, 
          sound_output, 
          coefficient, 
          pad_x, 
          first_top, 
          bottom, 
          current_sound_cursor->expected_flip_play_cursor,
          expected_frame_clip_color
      );
    }
       
    win32_draw_sound_buffer_line(
        back_buffer, 
        sound_output, 
        coefficient, 
        pad_x, 
        top, 
        bottom, 
        current_sound_cursor->flip_play_cursor,
        play_color
    );

    win32_draw_sound_buffer_line(
        back_buffer, 
        sound_output, 
        coefficient, 
        pad_x, 
        top, 
        bottom, 
        current_sound_cursor->flip_write_cursor,
        write_color
    );
  }
}

// Map the thumbstick values [-1..1] and account for deadzone. Values inside the (SQUARE) deadzone
// are not included in the map. Once outside the deadzone, the values will go -1..0 and 0..1.
// Otherwise including the deadzone in the map would mean values would start at 0 and then jump
// to something like 0.2 once the deadzone is exited.
static f32 win32_process_input_stick_value(SHORT thumbstick, f32 deadzone_threshold) {
  f32 result = 0;
  if(thumbstick < -deadzone_threshold) {
    result = (f32)((f32)thumbstick + deadzone_threshold) / (32768.0f - deadzone_threshold);
  } else if (thumbstick > deadzone_threshold) {
    result = (f32)((f32)thumbstick + deadzone_threshold) / (32767.0f - deadzone_threshold);
  }

  return result;
}


static float win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
  f32 delta = (f32)(end.QuadPart - start.QuadPart);
  f32 elapsed_seconds = delta / (f32)GlobalPerfCounterFrequency;
  return elapsed_seconds;
}

static LARGE_INTEGER win32_get_wall_clock(void) {
  LARGE_INTEGER counter;
  QueryPerformanceCounter(&counter);
  return counter;
}

static void concat_strings(
    int source_one_count, char *source_one,
    int source_two_count, char *source_two,
    int destination_count, char *destination)
{
  for (int index = 0; index < source_one_count; ++index) {
    *destination++ = *source_one++;
  }
  
  for (int index = 0; index < source_two_count; ++index) {
    *destination++ = *source_two++;
  }

  *destination++ = 0;
}


int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow
) {
  // TODO - MAX_PATH is dangerous and shouldn't be used in production code.
  // Get path of where this executable is running
  char exe_file_name[MAX_PATH];
  DWORD size_of_filename = GetModuleFileNameA(0, exe_file_name, sizeof(exe_file_name));
  char *one_past_last_slash = exe_file_name;
  for (char *scan = exe_file_name; *scan; ++scan) {
    if (*scan == '\\') {
      one_past_last_slash = scan + 1;
    }
  }

  win32_load_xinput();

  char source_game_code_dll_filename[] = "handmade.dll";
  char source_game_code_dll_full_path[MAX_PATH];
  concat_strings(
      one_past_last_slash - exe_file_name, 
      exe_file_name,
      sizeof(source_game_code_dll_filename) - 1,
      source_game_code_dll_filename,
      sizeof(source_game_code_dll_full_path),
      source_game_code_dll_full_path
  );
  
  char temp_game_code_dll_filename[] = "handmade_temp.dll";
  char temp_game_code_dll_full_path[MAX_PATH];
  concat_strings(
      one_past_last_slash - exe_file_name, 
      exe_file_name,
      sizeof(temp_game_code_dll_filename) - 1,
      temp_game_code_dll_filename,
      sizeof(temp_game_code_dll_full_path),
      temp_game_code_dll_full_path
  );

	LARGE_INTEGER perf_counter_frequency_result;
	QueryPerformanceFrequency(&perf_counter_frequency_result);
  GlobalPerfCounterFrequency = perf_counter_frequency_result.QuadPart;

  // set OS thread sleep duration to 1ms
  bool sleep_is_granular = timeBeginPeriod(1) == TIMERR_NOERROR;

	WNDCLASSA WindowClass = {};
	ThreadContext thread_ctx = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_CLASSDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";


	/* takes pointer to WindowClass */
	if (RegisterClass(&WindowClass)) {
		HWND window = CreateWindowEx(
			0,
			WindowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0
		);

		if (window) {
		  HDC refresh_dc= GetDC(window);
      int monitor_refresh_rate = 60;
      int win32_refresh_rate = GetDeviceCaps(refresh_dc, VREFRESH);

      if (win32_refresh_rate > 1) {
        monitor_refresh_rate = win32_refresh_rate;
      }

    // These have to be defines because it is eventually used in sizing an array
      f32 game_update_hz = (f32)(monitor_refresh_rate / 2.0f);
      f32 target_seconds_per_frame = 1.0f / (f32)game_update_hz;

      ReleaseDC(window, refresh_dc);

			char refresh_rate_string_buffer[256];
			_snprintf_s(
				refresh_rate_string_buffer, 
        sizeof(refresh_rate_string_buffer),
				"%dhz is the monitor refresh rate\n", 
				monitor_refresh_rate
			);
			OutputDebugStringA(refresh_rate_string_buffer);
      

		  HDC device_context = GetDC(window);

			Win32SoundOutput sound_output = {};
			sound_output.samples_per_second = 48000;
			sound_output.running_sample_index = 0;
			sound_output.bytes_per_sample = sizeof(i16) * 2; // Two channel audio
      sound_output.safety_bytes = (int)(((f32)sound_output.samples_per_second * (f32)sound_output.bytes_per_sample / game_update_hz) / 1.0f);
			sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
      sound_output.latency_sample_count = sound_output.samples_per_second / game_update_hz;

			Win32InitDirectSound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
      Win32ClearSoundBuffer(&sound_output);
			GlobalSecondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

      i16 *samples = (i16 *)VirtualAlloc(
        0, 
        sound_output.secondary_buffer_size,
        MEM_RESERVE|MEM_COMMIT, 
        PAGE_READWRITE
      );
				
      Win32State win32_state = {};
      win32_state.input_recording_index = 0;
      win32_state.input_playback_index = 0;

			Running = true;

      // Initialize game memory
      //LPVOID base_game_memory_address = 2 * 1024 * 1024 * 1024 * 1024; // 2 TB
      LPVOID base_game_memory_address = 0;

      GameMemory game_memory = {};
      game_memory.permanent_storage_size = 64 * 1024 * 1024; // 64MB
      game_memory.transient_storage_size = (u64)4 * 1024 * 1024 * 1024; // 4GB
      game_memory.dbg_platform_free_file_memory = dbg_platform_free_file_memory;
      game_memory.dbg_platform_read_entire_file = dbg_platform_read_entire_file;
      game_memory.dbg_platform_write_entire_file = dbg_platform_write_entire_file;
      
      win32_state.game_memory_total_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;

      win32_state.game_memory = VirtualAlloc(
        base_game_memory_address, 
        (size_t)win32_state.game_memory_total_size,
        MEM_RESERVE|MEM_COMMIT, 
        PAGE_READWRITE
      );

      game_memory.permanent_storage = win32_state.game_memory;

      // TODO: Figure out transient_storage
      /*
       * Need to get this working. Unsure of game memory struct changes.
      game_memory.transient_storage = ((uint8_t)game_memory.permanent_storage +
          game_memory.permanent_storage_size);
      */

      /*
      win32_state.game_memory.transient_storage = VirtualAlloc(
        0, 
        win32_state.game_memory.transient_storage_size, 
        MEM_RESERVE|MEM_COMMIT, 
        PAGE_READWRITE
      );
      */

      // Initialize sound cursor tracking
      int debug_sound_cursor_idx = 0;
      Win32DebugSoundCursor debug_sound_cursors[30] = {0};
      DWORD audio_latency_bytes = 0;
      float audio_latency_seconds = 0.0f;
      bool sound_is_valid = false;

      // Initialize Controllers
      GameInput game_input[2] = {};
      GameInput *new_input = &game_input[0];
      GameInput *old_input = &game_input[1];

      // Load function pointers for live-loading game code.
      //char *source_dll_name = "./build/handmade.dll";
      Win32GameCode game_code = win32_load_game_code(
          source_game_code_dll_full_path,
          temp_game_code_dll_full_path
      );

      // Performance counting
      LARGE_INTEGER last_counter = win32_get_wall_clock();
			u64 last_cycle_count = __rdtsc();
      LARGE_INTEGER frame_wall_clock = win32_get_wall_clock();

			while(Running) {

        FILETIME new_dll_write_time = win32_get_last_write_time(source_game_code_dll_full_path);

        if (CompareFileTime(&new_dll_write_time, &game_code.last_write_time) != 0) {
          win32_unload_game_code(&game_code);
          game_code = win32_load_game_code(
              source_game_code_dll_full_path,
              temp_game_code_dll_full_path
          );
        }

        GameControllerInput *old_keyboard_controller = get_controller(old_input, 0);
        GameControllerInput *new_keyboard_controller = get_controller(new_input, 0);
        GameControllerInput zeroed_controller = {};
        *new_keyboard_controller = zeroed_controller;
        new_keyboard_controller->is_connected = true;

        for(int button_idx = 0; 
            button_idx < ArrayCount(new_keyboard_controller->buttons);
            ++button_idx) 
        {
          new_keyboard_controller->buttons[button_idx].ended_down =  
            old_keyboard_controller->buttons[button_idx].ended_down;
        }

        win32_process_pending_messages(&win32_state, new_keyboard_controller);

        int max_controller_count = XUSER_MAX_COUNT;
        if(max_controller_count > (ArrayCount(new_input->controllers) - 1)) {
          max_controller_count = ArrayCount(new_input->controllers) - 1;
        }

				for(DWORD controller_idx = 0; controller_idx < max_controller_count; ++controller_idx) 
				{
				  // keyboard is controller_idx 0, so add 1 for processing all other inputs
          DWORD our_controller_idx = controller_idx + 1;
          GameControllerInput *old_controller = get_controller(old_input, our_controller_idx);
          GameControllerInput *new_controller = get_controller(new_input, our_controller_idx);


					XINPUT_STATE controller_state;

				  // indicates the controller is plugged-in
					if (XInputGetState(controller_idx, &controller_state) == ERROR_SUCCESS) {

						XINPUT_GAMEPAD *gamepad = &controller_state.Gamepad;

            new_controller->is_connected = true;
            new_controller->is_analog = old_controller->is_analog;

            new_controller->avg_stick_x = win32_process_input_stick_value(
              gamepad->sThumbLX,
              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
            );
            new_controller->avg_stick_y = win32_process_input_stick_value(
              gamepad->sThumbLY,
              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
            );

            if ((new_controller->avg_stick_x != 0.0f) || (new_controller->avg_stick_y != 0.0f)) {
              new_controller->is_analog = true;
            }


            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
              new_controller->avg_stick_y = 1.0f;
              new_controller->is_analog = false;
            }
            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
              new_controller->avg_stick_y = -1.0f;
              new_controller->is_analog = false;
            }
            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
              new_controller->avg_stick_x = -1.0f;
              new_controller->is_analog = false;
            }
            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
              new_controller->avg_stick_x = 1.0f;
              new_controller->is_analog = false;
            }

            f32 threshold = 0.5f;
            win32_process_x_input_button(
              (new_controller->avg_stick_x < -threshold) ? 1 : 0,
              1,
              &old_controller->move_left,
              &new_controller->move_left
            );
            win32_process_x_input_button(
              (new_controller->avg_stick_x > threshold) ? 1 : 0,
              1,
              &old_controller->move_right,
              &new_controller->move_right
            );
            win32_process_x_input_button(
              (new_controller->avg_stick_y < -threshold) ? 1 : 0,
              1,
              &old_controller->move_up,
              &new_controller->move_up
            );
            win32_process_x_input_button(
              (new_controller->avg_stick_y > threshold) ? 1 : 0,
              1,
              &old_controller->move_down,
              &new_controller->move_down
            );

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_A,
              &old_controller->action_down,
              &new_controller->action_down
            );
            
            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_B,
              &old_controller->action_right,
              &new_controller->action_right
            );
            
            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_X,
              &old_controller->action_left,
              &new_controller->action_left
            );

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_Y,
              &old_controller->action_up,
              &new_controller->action_up
            );
            
            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_LEFT_SHOULDER,
              &old_controller->left_shoulder,
              &new_controller->left_shoulder
            );

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_RIGHT_SHOULDER,
              &old_controller->right_shoulder,
              &new_controller->right_shoulder
            );

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_START,
              &old_controller->start,
              &new_controller->start
            );

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_BACK,
              &old_controller->back,
              &new_controller->back
            );

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_RIGHT_SHOULDER,
              &old_controller->right_shoulder,
              &new_controller->right_shoulder
            );
					} else {
            new_controller->is_connected = false;
					}
				}

        /*
        // Setting Controller Vibration for testing
				XINPUT_VIBRATION Vibration;
				Vibration.wLeftMotorSpeed = 60000;
				Vibration.wRightMotorSpeed = 60000;
				XInputSetState(0, &Vibration);
        */

        
        // Initialize screen buffer for the next frame
				GameOffScreenBuffer game_offscreen_buffer = {};
				game_offscreen_buffer.memory = GlobalBackBuffer.memory;
				game_offscreen_buffer.width =  GlobalBackBuffer.width;
				game_offscreen_buffer.height = GlobalBackBuffer.height;
				game_offscreen_buffer.pitch = GlobalBackBuffer.pitch;
				game_offscreen_buffer.bytes_per_pixel = GlobalBackBuffer.bytes_per_pixel;

        if (win32_state.input_recording_index) {
          win32_record_input(&win32_state, new_input);
        }

        if (win32_state.input_playback_index) {
          // Overwrites input from previous stream
          win32_playback_input(&win32_state, new_input);
        }

				game_code.update_and_render(&thread_ctx, &game_memory, new_input, &game_offscreen_buffer);

       
        /*
            Audio latency
            ---------------
            Low Latency Audio =)
              The write cursor falls within the leftover frame time.
              Move audio write up to the frame boundary and write one frame's worth of audio.
              We will have to check how much the write cursor has moved every frame
              Basically always writing one frame or average WC amount

              Round up to frame boundary from where write cursor is projected.
              Find where write cursor is. If it is inside the frame boundary - there is time left before the frame ends.
              Project the write cursor forward with wc + samples per frame.
              The target is the next expected frame boundary.


              ___CURRENT FRAME____|
                              ~~~~~~~~WRITE AUDIO~~~~~~|
                                   __FOLLOWING FRAME___|
                                                 ~~~~~~~~~WRITE NEXT AUDIO~~|
                                                 |-OVW-| (overwrite - Play cursor hasn't gotten here yet)

              |-------P------W----|------P2------W2----|--------------------|---->
              
              

            High Latency Audio =(
              The write cursor will fall in the following frame which we aren't yet computing.
              We will write up to the next frame and then also to where we EXPECT the next WC to be.
              Add some safety margin past that because the next WC may be a little earlier or a little later.
              WC + frame samples + safety margin (probably a few samples 1-2ms)

              ___CURRENT FRAME____|
                                    ~~~~~~~~~~~~~~~~~~~|~~~SM~~~| (write up to the next frame plus some safety margin incase the next WC doesn't fall exactly where we expect)

                                   __FOLLOWING FRAME___|
                                                          |~~OVW~~~~~~~~~~~~|~~~SM~~~|

              |-------P-----------|--W---P2------------|--W2----------------|---------------->

            
        */
        LARGE_INTEGER audio_wall_clock = win32_get_wall_clock();
        f32 frame_begin_to_audio_seconds_delta = win32_get_seconds_elapsed(frame_wall_clock, audio_wall_clock);

        DWORD play_cursor;
        DWORD write_cursor;
        HRESULT sound_buffer_position_result = GlobalSecondarySoundBuffer->GetCurrentPosition(
          &play_cursor,
          &write_cursor
        );

        if (SUCCEEDED(sound_buffer_position_result)) {

          if (!sound_is_valid) {
            sound_output.running_sample_index = write_cursor / sound_output.bytes_per_sample;
            sound_is_valid = true;
          }

					DWORD byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % 
              sound_output.secondary_buffer_size;

          DWORD expected_sound_bytes_per_frame = (DWORD)(((f32)sound_output.samples_per_second *
            (f32)sound_output.bytes_per_sample) / game_update_hz);

          f32 seconds_left_until_frame_end = target_seconds_per_frame - frame_begin_to_audio_seconds_delta;
          if (seconds_left_until_frame_end < 0.0) {
            // DWORD is a u32. When the window is clicked and dragged, this is causing the seconds_left_until_frame_end
            // to go negative. After the computation is complete, the cast to DWORD is an invalid operation.
            // clamp to 0.0 for now....
            seconds_left_until_frame_end = 0.0;
          }

          DWORD expected_bytes_until_frame_end =
            (DWORD)((seconds_left_until_frame_end / target_seconds_per_frame) * (f32)expected_sound_bytes_per_frame);

          // Add buffer size when write cursor wraps around and is behind the play cursor
          DWORD unwrapped_write_cursor = write_cursor;
          if (unwrapped_write_cursor < play_cursor) {
            unwrapped_write_cursor += sound_output.secondary_buffer_size;
          }

          audio_latency_bytes = unwrapped_write_cursor - play_cursor;
          f32 samples_between_cursors = (f32)audio_latency_bytes / (f32)sound_output.bytes_per_sample;
          audio_latency_seconds = samples_between_cursors / (f32)sound_output.samples_per_second;

          // TODO!!!!!
          // This expected_bytes_until_flip calculation is blowing up when declared as a DWORD and the screen is resizing.
          // Need to investigate this bug and better understand the sound code.
          // Setting it to 0 for now, because we weren't even using it.
          /*
          float expected_bytes_until_flip = 
              (seconds_left_until_flip / target_seconds_per_frame) * 
              (float)expected_sound_bytes_per_frame;
          */

          /*
          DWORD expected_bytes_until_flip = (DWORD)(
              (seconds_left_until_flip / target_seconds_per_frame) * 
              (float)expected_sound_bytes_per_frame);
          */

          DWORD expected_bytes_until_flip = 0;

          DWORD expected_frame_boundary_byte = play_cursor + expected_sound_bytes_per_frame;
          DWORD safe_write_cursor = unwrapped_write_cursor + sound_output.safety_bytes;
          bool audio_card_is_latent = safe_write_cursor >= expected_frame_boundary_byte;
          DWORD target_cursor = 0;

          if (audio_card_is_latent) {
            target_cursor = safe_write_cursor + expected_sound_bytes_per_frame;
          } else {
            target_cursor = expected_frame_boundary_byte + expected_sound_bytes_per_frame;
          }

          target_cursor = target_cursor % sound_output.secondary_buffer_size;

          DWORD bytes_to_write = 0;
					if(byte_to_lock > target_cursor) {
						bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
						bytes_to_write += target_cursor;
					} else {
						bytes_to_write = target_cursor - byte_to_lock;
					}

          // Set sample count based on frame-rate of the game to try and keep
          // sounds in sync with visuals
          GameSoundOutputBuffer sound_buffer = {};
          sound_buffer.samples_per_second = sound_output.samples_per_second;
          sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
          sound_buffer.samples = samples;
          game_code.get_sound_samples(&thread_ctx, &game_memory, &sound_buffer);

          // Debug Sound stuff
          Win32DebugSoundCursor *sound_cursor = &debug_sound_cursors[debug_sound_cursor_idx];
          sound_cursor->output_play_cursor = play_cursor;
          sound_cursor->output_write_cursor = write_cursor;
          sound_cursor->output_location = byte_to_lock;
          sound_cursor->output_byte_count = bytes_to_write;
          sound_cursor->expected_flip_play_cursor = expected_frame_boundary_byte;

          // DWORD unwrapped_write_cursor = write_cursor;

          // // Add buffer size when write cursor wraps around and is behind the play cursor
          // if (unwrapped_write_cursor < play_cursor) {
          //   unwrapped_write_cursor += sound_output.secondary_buffer_size;
          // }
          // audio_latency_bytes = unwrapped_write_cursor - play_cursor;
          // f32 samples_between_cursors = (f32)audio_latency_bytes / (f32)sound_output.bytes_per_sample;
          // audio_latency_seconds = samples_between_cursors / (f32)sound_output.samples_per_second;
          // End Debug Sound Stuff

          Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
        } else {
          sound_is_valid = false;
        }


        /*
          WAIT TIME
          --------
          This is the leftover time until the frame ends.
          Sleep if we can so we don't burn processor time.
        */
        LARGE_INTEGER work_counter = win32_get_wall_clock();
        f32 seconds_elapsed_for_work = win32_get_seconds_elapsed(last_counter, work_counter);
        f32 seconds_elapsed_for_frame = seconds_elapsed_for_work;

        if (seconds_elapsed_for_frame < target_seconds_per_frame) {
          while (seconds_elapsed_for_frame < target_seconds_per_frame) {
            if (sleep_is_granular) {
              DWORD sleep_ms = (DWORD)(1000.0f * (target_seconds_per_frame - seconds_elapsed_for_frame));

              if (sleep_ms > 0) {
                // Avoid burning cycles in a spin cycle
                Sleep(sleep_ms);
              }
            }
            while(seconds_elapsed_for_frame < target_seconds_per_frame) {
              seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
            }
          }
        } else {
          // TODO: Missed frame rate, need to LOG
        }

        
        
				// Close time window
        LARGE_INTEGER end_counter = win32_get_wall_clock();
        f32 ms_per_frame = 1000.0f * win32_get_seconds_elapsed(last_counter, end_counter);
				last_counter = end_counter;

        // Blit to screen after frame rate calculations
				Win32WindowDimension dimension = GetWindowDimension(window);

        win32_debug_sync_display(
          &GlobalBackBuffer,
          ArrayCount(debug_sound_cursors),
          debug_sound_cursor_idx - 1, // TODO: This is wrong when the current index is 0
          debug_sound_cursors,
          &sound_output,
          target_seconds_per_frame
        );

				Win32DisplayBufferInWindow(
					&GlobalBackBuffer,
					device_context, 
					dimension.width,
					dimension.height
				);
        
        // This marks the end of the frame
        frame_wall_clock = win32_get_wall_clock();

        //For debugging sound cursors
        sound_buffer_position_result = GlobalSecondarySoundBuffer->GetCurrentPosition(
          &play_cursor,
          &write_cursor
        );

        if (SUCCEEDED(sound_buffer_position_result)) {
          if (!sound_is_valid) {
            sound_output.running_sample_index = write_cursor / sound_output.bytes_per_sample;
            sound_is_valid = true;
          }
        } else {
          sound_is_valid = false;
        }

        Win32DebugSoundCursor *debug_sound_cursor = &debug_sound_cursors[debug_sound_cursor_idx];
        debug_sound_cursor->flip_play_cursor = play_cursor;
        debug_sound_cursor->flip_write_cursor = write_cursor;
        // End debugging sound cursors

        // Swap game inputs
        GameInput *temp = new_input;
        new_input = old_input;
        old_input = temp;

        int64_t end_cycle_count = __rdtsc();
				int64_t elapsed_cycles = end_cycle_count - last_cycle_count;
				last_cycle_count = end_cycle_count;
       
				//int32_t elapsed_millis = (int32_t)((1000 * elapsed_counter) / perf_counter_frequency.QuadPart);
        //int32_t fps = perf_counter_frequency.QuadPart / elapsed_counter;
        f32 fps = 0.0f;
				f64 mega_cycles_per_frame = (f64)(elapsed_cycles / (1000.0 * 1000.0));

				char string_buffer[256];
				_snprintf_s(
					string_buffer, 
          sizeof(string_buffer),
					"%.02fms/frame, %.02ffps, %.02fMc/f\n", 
					ms_per_frame,
					fps,
					mega_cycles_per_frame
				);
				OutputDebugStringA(string_buffer);

        // debug sound cursors
        ++debug_sound_cursor_idx;
        if (debug_sound_cursor_idx >= ArrayCount(debug_sound_cursors)) {
          debug_sound_cursor_idx = 0;
        }
        // end debug sound cursors
			}

		} else {
			// TODO: Handle Create Window Failure
		}
	} else {
		// TODO: Handle Failure of window creation
	}
	
	return 0;
}


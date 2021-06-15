#include <windows.h>
#include <stdint.h> // Bring in unit8_t to avoid casting to (unsigned char *)
#include <xinput.h> // Device input like joypads
#include <dsound.h>
#include <math.h>
#include "win32_handmade.h"

#define internal_function static
#define local_persist static
#define global_variable static
#define Pi32 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

#include "handmade.cpp"

/*
// NOTE: Not using this because I can't get zig to compile it and I'm using Windows 10...
//
// Create function pointers to xinput functions in case the linker
// can't find the required files on the version of Windows.
// We remap to try and trick the compiler to allow us to syntactically reference the function
// as if it were the lib version. We're still including the header file to get
// all the other definitions

#define X_INPUT_GET_STATE(name) WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return(0); }
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return(0); }
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_
*/

global_variable bool Running = false;
global_variable Win32OffScreenBuffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondarySoundBuffer;
global_variable int64_t GlobalPerfCounterFrequency;

static DebugFileReadResult
debug_platform_read_entire_file(char *file_name) {
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

  if (file_handle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER file_size;

    if(GetFileSizeEx(file_handle, &file_size)) {
      result.contents = VirtualAlloc(
        0, 
        file_size.QuadPart, 
        MEM_RESERVE | MEM_COMMIT, 
        PAGE_READWRITE
      );

      if (result.contents) {
        DWORD bytes_read;

        if (ReadFile(
          file_handle,
          result.contents,
          file_size.QuadPart,
          &bytes_read,
          0
        )) {
          // TODO - implement success file read
          result.contents_size = (uint32_t)file_size.QuadPart;
        }
        else {
          debug_platform_free_file_memory(result.contents);
          result.contents = 0;
        }
      }
      else {
        // TODO - log error
      }
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

static void 
debug_platform_free_file_memory(void *memory) {
  if(memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
  }
}

static bool 
debug_platform_write_entire_file(char *file_name, uint32_t memory_size, void *memory) {
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

static void 
win32_process_keyboard_message(
    GameButtonState *new_button_state,
    bool is_down
) {
  new_button_state->ended_down = is_down;
  ++new_button_state->half_transition_count;
}


static void 
win32_process_x_input_button(
    DWORD x_input_button_state,
    DWORD button_bit, 
    GameButtonState *old_button_state,
    GameButtonState *new_button_state
) {
  new_button_state->ended_down = (x_input_button_state & button_bit) == button_bit;
  new_button_state->half_transition_count = 
    (old_button_state->ended_down != new_button_state->ended_down) ? 1 : 0;
}


static void 
Win32ClearSoundBuffer(Win32SoundOutput *sound_output) {
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
		uint8_t *destination_sample = (uint8_t *)region1;

		for(DWORD byte_index = 0; byte_index < region1_size; ++byte_index) {
			*destination_sample++ = 0;
		}
		
    destination_sample = (uint8_t *)region2;

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
    DWORD byte_to_lock, 
    DWORD bytes_to_write,
    GameSoundOutputBuffer *source_buffer
) {
	VOID *region1;
	DWORD region1_size;
	VOID *region2;
	DWORD region2_size;

	HRESULT buffer_lock_result = GlobalSecondarySoundBuffer->Lock(
			byte_to_lock,
			bytes_to_write,
			&region1, &region1_size,
			&region2, &region2_size,
			0
	);

	if(SUCCEEDED(buffer_lock_result)) {
		DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;

		int16_t *destination_sample = (int16_t *)region1;
    int16_t *source_sample = source_buffer->samples;

		for(DWORD sample_index = 0; sample_index < region1_sample_count; ++sample_index) {
			*destination_sample++ = *source_sample++;
			*destination_sample++ = *source_sample++;

			++sound_output->running_sample_index;
		}
		

		DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
		destination_sample = (int16 *)region2;

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
static void Win32InitDirectSound(HWND window, int32 samples_per_second, int32 buffer_size) {
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
		0, 0, window_width, window_height,
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

static void
win32_process_pending_messages(GameControllerInput *keyboard_controller) {
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
        uint32_t keycode = (uint32_t)message.wParam;
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

static float
win32_process_input_stick_value(SHORT thumbstick, float deadzone_threshold) {
  float result = 0;
  if(thumbstick < -deadzone_threshold) {
    result = (float)thumbstick / 32768.0f;
  } else if (thumbstick > deadzone_threshold){
    result = (float)thumbstick / 32767.0f;
  }

  return result;
}


static float
win32_get_seconds_elapsed(LARGE_INTEGER start, LARGE_INTEGER end) {
  float delta = (float)end.QuadPart - (float)start.QuadPart;
  float seconds_elapsed_for_work = delta / (float)GlobalPerfCounterFrequency;
  return seconds_elapsed_for_work;
}

static LARGE_INTEGER
win32_get_wall_clock(void) {
  LARGE_INTEGER end_counter;
  QueryPerformanceCounter(&end_counter);
  return end_counter;
}

int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow
) {
	LARGE_INTEGER perf_counter_frequency_result;
	QueryPerformanceFrequency(&perf_counter_frequency_result);
  GlobalPerfCounterFrequency = perf_counter_frequency_result.QuadPart;

  // set OS thread sleep duration to 1ms
  bool sleep_is_granular = timeBeginPeriod(1) == TIMERR_NOERROR;

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_CLASSDC | CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  // Fixed frame timing: 30fps
  int monitor_refresh_rate = 60;
  int game_update_hz = monitor_refresh_rate / 2;
  float target_seconds_elapsed_per_frame = 1.0f / (float)game_update_hz;

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
		  HDC device_context = GetDC(window);

			Win32SoundOutput sound_output = {};
			sound_output.samples_per_second = 48000;
			sound_output.running_sample_index = 0;
			sound_output.bytes_per_sample = sizeof(int16_t) * 2;
			sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
      sound_output.latency_sample_count = sound_output.samples_per_second / 15;

			Win32InitDirectSound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
      Win32ClearSoundBuffer(&sound_output);
			GlobalSecondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

      int16_t *samples = (int16_t *)VirtualAlloc(
        0, 
        sound_output.secondary_buffer_size, 
        MEM_RESERVE|MEM_COMMIT, 
        PAGE_READWRITE
      );
				
			Running = true;

      // Initialize game memory
      GameMemory game_memory = {};
      game_memory.permanent_storage_size = 64 * 1024 * 1024; // 64MB
      game_memory.permanent_storage = VirtualAlloc(
        0, 
        game_memory.permanent_storage_size, 
        MEM_RESERVE|MEM_COMMIT, 
        PAGE_READWRITE
      );

      game_memory.transient_storage_size = (uint64_t)4 * 1024 * 1024 * 1024; // 4GB
      game_memory.transient_storage = VirtualAlloc(
        0, 
        game_memory.transient_storage_size, 
        MEM_RESERVE|MEM_COMMIT, 
        PAGE_READWRITE
      );


      // Initialize Controllers
      GameInput game_input[2] = {};
      GameInput *new_input = &game_input[0];
      GameInput *old_input = &game_input[1];

      // Performance counting
      LARGE_INTEGER last_counter = win32_get_wall_clock();
			int64_t last_cycle_count = __rdtsc();

			while(Running) {
        GameControllerInput *old_keyboard_controller = &old_input->controllers[0];
        GameControllerInput *new_keyboard_controller = &new_input->controllers[0];
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

        win32_process_pending_messages(new_keyboard_controller);

        int max_controller_count = XUSER_MAX_COUNT;
        if(max_controller_count > (ArrayCount(new_input->controllers) - 1)) {
          max_controller_count = ArrayCount(new_input->controllers) - 1;
        }

				for(DWORD controller_idx = 0; controller_idx < max_controller_count; ++controller_idx) 
				{
          DWORD our_controller_idx = controller_idx + 1;
          GameControllerInput *old_controller = &old_input->controllers[our_controller_idx];
          GameControllerInput *new_controller = &new_input->controllers[our_controller_idx];


					XINPUT_STATE controller_state;
					if(XInputGetState(controller_idx, &controller_state) == 
            ERROR_SUCCESS) { // indicates the controller is plugged-in

						XINPUT_GAMEPAD *gamepad = &controller_state.Gamepad;

            new_controller->is_analog = true;
            new_controller->is_connected = true;

            new_controller->avg_stick_x = win32_process_input_stick_value(
              gamepad->sThumbLX,
              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
            );
            new_controller->avg_stick_y = win32_process_input_stick_value(
              gamepad->sThumbLY,
              XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
            );


            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
              new_controller->avg_stick_y = 1.0f;
            }
            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
              new_controller->avg_stick_y = -1.0f;
            }
            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
              new_controller->avg_stick_x = -1.0f;
            }
            if(gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
              new_controller->avg_stick_x = 1.0f;
            }

            float threshold = 0.5f;
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

        // Start setting up sound buffer
        DWORD byte_to_lock;
        DWORD target_cursor;
        DWORD bytes_to_write;
				DWORD play_cursor;
				DWORD write_cursor;
        bool is_sound_valid = false;

        HRESULT position_result = GlobalSecondarySoundBuffer->GetCurrentPosition(
					&play_cursor,
					&write_cursor
				);

				if(SUCCEEDED(position_result)) {
					byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % 
              sound_output.secondary_buffer_size;

          target_cursor = 
            ((play_cursor +
              (sound_output.latency_sample_count * sound_output.bytes_per_sample)) %
              sound_output.secondary_buffer_size);
				
					if(byte_to_lock > target_cursor) {
						bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
						bytes_to_write += target_cursor;
					} else {
						bytes_to_write = target_cursor - byte_to_lock;
					}

          is_sound_valid = true;
				}
        
        // Set sample count based on frame-rate of the game to try and keep
        // sounds in sync with visuals
        GameSoundOutputBuffer sound_buffer = {};
        sound_buffer.samples_per_second = sound_output.samples_per_second;
        sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
        sound_buffer.samples = samples;


        // Initialize screen buffer for the next frame
				GameOffScreenBuffer game_offscreen_buffer = {};
				game_offscreen_buffer.memory = GlobalBackBuffer.memory;
				game_offscreen_buffer.width =  GlobalBackBuffer.width;
				game_offscreen_buffer.height = GlobalBackBuffer.height;
				game_offscreen_buffer.pitch = GlobalBackBuffer.pitch;

				game_update_and_render(&game_memory, new_input, &game_offscreen_buffer, &sound_buffer);

        if (is_sound_valid) {
					Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
        }


        // compute target FPS
        LARGE_INTEGER work_counter = win32_get_wall_clock();
        float seconds_elapsed_for_work = win32_get_seconds_elapsed(last_counter, work_counter);
        float seconds_elapsed_for_frame = seconds_elapsed_for_work;
        if (seconds_elapsed_for_frame < target_seconds_elapsed_per_frame) {
          while (seconds_elapsed_for_frame < target_seconds_elapsed_per_frame) {
            if (sleep_is_granular) {
              DWORD sleep_ms = (DWORD)(1000.0f * 
                (target_seconds_elapsed_per_frame - seconds_elapsed_for_frame));

              if (sleep_ms > 0) {
                Sleep(sleep_ms);
              }
            }
            seconds_elapsed_for_frame = win32_get_seconds_elapsed(last_counter, win32_get_wall_clock());
          }
        } else {
          // TODO: Missed frame rate, need to log

        }

        // Blit to screen after frame rate calculations
				Win32WindowDimension dimension = GetWindowDimension(window);

				Win32DisplayBufferInWindow(
					&GlobalBackBuffer,
					device_context, 
					dimension.width,
					dimension.height
				);
        
				//ReleaseDC(window, device_context);

        /*
         * Used for debug FPS
				int32_t elapsed_millis = (int32_t)((1000 * elapsed_counter) / perf_counter_frequency.QuadPart);
				int32_t fps = perf_counter_frequency.QuadPart / elapsed_counter;
				int32_t mega_cycles_per_frame = (int32_t)(elapsed_cycles / (1000 * 1000));
        */

        /*
         * Used for Debug FPS
				char string_buffer[256];
				wsprintf(
					string_buffer, 
					"%dms/frame, %dfps, %dMc/f\n", 
					elapsed_millis,
					fps,
					mega_cycles_per_frame
				);
				OutputDebugStringA(string_buffer);
        */

        GameInput *temp = new_input;
        new_input = old_input;
        old_input = temp;

				// Close time window
        LARGE_INTEGER end_counter = win32_get_wall_clock();
				last_counter = end_counter;
				
        int64_t end_cycle_count = __rdtsc();
				int64_t elapsed_cycles = end_cycle_count - last_cycle_count;
				last_cycle_count = end_cycle_count;
			}

		} else {
			// TODO: Handle Create Window Failure
		}
	} else {
		// TODO: Handle Failure of window creation
	}
	
	return(0);
}




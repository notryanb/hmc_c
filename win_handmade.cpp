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
			uint32 keycode = WParam;
			bool was_down = ((LParam & (1 << 30)) != 0); // Get bit 30 of LParam
			bool is_down = ((LParam & (1 << 31)) == 0); // Get bit 30 of LParam

			if (was_down == is_down) break;

			if (keycode == 'W') {
      } else if (keycode == 'A') {
      } else if (keycode == 'S') {
      } else if (keycode == 'D') {
      } else if (keycode == 'Q') {
      } else if (keycode == 'E') {
      } else if (keycode == VK_UP) {
      } else if (keycode == VK_LEFT) {
      } else if (keycode == VK_DOWN) {
      } else if (keycode == VK_RIGHT) {
      } else if (keycode == VK_ESCAPE) {
      } else if (keycode == VK_SPACE) {
			}
		} break;
		case WM_PAINT: {
			PAINTSTRUCT Paint;
			HDC device_context = BeginPaint(Window, &Paint);

			int x = Paint.rcPaint.left;
			int y = Paint.rcPaint.top;
			int height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int width = Paint.rcPaint.right - Paint.rcPaint.left;
			
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


int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow
) {
	LARGE_INTEGER perf_counter_frequency;
	QueryPerformanceFrequency(&perf_counter_frequency);

	WNDCLASSA WindowClass = {};

	//Win32WindowDimension dimension = GetWindowDimension(Window);
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

      GameInput game_input[2] = {};
      GameInput *new_input = &game_input[0];
      GameInput *old_input = &game_input[1];


			LARGE_INTEGER last_counter;
			QueryPerformanceCounter(&last_counter);
			int64_t last_cycle_count = __rdtsc();

			while(Running) {



				MSG Message;
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT) {
						Running = false;
					}

					TranslateMessage(&Message);	
					DispatchMessageA(&Message);
				}

        int max_controller_count = XUSER_MAX_COUNT;
        if(max_controller_count > ArrayCount(new_input->controllers)) {
          max_controller_count = ArrayCount(new_input->controllers);
        }

				for(DWORD controller_idx = 0; controller_idx < max_controller_count; controller_idx++) 
				{
          GameControllerInput *old_controller = &old_input->controllers[controller_idx];
          GameControllerInput *new_controller = &new_input->controllers[controller_idx];


					XINPUT_STATE controller_state;
					if(XInputGetState(controller_idx, &controller_state) == 
            ERROR_SUCCESS) { // indicates the controller is plugged-in

						XINPUT_GAMEPAD *gamepad = &controller_state.Gamepad;

						bool dpad_up = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool dpad_down = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool dpad_left = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool dpad_right = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

            new_controller->is_analog = true;
            new_controller->start_x = old_controller->end_x;
            new_controller->start_y = old_controller->end_y;

            // Normalize stick values between -1 and 1.
            float stick_x;
            if(gamepad->sThumbLX < 0) {
              stick_x = gamepad->sThumbLX / 32768.0f;
            } else {
              stick_x = gamepad->sThumbLX / 32767.0f;
            }
            new_controller->min_x = new_controller->max_x = new_controller->end_x = stick_x;

            float stick_y;
            if(gamepad->sThumbLY < 0) {
              stick_y = gamepad->sThumbLY / 32768.0f;
            } else {
              stick_y = gamepad->sThumbLY / 32767.0f;
            }
            new_controller->min_y = new_controller->max_y = new_controller->end_y = stick_y;

						//int16_t stick_hori = (float)(gamepad->sThumbLX);
						//int16_t stick_vert = (float)(gamepad->sThumbLY);

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_A,
              &old_controller->down,
              &new_controller->down
            );
            
            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_B,
              &old_controller->right,
              &new_controller->right
            );
            
            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_X,
              &old_controller->left,
              &new_controller->left
            );

            win32_process_x_input_button(
              gamepad->wButtons,
              XINPUT_GAMEPAD_Y,
              &old_controller->up,
              &new_controller->up
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
					} else {
						// Report controller is unavailable
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

				GameOffScreenBuffer game_offscreen_buffer = {};
				game_offscreen_buffer.memory = GlobalBackBuffer.memory;
				game_offscreen_buffer.width =  GlobalBackBuffer.width;
				game_offscreen_buffer.height = GlobalBackBuffer.height;
				game_offscreen_buffer.pitch = GlobalBackBuffer.pitch;

				game_update_and_render(new_input, &game_offscreen_buffer, &sound_buffer);

        if (is_sound_valid) {
					Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
        }

				HDC device_context = GetDC(window);
				Win32WindowDimension dimension = GetWindowDimension(window);

				Win32DisplayBufferInWindow(
					&GlobalBackBuffer,
					device_context, 
					dimension.width,
					dimension.height
				);

				ReleaseDC(window, device_context);

				int64_t end_cycle_count = __rdtsc();
				int64_t elapsed_cycles = end_cycle_count - last_cycle_count;

				LARGE_INTEGER end_counter;
				QueryPerformanceCounter(&end_counter);

				int64_t elapsed_counter = end_counter.QuadPart - last_counter.QuadPart;
				int32_t elapsed_millis = (int32_t)((1000 * elapsed_counter) / perf_counter_frequency.QuadPart);
				int32_t fps = perf_counter_frequency.QuadPart / elapsed_counter;
				int32_t mega_cycles_per_frame = (int32_t)(elapsed_cycles / (1000 * 1000));

				char string_buffer[256];
				wsprintf(
					string_buffer, 
					"%dms/frame, %dfps, %dMc/f\n", 
					elapsed_millis,
					fps,
					mega_cycles_per_frame
				);
				OutputDebugStringA(string_buffer);


				// Close time window
				last_cycle_count = end_cycle_count;
				last_counter = end_counter;

        GameInput *temp = new_input;
        new_input = old_input;
        old_input = temp;
			}

		} else {
			// TODO: Handle Create Window Failure
		}
	} else {
		// TODO: Handle Failure of window creation
	}
	
	return(0);
}




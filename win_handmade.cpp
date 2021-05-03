#include <windows.h>
#include <stdint.h> // Bring in unit8_t to avoid casting to (unsigned char *)
#include <xinput.h> // Device input like joypads
#include <dsound.h>
#include <math.h>

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

struct Win32SoundOutput {
	int samples_per_second;
	int hz;
	int tone_volume;
	uint32_t running_sample_index;
	int wave_period;
	int bytes_per_sample;
	int secondary_buffer_size;
};


struct  Win32OffScreenBuffer {
	BITMAPINFO info;
	void *memory;
	int width;
	int height;
	int pitch;
	int bytes_per_pixel;
};

global_variable bool Running = false;
global_variable Win32OffScreenBuffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondarySoundBuffer;

struct Win32WindowDimension {
	int width;
	int height;
};

void Win32FillSoundBuffer(Win32SoundOutput *sound_output, DWORD byte_to_lock, DWORD bytes_to_write) {
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
		int16 *sample_out = (int16 *)region1;
		DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;
		for(
			DWORD sample_index = 0; 
			sample_index < region1_sample_count; 
			++sample_index
		) {
			float wave_position = 2.0f * Pi32 * ((float)sound_output->running_sample_index / (float)sound_output->wave_period);
			float sine_value = sinf(wave_position);
			int16_t sample_value = (int16)(sine_value * sound_output->tone_volume);

			*sample_out++ = sample_value;
			*sample_out++ = sample_value;

			++sound_output->running_sample_index;
		}
		

		DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
		sample_out = (int16 *)region2;
		for(
			DWORD sample_index = 0; 
			sample_index < region2_sample_count; 
			++sample_index
		) {
			float wave_position = 2.0f * Pi32 * ((float)sound_output->running_sample_index / (float)sound_output->wave_period);
			float sine_value = sinf(wave_position);
			int16_t sample_value = (int16)(sine_value * sound_output->tone_volume);

			*sample_out++ = sample_value;
			*sample_out++ = sample_value;
			
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
internal_function void Win32InitDirectSound(HWND window, int32 samples_per_second, int32 buffer_size) {
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
internal_function void Win32ResizeDIBSection(Win32OffScreenBuffer *buffer, int width, int height) {

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
	buffer->memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = width * buffer->bytes_per_pixel;
}

internal_function void Win32DisplayBufferInWindow(
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

// Handle controller polling
internal_function void GamePadInput() {
	for(DWORD controller_idx = 0; controller_idx < XUSER_MAX_COUNT; controller_idx++) 
	{
		XINPUT_STATE controller_state;
		if(XInputGetState(controller_idx, &controller_state) == 
		ERROR_SUCCESS) { // indicates the controller is plugged-in
			XINPUT_GAMEPAD *gamepad = &controller_state.Gamepad;

			bool dpad_up = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
			bool dpad_down = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			bool dpad_left = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			bool dpad_right = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
			bool start = (gamepad->wButtons & XINPUT_GAMEPAD_START);
			bool back = (gamepad->wButtons & XINPUT_GAMEPAD_BACK);
			bool left_shoulder = (gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			bool right_shoulder = (gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
			bool a_button = (gamepad->wButtons & XINPUT_GAMEPAD_A);
			bool b_button = (gamepad->wButtons & XINPUT_GAMEPAD_B);
			bool x_button = (gamepad->wButtons & XINPUT_GAMEPAD_X);
			bool y_button = (gamepad->wButtons & XINPUT_GAMEPAD_Y);

			int16 stick_hori = gamepad->sThumbLX;
			int16 stick_vert = gamepad->sThumbLY;
		} else {
			// controller is unavailable
			
		}


	}

}


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
			int x_offset = 0;
			int y_offset = 0;

			Win32SoundOutput sound_output = {};
			sound_output.samples_per_second = 48000;
			sound_output.hz = 256;
			sound_output.tone_volume = 3000;
			sound_output.running_sample_index = 0;
			sound_output.wave_period = sound_output.samples_per_second / sound_output.hz;
			sound_output.bytes_per_sample = sizeof(int16_t) * 2;
			sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;

			Win32InitDirectSound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
			Win32FillSoundBuffer(&sound_output, 0, sound_output.secondary_buffer_size);
			GlobalSecondarySoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
				
			Running = true;

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
				for(DWORD controller_idx = 0; controller_idx < XUSER_MAX_COUNT; controller_idx++) 
				{
					XINPUT_STATE controller_state;
					if(XInputGetState(controller_idx, &controller_state) == 
					ERROR_SUCCESS) { // indicates the controller is plugged-in
						XINPUT_GAMEPAD *gamepad = &controller_state.Gamepad;

						bool dpad_up = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
						bool dpad_down = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
						bool dpad_left = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
						bool dpad_right = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
						bool start = (gamepad->wButtons & XINPUT_GAMEPAD_START);
						bool back = (gamepad->wButtons & XINPUT_GAMEPAD_BACK);
						bool left_shoulder = (gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
						bool right_shoulder = (gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
						bool a_button = (gamepad->wButtons & XINPUT_GAMEPAD_A);
						bool b_button = (gamepad->wButtons & XINPUT_GAMEPAD_B);
						bool x_button = (gamepad->wButtons & XINPUT_GAMEPAD_X);
						bool y_button = (gamepad->wButtons & XINPUT_GAMEPAD_Y);

						int16 stick_hori = gamepad->sThumbLX;
						int16 stick_vert = gamepad->sThumbLY;

						if (a_button) {
							y_offset += 2;
						}
					} else {
						// controller is unavailable
						
					}


				}

				XINPUT_VIBRATION Vibration;
				Vibration.wLeftMotorSpeed = 60000;
				Vibration.wRightMotorSpeed = 60000;
				XInputSetState(0, &Vibration);

				GameOffScreenBuffer game_offscreen_buffer = {};
				game_offscreen_buffer.memory = GlobalBackBuffer.memory;
				game_offscreen_buffer.width =  GlobalBackBuffer.width;
				game_offscreen_buffer.height = GlobalBackBuffer.height;
				game_offscreen_buffer.pitch = GlobalBackBuffer.pitch;
				game_update_and_render(&game_offscreen_buffer);

				DWORD play_cursor;
				DWORD write_cursor;
				if(SUCCEEDED(GlobalSecondarySoundBuffer->GetCurrentPosition(
					&play_cursor,
					&write_cursor
				))){
					DWORD byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample) % sound_output.secondary_buffer_size;
					DWORD bytes_to_write;
				
					if(byte_to_lock == play_cursor) {
						bytes_to_write = 0;
					} else if(byte_to_lock > play_cursor) {
						bytes_to_write = (sound_output.secondary_buffer_size - byte_to_lock);
						bytes_to_write += play_cursor;
					} else {
						bytes_to_write = play_cursor - byte_to_lock;
					}

					Win32FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write);
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
			}

		} else {
			// TODO: Handle Create Window Failure
		}
	} else {
		// TODO: Handle Failure of window creation
	}
	


	return(0);
}



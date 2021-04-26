#include <windows.h>
#include <stdint.h> // Bring in unit8_t to avoid casting to (unsigned char *)

#define internal_function static
#define local_persist static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;


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


struct Win32WindowDimension {
	int width;
	int height;
};

Win32WindowDimension GetWindowDimension(HWND window) {
	Win32WindowDimension result;

	RECT client_rect;
	GetClientRect(window, &client_rect);
	result.width = client_rect.right - client_rect.left;
	result.height = client_rect.bottom - client_rect.top;

	return(result);
};


internal_function void RenderWeirdGradient(Win32OffScreenBuffer buffer, int x_offset, int y_offset) {
	// ensure void* is cast to unsigned 8-bit int so we can do pointer arithmetic
	uint8 *row = (uint8 *)buffer.memory;

	for(int y = 0; y < buffer.height; ++y) {
		uint32 *pixel = (uint32 *)row;

		for(int x = 0; x < buffer.width; ++x) {
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

		row += buffer.pitch;
	}

}


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
		HDC device_context, 
		int window_width,
		int window_height,
		Win32OffScreenBuffer buffer,
		int x, 
		int y, 
		int width, 
		int height
) {
	StretchDIBits(
		device_context,
		0, 0, window_width, window_height,
		0, 0, buffer.width, buffer.height,
		buffer.memory,
		&buffer.info,
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
		case WM_PAINT: {
			PAINTSTRUCT Paint;
			HDC device_context = BeginPaint(Window, &Paint);

			int x = Paint.rcPaint.left;
			int y = Paint.rcPaint.top;
			int height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int width = Paint.rcPaint.right - Paint.rcPaint.left;
			
			Win32WindowDimension dimension = GetWindowDimension(Window);
			Win32DisplayBufferInWindow(
				device_context, 
				dimension.width,
				dimension.height,
				GlobalBackBuffer,
				x, 
				y, width, 
				height
			);
			
			EndPaint(Window, &Paint);
		} break;
		default: { 
			Result = DefWindowProc(Window, Message, WParam, LParam);
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
	WNDCLASS WindowClass = {};

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
			Running = true;
			int x_offset = 0;
			int y_offset = 0;
			while(Running) {
				MSG Message;
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) {
					if (Message.message == WM_QUIT) {
						Running = false;
					}

					TranslateMessage(&Message);	
					DispatchMessageA(&Message);
				}

				RenderWeirdGradient(GlobalBackBuffer, x_offset, y_offset);

				HDC device_context = GetDC(window);
				Win32WindowDimension dimension = GetWindowDimension(window);

				Win32DisplayBufferInWindow(
					device_context, 
					dimension.width,
					dimension.height,
					GlobalBackBuffer,
					0,
					0,
					dimension.width, 
					dimension.height
				);
				ReleaseDC(window, device_context);
				++x_offset;
			}

		} else {
			// TODO: Handle Create Window Failure
		}
	} else {
		// TODO: Handle Failure of window creation
	}
	


	return(0);
}





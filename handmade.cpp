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

global_variable bool Running = false;
global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;
global_variable int bitmap_width;
global_variable int bitmap_height;
global_variable int bytes_per_pixel = 4;

internal_function void RenderWeirdGradient(int x_offset, int y_offset) {
	int width = bitmap_width;
	int height = bitmap_height; 
	int pitch = width * bytes_per_pixel;

	// ensure void* is cast to unsigned 8-bit int so we can do pointer arithmetic
	uint8 *row = (uint8 *)bitmap_memory;

	for(int y = 0; y < bitmap_height; ++y) {
		uint32 *pixel = (uint32 *)row;

		for(int x = 0; x <= bitmap_width; ++x) {
				/* 
				 * This is little endian and swapped by Windows developers
				 * Index: 3  2  1  0
				 * Bytes: 00 00 00 00
				 * Repr:  xx RR GG BB
				 */
			uint8 blue = (x + x_offset);
			uint8 green = (y + y_offset); 
			*pixel++ = ((green << 8) | blue);
		}

		row += pitch;
	}

}


// Resize Device Independent BitMap Section
internal_function void
Win32ResizeDIBSection(int width, int height) {

	if (bitmap_memory) {
		VirtualFree(bitmap_memory, 0, MEM_RELEASE);
	} 

	bitmap_width = width;
	bitmap_height = height;
	
	bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
	bitmap_info.bmiHeader.biWidth = bitmap_width;
	bitmap_info.bmiHeader.biHeight = -bitmap_height; // Make this negative so the bitmap origin is top left
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32; // Adding for 32 bits even though we only need 24 for RGB
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	int bitmap_memory_size = (width * height) * bytes_per_pixel;
	bitmap_memory = VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
	
	//RenderWeirdGradient(0, 0);
}

internal_function void
Win32UpdateWindow(HDC device_context, RECT *client_rect, int x, int y, int width, int height) {
	int window_width = client_rect->right - client_rect->left;
	int window_height = client_rect->bottom - client_rect->top;

	StretchDIBits(
		device_context,
		0, 0, bitmap_width, bitmap_height,
		0, 0, window_width, window_height,
		bitmap_memory,
		&bitmap_info,
		DIB_RGB_COLORS,
		SRCCOPY
	);
}


LRESULT CALLBACK 
Win32MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{
	LRESULT Result = 0;

	switch(Message) {
		case WM_SIZE: {
			RECT ClientRect;
			// Get part of Window we can draw into
			GetClientRect(Window, &ClientRect);

			int height = ClientRect.bottom - ClientRect.top;
			int width = ClientRect.right - ClientRect.left;

			Win32ResizeDIBSection(width, height);

			OutputDebugStringA("WM_SIZE\n");
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

			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			
			RECT client_rec;
			GetClientRect(Window, &client_rec);
			Win32UpdateWindow(device_context, &client_rec, X, Y, Width, Height);
			
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

				RenderWeirdGradient(x_offset, y_offset);

				HDC device_context = GetDC(window);
				RECT client_rect;
				GetClientRect(window, &client_rect);
				int window_width = client_rect.right - client_rect.left;
				int window_height = client_rect.bottom - client_rect.top;

				Win32UpdateWindow(
					device_context, 
					&client_rect, 
					0,
					0,
					window_width, 
					window_height
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





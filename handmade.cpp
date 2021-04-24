#include <windows.h>

#define internal_function static
#define local_persist static
#define global_variable static

global_variable bool Running = false;
global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;
global_variable HBITMAP bitmap_handle;
global_variable HDC bitmap_device_context;


// Resize Device Independent BitMap Section
internal_function void
Win32ResizeDIBSection(int width, int height) {
	// Free DIB section
	if (bitmap_handle) {
		DeleteObject(bitmap_handle);
	} 
	
	if (!bitmap_device_context) {
		bitmap_device_context = CreateCompatibleDC(0);
	}


	bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
	bitmap_info.bmiHeader.biWidth = width;
	bitmap_info.bmiHeader.biHeight = height;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	/*
	 * The following fields are cleared for free when the global var
	 * is declared
		bitmap_info.biSizeImage = 0;
		bitmap_info.biXPelsPerMeter = 0;
		bitmap_info.biYPelsPerMeter = 0;
		bitmap_info.biClrUsed = 0;
		bitmap_info.biClrImportant = 0;
	*/

	bitmap_handle = CreateDIBSection(
		bitmap_device_context,
		&bitmap_info,
		DIB_RGB_COLORS,
		&bitmap_memory,
		0,
		0
	);

}

internal_function void
Win32UpdateWindow(HDC device_context, int x, int y, int width, int height) {
	StretchDIBits(
		device_context,
		x, y, width, height, /* Source rect buffer we're reading from */
		x, y, width, height, /* Back Buffer we're drawing our rect into */
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
			int height = ClientRect.bottom - ClientRect.top;
			int width = ClientRect.right - ClientRect.left;

			// Get part of Window we can draw into
			GetClientRect(Window, &ClientRect);
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
			
			Win32UpdateWindow(device_context, X, Y, Width, Height);

			
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
	//WindowClass.hIcon;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	/* takes pointer to WindowClass */
	if (RegisterClass(&WindowClass)) {
		HWND WindowHandle = CreateWindowEx(
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

		if (WindowHandle) {
			Running = true;
			while(Running) {
				MSG Message;
				BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
				if (MessageResult > 0) {
					// Pass in Pointer, aka "Address of Message"
					TranslateMessage(&Message);	
					DispatchMessage(&Message);
				} else {
					break;
				}
			}

		} else {
			// TODO: Handle Create Window Failure
		}
	} else {
		// TODO: Handle Failure of window creation
	}
	


	return(0);
}





#include <windows.h>

LRESULT CALLBACK 
MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
)
{
	LRESULT Result = 0;

	switch(Message) {
		case WM_SIZE: {
			OutputDebugStringA("WM_SIZE\n");
		} break;
		case WM_DESTROY: {
			OutputDebugStringA("WM_DESTROY\n");
		} break;
		case WM_CLOSE: {
			OutputDebugStringA("WM_CLOSE\n");
		} break;
		case WM_ACTIVATEAPP: {
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		case WM_PAINT: {
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);

			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			PatBlt(DeviceContext, X, Y, Width, Height, WHITENESS);

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
	WindowClass.lpfnWndProc = MainWindowCallback;
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
			MSG Message;
			for(;;)
			{
				BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
				if (MessageResult > 0) {
					// Pass in Pointer, aka "Address of Message"
					TranslateMessage(&Message);	
					DispatchMessage(&Message);
				} else {
					break;
					// TODO There was an error getting Window Message Events
					// It returned a -1 even though it is a Bool.
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





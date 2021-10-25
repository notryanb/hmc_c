Building the file


Builing with zig and putting it into the `\build` dir.
-o is the output
-l is to link a system library. the `.lib` or `.dll` can be omitted
-g generates debug symbols for helping step through code in VS debug mode.
-fPIC does something with address layout. Check the C book and fill it in here.

zig cc handmade.cpp -o build\handmade.dll -shared -g -fPIC
zig cc win_handmade.cpp -o build\win_handmade.exe -l user32 -l gdi32 -l xinput -l dsound -l winmm -g

## Notes
- use VisualStudio `dumpbin /EXPORTS <DLL HERE>` to view exported functions from DLLs

## Replay Feature (Debug loops)
- press `l` while the game is running and then enter some game controller input.
- Once done entering game input, press `l` to finish the recording section and initiate looping.


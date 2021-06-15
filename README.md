Building the file


Builing with zig and putting it into the `\build` dir.
-o is the output
-l is to link a system library. the `.lib` or `.dll` can be omitted
-g generates debug symbols for helping step through code in VS debug mode.

zig cc win_handmade.cpp -o build\win_handmade.exe -l user32 -l gdi32 -l xinput -l dsound -l winmm -g


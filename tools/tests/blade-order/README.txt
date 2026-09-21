Blade scene-order regression tests

python3 tools/tests/blade-order/test_queue.py
LIBGL_ALWAYS_SOFTWARE=1 python3 tools/tests/blade-order/test_gl.py /path/to/CARMA/DATA/PIXELMAP/EAGLE1.PIX

Requires a host C compiler; queue test uses ASan/UBSan. GL test requires EGL/OpenGL development libraries and Mesa surfaceless EGL. Neither launches an Amiga emulator. Both compile the queue functions extracted from current match.c, using small host fixture types and a fixture state setter. GL test uses the actual indexed blade mask with white opaque pixels, not its game palette. It validates visibility and depth ordering, not the complete game or MiniGL implementation.

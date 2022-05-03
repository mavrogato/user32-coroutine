
all: win32-coroutine
run: win32-coroutine
	win32-coroutine

win32-coroutine: makefile main.cc
	cl main.cc /Fe:win32-coroutine /EHsc /Ob2 /O1 /W4 /std:c++latest

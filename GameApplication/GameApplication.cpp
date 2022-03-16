// GameApplication.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

#ifdef __ANDROID__
#include <jni.h>
#include <errno.h>

#include <android/sensor.h>
#include <android/log.h>
#include "android_native_app_glue.h"
#include <android/native_window_jni.h>

#include "TeapotRenderer.h"
#include "NDKHelper.h"
#include "Engine.h"

//-------------------------------------------------------------------------
// Preprocessor
//-------------------------------------------------------------------------
#define HELPER_CLASS_NAME \
  "com/example/gameapplication/NDKHelper"  // Class name of helper function
//-------------------------------------------------------------------------
// Shared state for our app.
//-------------------------------------------------------------------------
struct android_app;
Engine g_engine;

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(android_app* state) {

	g_engine.SetState(state);

	// Init helper functions
	ndk_helper::JNIHelper::Init(state->activity, HELPER_CLASS_NAME);

	state->userData = &g_engine;
	state->onAppCmd = Engine::HandleCmd;
	state->onInputEvent = Engine::HandleInput;

#ifdef USE_NDK_PROFILER
	monstartup("libTeapotNativeActivity.so");
#endif

	// Prepare to monitor accelerometer
	g_engine.InitSensors();

	// loop waiting for stuff to do.
	while (1) {
		// Read all pending events.
		int id;
		int events;
		android_poll_source* source;

		// If not animating, we will block forever waiting for events.
		// If animating, we loop until all events are read, then continue
		// to draw the next frame of animation.
		while ((id = ALooper_pollAll(g_engine.IsReady() ? 0 : -1, NULL, &events,
			(void**)&source)) >= 0) {
			// Process this event.
			if (source != NULL) source->process(state, source);

			g_engine.ProcessSensors(id);

			// Check if we are exiting.
			if (state->destroyRequested != 0) {
				g_engine.TermDisplay();
				return;
			}
		}

		if (g_engine.IsReady()) {
			// Drawing is throttled to the screen update rate, so there
			// is no need to do timing here.
			g_engine.DrawFrame();
		}
	}
}

#else
#include<Windows.h>    
// first include Windows.h header file which is required    
#include<stdio.h>
#include<gl/glew.h>
#include<gl/GL.h>   // GL.h header file    
#include<gl/GLU.h> // GLU.h header file    
#include<gl/glut.h>  // glut.h header file from freeglut\include\GL folder    
#include<conio.h>    
#include<stdio.h>    
#include<math.h>    
#include<string.h>
#include "TeapotRenderer.h"
#include "TapCamera.h"
#include "Engine.h"

Engine g_engine;

// Display_Objects() function    
void Draw(void)
{
	g_engine.Draw();
}
// Reshape() function    
void Reshape(int w, int h)
{
	g_engine.Reshape(w, h);
}

void Mouse(int button, int state, int x, int y)
{
	g_engine.Mouse(button, state, x, y);
}

void MouseMove(int x, int y)
{
	g_engine.MouseMove(x, y);
}

#pragma comment(lib, "user32.lib")

void DumpDevice(const DISPLAY_DEVICE& dd, int nSpaceCount)
{
	wprintf(TEXT("%*sDevice Name: %s\n"), nSpaceCount, TEXT(""), dd.DeviceName);
	wprintf(TEXT("%*sDevice String: %s\n"), nSpaceCount, TEXT(""), dd.DeviceString);
	wprintf(TEXT("%*sState Flags: %x\n"), nSpaceCount, TEXT(""), dd.StateFlags);
	wprintf(TEXT("%*sDeviceID: %s\n"), nSpaceCount, TEXT(""), dd.DeviceID);
	wprintf(TEXT("%*sDeviceKey: ...%s\n\n"), nSpaceCount, TEXT(""), dd.DeviceKey + 42);
}

bool InitializeWithFirstDevice(char * program)
{
	DISPLAY_DEVICE dd;
	dd.cb = sizeof(DISPLAY_DEVICE);

	DWORD deviceNum = 0;
	while (EnumDisplayDevices(NULL, deviceNum, &dd, 0)) {

		DISPLAY_DEVICE newdd = { 0 };
		newdd.cb = sizeof(DISPLAY_DEVICE);
		DWORD monitorNum = 0;
		while (EnumDisplayDevices(dd.DeviceName, monitorNum, &newdd, 0))
		{
			DumpDevice(dd, 0);
			DumpDevice(newdd, 4);
			monitorNum++;
			int argc = 3;
			char * argv[3];
			char hold[250];
			argv[2] = hold;
			size_t size;
			wcstombs_s(&size, hold, dd.DeviceName, 250);
			argv[0] = _strdup(program);
			argv[1] = _strdup("-display");
			glutInit(&argc, argv);
			free(argv[0]);
			free(argv[1]);
			free(argv[2]);
			return true;
		}
		puts("");
		deviceNum++;
	}

	return false;
}

int main(int argc, char** argv)
{
	if (argc != 1)
	{
		// If user passed parameters, then use those.
		glutInit(&argc, argv);
	}
	else
	{
		// Otherwise, choose the first device.
		if (!InitializeWithFirstDevice(argv[0]))
		{
			// Well that didn't work. Try default init afterall.
			glutInit(&argc, argv);
		}
	}

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(700, 500);
	glutInitWindowPosition(250, 50);
	glutCreateWindow("OpenGL Demo");
	g_engine.InitDisplay();
	glutDisplayFunc(Draw);
	glutReshapeFunc(Reshape);
	glutMouseFunc(Mouse);
	glutMotionFunc(MouseMove);
	glutMainLoop();

	return 0;
}

#endif
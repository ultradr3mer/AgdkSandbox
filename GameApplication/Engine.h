#pragma once
#include <TeapotRenderer.h>

#include "pch.h"

class Engine {
  TeapotRenderer renderer_;
  ndk_helper::TapCamera tap_camera_;
  
  void TransformPosition(ndk_helper::Vec2& vec);

  int screen_width_;
  int screen_height_;

  ndk_helper::GLContext* gl_context_;

  #ifdef __ANDROID__

  bool initialized_resources_;
  bool has_focus_;

  ndk_helper::DoubletapDetector doubletap_detector_;
  ndk_helper::PinchDetector pinch_detector_;
  ndk_helper::DragDetector drag_detector_;
  ndk_helper::PerfMonitor monitor_;

  android_app* app_;

  ASensorManager* sensor_manager_;
  const ASensor* accelerometer_sensor_;
  ASensorEventQueue* sensor_event_queue_;

  void UpdateFPS(float fFPS);
  void ShowUI();

public:
  static void HandleCmd(struct android_app* app, int32_t cmd);
  static int32_t HandleInput(android_app* app, AInputEvent* event);

  Engine();
  ~Engine();
  void SetState(android_app* app);
  int InitDisplay(android_app* app);
  void LoadResources();
  void UnloadResources();
  void DrawFrame();
  void TermDisplay();
  void TrimMemory();
  bool IsReady();

  void UpdatePosition(AInputEvent* event, int32_t iIndex, float& fX, float& fY);

  void InitSensors();
  void ProcessSensors(int32_t id);
  void SuspendSensors();
  void ResumeSensors();

  #endif // __ANDROID__

  #ifdef __WIN__
public:
  Engine();
  ~Engine();

  void Draw();
  void InitDisplay();
  void Mouse(int button, int state, int x, int y);
  void MouseMove(int x, int y);
  void Reshape(int w, int h);
  #endif // __WIN__
};


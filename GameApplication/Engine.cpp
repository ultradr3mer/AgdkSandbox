#include "pch.h"

#include "Engine.h"
#include "android_native_app_glue.h"

// Old (blue)
//float TEAPOT_COLOR[] = { 100 / 255.0f, 149 / 255.0f, 237 / 255.0f };
// New (green)
float TEAPOT_COLOR[] = { 61 / 255.0f, 220 / 255.0f, 132 / 255.0f };

void Engine::TransformPosition(ndk_helper::Vec2& vec) {
  vec = ndk_helper::Vec2(2.0f, 2.0f) * vec /
    ndk_helper::Vec2(gl_context_->GetScreenWidth(), gl_context_->GetScreenHeight()) -
    ndk_helper::Vec2(1.f, 1.f);
}

#ifdef __ANDROID__

//-------------------------------------------------------------------------
// Ctor
//-------------------------------------------------------------------------
Engine::Engine()
  : initialized_resources_(false),
  has_focus_(false),
  app_(NULL),
  sensor_manager_(NULL),
  accelerometer_sensor_(NULL),
  sensor_event_queue_(NULL) {
  gl_context_ = ndk_helper::GLContext::GetInstance();
  //this->
}

//-------------------------------------------------------------------------
// Dtor
//-------------------------------------------------------------------------
Engine::~Engine() {}

/**
 * Load resources
 */
void Engine::LoadResources() {
  renderer_.Init();
  renderer_.Bind(&tap_camera_);
}

/**
 * Unload resources
 */
void Engine::UnloadResources() { renderer_.Unload(); }

/**
 * Initialize an EGL context for the current display.
 */
int Engine::InitDisplay(android_app* app) {
  if (!initialized_resources_) {
    gl_context_->Init(app_->window);
    LoadResources();
    initialized_resources_ = true;
  }
  else if (app->window != gl_context_->GetANativeWindow()) {
    // Re-initialize ANativeWindow.
    // On some devices, ANativeWindow is re-created when the app is resumed
    assert(gl_context_->GetANativeWindow());
    UnloadResources();
    gl_context_->Invalidate();
    app_ = app;
    gl_context_->Init(app->window);
    LoadResources();
    initialized_resources_ = true;
  }
  else {
    // initialize OpenGL ES and EGL
    if (EGL_SUCCESS == gl_context_->Resume(app_->window)) {
      UnloadResources();
      LoadResources();
    }
    else {
      assert(0);
    }
  }

  ShowUI();

  // Initialize GL state.
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);

  // Note that screen size might have been changed
  glViewport(0, 0, gl_context_->GetScreenWidth(),
    gl_context_->GetScreenHeight());
  renderer_.UpdateViewport();

  tap_camera_.SetFlip(1.f, -1.f, -1.f);
  tap_camera_.SetPinchTransformFactor(2.f, 2.f, 8.f);

  return 0;
}

/**
 * Just the current frame in the display.
 */
void Engine::DrawFrame() {
  float fps;
  if (monitor_.Update(fps)) {
    UpdateFPS(fps);
  }
  renderer_.Update(monitor_.GetCurrentTime());

  // Just fill the screen with a color.
  glClearColor(0.0f, 0.0f, 0.0f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderer_.Render(TEAPOT_COLOR);

  // Swap
  if (EGL_SUCCESS != gl_context_->Swap()) {
    UnloadResources();
    LoadResources();
  }
}

/**
 * Tear down the EGL context currently associated with the display.
 */
void Engine::TermDisplay() { gl_context_->Suspend(); }

void Engine::TrimMemory() {
  LOGI("Trimming memory");
  gl_context_->Invalidate();
}
/**
 * Process the next input event.
 */
int32_t Engine::HandleInput(android_app* app, AInputEvent* event) {
  Engine* eng = (Engine*)app->userData;
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    ndk_helper::GESTURE_STATE doubleTapState =
      eng->doubletap_detector_.Detect(event);
    ndk_helper::GESTURE_STATE dragState = eng->drag_detector_.Detect(event);
    ndk_helper::GESTURE_STATE pinchState = eng->pinch_detector_.Detect(event);

    // Double tap detector has a priority over other detectors
    if (doubleTapState == ndk_helper::GESTURE_STATE_ACTION) {
      // Detect double tap
      eng->tap_camera_.Reset(true);
    }
    else {
      // Handle drag state
      if (dragState & ndk_helper::GESTURE_STATE_START) {
        // Otherwise, start dragging
        ndk_helper::Vec2 v;
        eng->drag_detector_.GetPointer(v);
        eng->TransformPosition(v);
        eng->tap_camera_.BeginDrag(v);
      }
      else if (dragState & ndk_helper::GESTURE_STATE_MOVE) {
        ndk_helper::Vec2 v;
        eng->drag_detector_.GetPointer(v);
        eng->TransformPosition(v);
        eng->tap_camera_.Drag(v);
      }
      else if (dragState & ndk_helper::GESTURE_STATE_END) {
        eng->tap_camera_.EndDrag();
      }

      // Handle pinch state
      if (pinchState & ndk_helper::GESTURE_STATE_START) {
        // Start new pinch
        ndk_helper::Vec2 v1;
        ndk_helper::Vec2 v2;
        eng->pinch_detector_.GetPointers(v1, v2);
        eng->TransformPosition(v1);
        eng->TransformPosition(v2);
        eng->tap_camera_.BeginPinch(v1, v2);
      }
      else if (pinchState & ndk_helper::GESTURE_STATE_MOVE) {
        // Multi touch
        // Start new pinch
        ndk_helper::Vec2 v1;
        ndk_helper::Vec2 v2;
        eng->pinch_detector_.GetPointers(v1, v2);
        eng->TransformPosition(v1);
        eng->TransformPosition(v2);
        eng->tap_camera_.Pinch(v1, v2);
      }
    }
    return 1;
  }
  return 0;
}

/**
 * Process the next main command.
 */
void Engine::HandleCmd(struct android_app* app, int32_t cmd) {
  Engine* eng = (Engine*)app->userData;
  switch (cmd) {
  case APP_CMD_SAVE_STATE:
    break;
  case APP_CMD_INIT_WINDOW:
    // The window is being shown, get it ready.
    if (app->window != NULL) {
      eng->InitDisplay(app);
      eng->has_focus_ = true;
      eng->DrawFrame();
    }
    break;
  case APP_CMD_TERM_WINDOW:
    // The window is being hidden or closed, clean it up.
    eng->TermDisplay();
    eng->has_focus_ = false;
    break;
  case APP_CMD_STOP:
    break;
  case APP_CMD_GAINED_FOCUS:
    eng->ResumeSensors();
    // Start animation
    eng->has_focus_ = true;
    break;
  case APP_CMD_LOST_FOCUS:
    eng->SuspendSensors();
    // Also stop animating.
    eng->has_focus_ = false;
    eng->DrawFrame();
    break;
  case APP_CMD_LOW_MEMORY:
    // Free up GL resources
    eng->TrimMemory();
    break;
  }
}

//-------------------------------------------------------------------------
// Sensor handlers
//-------------------------------------------------------------------------
void Engine::InitSensors() {
  sensor_manager_ = ndk_helper::AcquireASensorManagerInstance(app_);
  accelerometer_sensor_ = ASensorManager_getDefaultSensor(
    sensor_manager_, ASENSOR_TYPE_ACCELEROMETER);
  sensor_event_queue_ = ASensorManager_createEventQueue(
    sensor_manager_, app_->looper, LOOPER_ID_USER, NULL, NULL);
}

void Engine::ProcessSensors(int32_t id) {
  // If a sensor has data, process it now.
  if (id == LOOPER_ID_USER) {
    if (accelerometer_sensor_ != NULL) {
      ASensorEvent event;
      while (ASensorEventQueue_getEvents(sensor_event_queue_, &event, 1) > 0) {
      }
    }
  }
}

void Engine::ResumeSensors() {
  // When our app gains focus, we start monitoring the accelerometer.
  if (accelerometer_sensor_ != NULL) {
    ASensorEventQueue_enableSensor(sensor_event_queue_, accelerometer_sensor_);
    // We'd like to get 60 events per second (in us).
    ASensorEventQueue_setEventRate(sensor_event_queue_, accelerometer_sensor_,
      (1000L / 60) * 1000);
  }
}

void Engine::SuspendSensors() {
  // When our app loses focus, we stop monitoring the accelerometer.
  // This is to avoid consuming battery while not being used.
  if (accelerometer_sensor_ != NULL) {
    ASensorEventQueue_disableSensor(sensor_event_queue_, accelerometer_sensor_);
  }
}

//-------------------------------------------------------------------------
// Misc
//-------------------------------------------------------------------------
void Engine::SetState(android_app* state) {
  app_ = state;
  doubletap_detector_.SetConfiguration();
  drag_detector_.SetConfiguration();
  pinch_detector_.SetConfiguration();
}

bool Engine::IsReady() {
  if (has_focus_) return true;

  return false;
}


void Engine::ShowUI() {
  JNIEnv* jni;
  app_->activity->vm->AttachCurrentThread(&jni, NULL);

  // Default class retrieval
  jclass clazz = jni->GetObjectClass(app_->activity->clazz);
  jmethodID methodID = jni->GetMethodID(clazz, "showUI", "()V");
  jni->CallVoidMethod(app_->activity->clazz, methodID);

  app_->activity->vm->DetachCurrentThread();
  return;
}

void Engine::UpdateFPS(float fFPS) {
  JNIEnv* jni;
  app_->activity->vm->AttachCurrentThread(&jni, NULL);

  // Default class retrieval
  jclass clazz = jni->GetObjectClass(app_->activity->clazz);
  jmethodID methodID = jni->GetMethodID(clazz, "updateFPS", "(F)V");
  jni->CallVoidMethod(app_->activity->clazz, methodID, fFPS);

  app_->activity->vm->DetachCurrentThread();
  return;
}

#else

#include<gl/glew.h>
#include<gl/GL.h>   // GL.h header file    
#include<gl/GLU.h> // GLU.h header file    
#include<gl/glut.h>  // glut.h header file from freeglut\include\GL folder    

//-------------------------------------------------------------------------
// Ctor
//-------------------------------------------------------------------------
Engine::Engine()
{
  gl_context_ = ndk_helper::GLContext::GetInstance();
}

//-------------------------------------------------------------------------
// Dtor
//-------------------------------------------------------------------------
Engine::~Engine() {}

// Display_Objects() function    
void Engine::Draw(void)
{
  tap_camera_.Update();
  renderer_.Update(0.13f);
  // clearing the window or remove all drawn objects    
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  renderer_.Render(TEAPOT_COLOR);
  glutSwapBuffers();
  glutPostRedisplay();
}
// Reshape() function    
void Engine::Reshape(int w, int h)
{
  //adjusts the pixel rectangle for drawing to be the entire new window    
  glViewport(0, 0, (GLsizei)w, (GLsizei)h);
  //matrix specifies the projection transformation    
  //glMatrixMode(GL_PROJECTION);
  // load the identity of matrix by clearing it.    
  //glLoadIdentity();
  //gluPerspective(60.0, (GLfloat)w / (GLfloat)h, 1.0, 20.0);
  //matrix specifies the modelview transformation    
  //glMatrixMode(GL_MODELVIEW);
  // again  load the identity of matrix    
  //glLoadIdentity();
  // gluLookAt() this function is used to specify the eye.    
  // it is used to specify the coordinates to view objects from a specific position    
  //gluLookAt(-0.3, 0.5, 5.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0);

  gl_context_->Reshape(w, h);
}

void Engine::Mouse(int button, int state, int x, int y)
{
  if (button == 0 and state == 0) {
    ndk_helper::Vec2 v = ndk_helper::Vec2(x, y);
    TransformPosition(v);
    tap_camera_.BeginDrag(v);
  }
  else if (button == 0 and state == 1) {
    tap_camera_.EndDrag();
  }
}

void Engine::MouseMove(int x, int y)
{
  ndk_helper::Vec2 v = ndk_helper::Vec2(x, y);
  TransformPosition(v);
  tap_camera_.Drag(v);
}

void Engine::InitDisplay()
{
  GLint GlewInitResult = glewInit();
  if (GLEW_OK != GlewInitResult)
  {
    printf("ERROR: %s", glewGetErrorString(GlewInitResult));
    exit(EXIT_FAILURE);
  }
  // set background color to Black    
  glClearColor(0.0, 0.0, 0.0, 0.0);
  renderer_.Init();
  renderer_.Bind(&tap_camera_);
  renderer_.UpdateViewport();
  // set shade model to Flat    
  glShadeModel(GL_FLAT);

  tap_camera_.SetFlip(1.f, -1.f, -1.f);
}

#endif
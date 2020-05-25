#include "platform.h"
#include "intrinsics.h"

#include <android/sensor.h>
#include <android/log.h>
#include <android_native_app_glue.h>

#include <EGL/egl.h>
#include <GLES/gl.h>
#include <GLES/glext.h>

#include <sys/mman.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, \
																						 "native-activity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, \
																						 "native-activity", __VA_ARGS__))

struct callback_globals
{
	s32 TouchX;
	s32 TouchY;

	s32 ScreenWidth;
	s32 ScreenHeight; 

	EGLSurface Surface;
	EGLDisplay Display;
	EGLContext Context;
};

internal_function s32
AndroidInputCallback(android_app* App, AInputEvent* Event)
{
	callback_globals* CallbackGlobals = (callback_globals*)App->userData;

	switch(AInputEvent_getType(Event))
	{
		case AINPUT_EVENT_TYPE_MOTION:
			{
				CallbackGlobals->TouchX = AMotionEvent_getX(Event, 0);
				CallbackGlobals->TouchY = AMotionEvent_getY(Event, 0);
				LOGI("Touch x:%d y:%d", CallbackGlobals->TouchX, CallbackGlobals->TouchY);
				return 1;
			} break;
	}

	return 0;
}

internal_function void
AndroidOSWantsToTalkCallback(android_app* App, s32 Cmd)
{
	callback_globals* CallbackGlobals = (callback_globals*)App->userData;

	switch(Cmd)
	{
		case APP_CMD_SAVE_STATE:
			{
			} break;
		case APP_CMD_INIT_WINDOW:
			{
				//NOTE(bjorn): We can only initialize OpenGl first when we have 
				//a "ANativeWindow" surface to use.
				if(App->window != 0)
				{
					//NOTE(bjorn): Initialize OpenGL.
					EGLint Attribs[] = 
					{
						//EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
						EGL_BLUE_SIZE, 8,
						EGL_GREEN_SIZE, 8,
						EGL_RED_SIZE, 8,
						EGL_ALPHA_SIZE, 8,
						EGL_NONE
					};

					EGLDisplay Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

					eglInitialize(Display, 0, 0);

					EGLint NumberOfConfigs;
					eglChooseConfig(Display, Attribs, 0, 0, &NumberOfConfigs);

					EGLConfig Configurations[256];

					eglChooseConfig(Display, Attribs, 
													(EGLConfig*)&Configurations, 256, &NumberOfConfigs);
					Assert(NumberOfConfigs <= 256);

					EGLConfig MatchingConfiguration = 0;
					for(s32 Index = 0; 
							Index < NumberOfConfigs; 
							Index++)
					{
						EGLConfig Config = Configurations[Index];

						EGLint R, G, B, A;
						if(eglGetConfigAttrib(Display, Config, EGL_RED_SIZE,   &R) &&
							 eglGetConfigAttrib(Display, Config, EGL_GREEN_SIZE, &G) &&
							 eglGetConfigAttrib(Display, Config, EGL_BLUE_SIZE,  &B) &&
							 eglGetConfigAttrib(Display, Config, EGL_ALPHA_SIZE, &A) &&
							 R == 8 && G == 8 && B == 8 && A == 8 ) 
						{

							MatchingConfiguration = Config;
							break;
						}
					}
					if (!MatchingConfiguration) {
						LOGW("No matching GLES configuration!");
						MatchingConfiguration = Configurations[0];
					}

					Assert(MatchingConfiguration);

					EGLint Format;
					/* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
					 * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
					 * As soon as we picked a EGLConfig, we can safely reconfigure the
					 * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
					eglGetConfigAttrib(Display, MatchingConfiguration, 
														 EGL_NATIVE_VISUAL_ID, &Format);

					EGLSurface Surface = 
						eglCreateWindowSurface(Display, MatchingConfiguration, App->window, 0);
					EGLContext Context = 
						eglCreateContext(Display, MatchingConfiguration, 0, 0);
					Assert(Surface);
					Assert(Context);

					if (eglMakeCurrent(Display, Surface, Surface, Context) == EGL_FALSE) {
						LOGW("Unable to eglMakeCurrent.");
						return;
					}

					EGLint Width, Height;
					eglQuerySurface(Display, Surface, EGL_WIDTH, &Width);
					eglQuerySurface(Display, Surface, EGL_HEIGHT, &Height);

					CallbackGlobals->Display = Display;
					CallbackGlobals->Surface = Surface;
					CallbackGlobals->Context = Surface;
					CallbackGlobals->ScreenWidth = Width;
					CallbackGlobals->ScreenHeight = Height;

					LOGI("OpenGL Vendor: %s Renderer: %s Version: %s Extensions: %s", 
							 glGetString(GL_VENDOR), 
							 glGetString(GL_RENDERER), 
							 glGetString(GL_VERSION), 
							 glGetString(GL_EXTENSIONS));

					glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
					glEnable(GL_CULL_FACE);
					glShadeModel(GL_SMOOTH);
					glDisable(GL_DEPTH_TEST);
				}
			} break;
		case APP_CMD_TERM_WINDOW:
			{
				//NOTE(bjorn): This happens both when window is being
				// hidden and when it is being closed.
			} break;
		case APP_CMD_GAINED_FOCUS:
			{
			} break;
		case APP_CMD_LOST_FOCUS:
			{
				//NOTE(bjorn): Here is a good time to alleviate the battery.
			} break;
	}
}

	void 
android_main(android_app* App) 
{
	callback_globals CallbackGlobals = {};

	App->userData = &CallbackGlobals;
	App->onAppCmd = AndroidOSWantsToTalkCallback;
	App->onInputEvent = AndroidInputCallback;

	if (App->savedState == 0) 
	{
		//NOTE(bjorn): App is starting fresh.
	}
	else
	{
		//NOTE(bjorn): App is recovering with saved game-state.
	}

	//NOTE(bjorn): Resolution on my phone is 1920x1080 -> half is 910x540.
	s32 GameScreenWidth = 910;
	s32 GameScreenHeight = 540;
	s32 BytesPerPixel = 4;
	void* PixelBuffer = mmap(0, GameScreenWidth * GameScreenHeight * BytesPerPixel,
													 PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);

	GLuint TextureHandle;
	glGenTextures(1, &TextureHandle);

	while(1)
	{
		s32 ID;
		s32 NumberOfEvents;
		android_poll_source* PollSource;
		while((ID = ALooper_pollAll(0, 0, &NumberOfEvents, (void**)&PollSource)) >= 0)
		{
			if(PollSource != 0)
			{
				PollSource->process(App, PollSource);
			}

			/*
				 if(ID == LOOPER_ID_USER)
				 {
			//TODO(bjorn): What is this?
			}
			*/
		}

		{
			u8* Row = (u8*)PixelBuffer;
			s32 Pitch = GameScreenWidth * BytesPerPixel;
			for(s32 Y = 0;
					Y < GameScreenHeight;
					Y++)
			{
				u8* Pixel = Row;
				for(s32 X = 0;
						X < GameScreenWidth;
						X++)
				{
					//                                                              high     low
					// NOTE(bjorn): Expected pixel layout in memory is top to bottom AA RR GG BB.
					if(((CallbackGlobals.TouchY*0.5 - 10) < X) && (X < (CallbackGlobals.TouchY*0.5 + 10)) &&
						 (((CallbackGlobals.ScreenWidth - CallbackGlobals.TouchX)*0.5 - 10) < Y) &&
						 (Y < ((CallbackGlobals.ScreenWidth - CallbackGlobals.TouchX)*0.5 + 10)))
					{
						*Pixel++ = 255;
						*Pixel++ = 0;
						*Pixel++ = 0;
						*Pixel++ = 255;
					}
					else
					{
						*Pixel++ = 0;
						*Pixel++ = (X * 255) / GameScreenWidth;
						*Pixel++ = (Y * 255) / GameScreenHeight;
						*Pixel++ = 255;
					}
				}
				Row += Pitch;
			}
		}


		glBindTexture(GL_TEXTURE_2D, TextureHandle);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, 
								 GL_RGBA, 
								 GameScreenWidth, GameScreenHeight, 
								 0, 
								 //GL_BGRA_EXT, 
								 GL_RGBA, 
								 GL_UNSIGNED_BYTE,
								 PixelBuffer);
		glEnable(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		glViewport(0, 0, CallbackGlobals.ScreenWidth, CallbackGlobals.ScreenHeight);
		glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

		GLfloat Verticies[] = {
			-1.0f, -1.0f,
			1.0f, -1.0f,
			1.0f,  1.0f,

			-1.0f, -1.0f,
			1.0f,  1.0f,
			-1.0f,  1.0f};

		GLfloat UV[] = {
			1.0f, 1.0f,
			1.0f, 0.0f,
			0.0f, 0.0f,

			1.0f, 1.0f,
			0.0f, 0.0f,
			0.0f, 1.0f,};

		glEnableClientState(GL_VERTEX_ARRAY);
		glVertexPointer(2, GL_FLOAT, 0, Verticies);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glTexCoordPointer(2, GL_FLOAT, 0, UV);

		glDrawArrays(GL_TRIANGLES,0,6);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		eglSwapBuffers(CallbackGlobals.Display, CallbackGlobals.Surface);
	}
}

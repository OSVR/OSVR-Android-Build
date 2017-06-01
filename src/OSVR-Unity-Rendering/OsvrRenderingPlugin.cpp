/** @file
@brief Implementation
@date 2017
@author
Sensics, Inc.
<http://sensics.com/osvr>
*/

// Copyright 2017 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <vector>
#include <sstream>
#include <dlfcn.h>
// Internal includes
#include "OsvrRenderingPlugin.h"
#include "Unity/IUnityGraphics.h"
#include "UnityRendererType.h"

// Library includes
#include <osvr/ClientKit/ContextC.h>
#include <osvr/ClientKit/InterfaceC.h>
#include <osvr/ClientKit/InterfaceStateC.h>
#include <osvr/ClientKit/DisplayC.h>
#include <osvr/ClientKit/InterfaceCallbackC.h>
#include <osvr/ClientKit/ImagingC.h>
#include <osvr/ClientKit/ServerAutoStartC.h>
#include <osvr/RenderKit/RenderManagerC.h>
#include <osvr/RenderKit/RenderManagerOpenGLC.h>
#include <osvr/RenderKit/RenderKitGraphicsTransforms.h>


#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <jni.h>

// VARIABLES
static IUnityInterfaces *s_UnityInterfaces = nullptr;
static IUnityGraphics *s_Graphics = nullptr;
static UnityRendererType s_deviceType;

//static osvr::renderkit::RenderManager::RenderParams s_renderParams;
//static osvr::renderkit::RenderManager *s_render = nullptr;
//static OSVR_ClientContext s_clientContext = nullptr;
//static std::vector<osvr::renderkit::RenderBuffer> s_renderBuffers;
//static std::vector<osvr::renderkit::RenderInfo> s_renderInfo;
//static osvr::renderkit::GraphicsLibrary s_library;

/// @todo is this redundant? (given renderParams)
static double s_nearClipDistance = 0.1;
/// @todo is this redundant? (given renderParams)
static double s_farClipDistance = 1000.0;
/// @todo is this redundant? (given renderParams)
static double s_ipd = 0.063;

// GLES globals
static int gWidth = 0;
static int gHeight = 0;
static GLuint gProgram;
static GLuint gvPositionHandle;
static GLuint gvColorHandle;
static GLuint gvTexCoordinateHandle;
static GLuint guTextureUniformId;
static GLuint gvProjectionUniformId;
static GLuint gvViewUniformId;
static GLuint gvModelUniformId;
static GLuint gframeBuffer;
static GLuint gTextureID;
static GLuint gLeftEyeTextureID;
static GLuint gRightEyeTextureID;
static bool gGraphicsInitializedOnce = false; // if setupGraphics has been called at least once

// OSVR globals
static bool gOSVRInitialized = false;
static bool gRenderManagerInitialized = false;
//static OSVR_DisplayConfig gOSVRDisplayConfig;
static OSVR_ClientContext gClientContext = NULL;
static OSVR_ClientInterface gCamera = NULL;
static OSVR_ClientInterface gHead = NULL;
static int gReportNumber = 0;
static OSVR_ImageBufferElement *gLastFrame = nullptr;
static GLuint gLastFrameWidth = 0;
static GLuint gLastFrameHeight = 0;
static GLubyte *gTextureBuffer = nullptr;
static OSVR_GraphicsLibraryOpenGL gGraphicsLibrary = { 0 };
static OSVR_RenderManager gRenderManager = nullptr;
static OSVR_RenderManagerOpenGL gRenderManagerOGL = nullptr;
static OSVR_RenderParams gRenderParams = { 0 };
static std::vector<OSVR_RenderBufferOpenGL> buffers;
static bool contextSet = false;

typedef struct OSVR_RenderTargetInfo {
    GLuint colorBufferName;
    GLuint depthBufferName;
    GLuint frameBufferName;
    GLuint renderBufferName; // @todo - do we need this?
} OSVR_RenderTargetInfo;

static const char gVertexShader[] =
            "uniform mat4 model;\n"
                    "uniform mat4 view;\n"
                    "uniform mat4 projection;\n"
                    "attribute vec4 vPosition;\n"
                    "attribute vec4 vColor;\n"
                    "attribute vec2 vTexCoordinate;\n"
                    "varying vec2 texCoordinate;\n"
                    "varying vec4 fragmentColor;\n"
                    "void main() {\n"
                    "  gl_Position = projection * view * model * vPosition;\n"
                    "  fragmentColor = vColor;\n"
                    "  texCoordinate = vTexCoordinate;\n"
                    "}\n";

    static const char gFragmentShader[] =
            "precision mediump float;\n"
                    "uniform sampler2D uTexture;\n"
                    "varying vec2 texCoordinate;\n"
                    "varying vec4 fragmentColor;\n"
                    "void main()\n"
                    "{\n"
                    "    gl_FragColor = fragmentColor * texture2D(uTexture, texCoordinate);\n"
                    //"    gl_FragColor = texture2D(uTexture, texCoordinate);\n"
                    "}\n";

static std::vector<OSVR_RenderTargetInfo> gRenderTargets;
static GLuint gFrameBuffer;


// RenderEvents
// Called from Unity with GL.IssuePluginEvent
enum RenderEvents {
    kOsvrEventID_Render = 0,
    kOsvrEventID_Shutdown = 1,
    kOsvrEventID_Update = 2,
    kOsvrEventID_SetRoomRotationUsingHead = 3,
    kOsvrEventID_ClearRoomToWorldTransform = 4
};

/*
// --------------------------------------------------------------------------
// Helper utilities
// Allow writing to the Unity debug console from inside DLL land.
static DebugFnPtr s_//DebugLog = nullptr;
void UNITY_INTERFACE_API LinkDebug(DebugFnPtr d) { s_//DebugLog = d; }

// Only for debugging purposes, as this causes some errors at shutdown
inline void //DebugLog(const char *str) {
#if !defined(NDEBUG) || defined(ENABLE_LOGGING)
    if (s_//DebugLog != nullptr) {
        s_//DebugLog(str);
    }
#endif // !defined(NDEBUG) || defined(ENABLE_LOGGING)

#if defined(ENABLE_LOGGING) && defined(ENABLE_LOGFILE)
    if (s_//DebugLogFile) {
        s_//DebugLogFile << str << std::endl;
    }
#endif // defined(ENABLE_LOGGING) && defined(ENABLE_LOGFILE)
}
*/

inline osvr::renderkit::OSVR_ProjectionMatrix ConvertProjectionMatrix(::OSVR_ProjectionMatrix matrix)
    {
        osvr::renderkit::OSVR_ProjectionMatrix ret = { 0 };
        ret.bottom = matrix.bottom;
        ret.top = matrix.top;
        ret.left = matrix.left;
        ret.right = matrix.right;
        ret.nearClip = matrix.nearClip;
        ret.farClip = matrix.farClip;
        return ret;
    }

    static void checkReturnCode(OSVR_ReturnCode returnCode, const char *msg) {
        if (returnCode != OSVR_RETURN_SUCCESS) {
            //LOGI("[OSVR] OSVR method returned a failure: %s", msg);
            throw std::runtime_error(msg);
        }
    }
// RAII wrapper around the RenderManager collection APIs for OpenGL
class RenderInfoCollectionOpenGL {
private:
	OSVR_RenderManager mRenderManager = nullptr;
	OSVR_RenderInfoCollection mRenderInfoCollection = nullptr;
	OSVR_RenderParams mRenderParams = { 0 };

public:
	RenderInfoCollectionOpenGL(OSVR_RenderManager renderManager, OSVR_RenderParams renderParams)
		: mRenderManager(renderManager), mRenderParams(renderParams) {
		OSVR_ReturnCode rc;
		rc = osvrRenderManagerGetRenderInfoCollection(mRenderManager, mRenderParams, &mRenderInfoCollection);
		checkReturnCode(rc, "osvrRenderManagerGetRenderInfoCollection call failed.");
	}

	OSVR_RenderInfoCount getNumRenderInfo() {
		OSVR_RenderInfoCount ret;
		OSVR_ReturnCode rc;
		rc = osvrRenderManagerGetNumRenderInfoInCollection(mRenderInfoCollection, &ret);
		checkReturnCode(rc, "osvrRenderManagerGetNumRenderInfoInCollection call failed.");
		return ret;
	}

	OSVR_RenderInfoOpenGL getRenderInfo(OSVR_RenderInfoCount index) {
		if (index < 0 || index >= getNumRenderInfo()) {
			const static char* err = "getRenderInfo called with invalid index";
			//LOGE(err);
			throw std::runtime_error(err);
		}
		OSVR_RenderInfoOpenGL ret;
		OSVR_ReturnCode rc;
		rc = osvrRenderManagerGetRenderInfoFromCollectionOpenGL(mRenderInfoCollection, index, &ret);
		checkReturnCode(rc, "osvrRenderManagerGetRenderInfoFromCollectionOpenGL call failed.");
		return ret;
	}

	~RenderInfoCollectionOpenGL() {
		if (mRenderInfoCollection) {
			osvrRenderManagerReleaseRenderInfoCollection(mRenderInfoCollection);
		}
	}
};

static void checkGlError(const char *op) {
	std::stringstream ss;
	for (GLint error = glGetError(); error; error = glGetError()) {
		// gluErrorString without glu
		std::string errorString;
		switch (error) {
		case GL_NO_ERROR:
			errorString = "GL_NO_ERROR";
			break;
		case GL_INVALID_ENUM:
			errorString = "GL_INVALID_ENUM";
			break;
		case GL_INVALID_VALUE:
			errorString = "GL_INVALID_VALUE";
			break;
		case GL_INVALID_OPERATION:
			errorString = "GL_INVALID_OPERATION";
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			errorString = "GL_INVALID_FRAMEBUFFER_OPERATION";
			break;
		case GL_OUT_OF_MEMORY:
			errorString = "GL_OUT_OF_MEMORY";
			break;
		default:
			errorString = "(unknown error)";
			break;
		}
		//LOGI("after %s() glError (%s)\n", op, errorString.c_str());
	}
}

class PassThroughOpenGLContextImpl {
	OSVR_OpenGLToolkitFunctions toolkit;
	int mWidth;
	int mHeight;

	static void createImpl(void* data) {
	}
	static void destroyImpl(void* data) {
		delete ((PassThroughOpenGLContextImpl*)data);
	}
	static OSVR_CBool addOpenGLContextImpl(void* data, const OSVR_OpenGLContextParams* p) {
		return ((PassThroughOpenGLContextImpl*)data)->addOpenGLContext(p);
	}
	static OSVR_CBool removeOpenGLContextsImpl(void* data) {
		return ((PassThroughOpenGLContextImpl*)data)->removeOpenGLContexts();
	}
	static OSVR_CBool makeCurrentImpl(void* data, size_t display) {
		return ((PassThroughOpenGLContextImpl*)data)->makeCurrent(display);
	}
	static OSVR_CBool swapBuffersImpl(void* data, size_t display) {
		return ((PassThroughOpenGLContextImpl*)data)->swapBuffers(display);
	}
	static OSVR_CBool setVerticalSyncImpl(void* data, OSVR_CBool verticalSync) {
		return ((PassThroughOpenGLContextImpl*)data)->setVerticalSync(verticalSync);
	}
	static OSVR_CBool handleEventsImpl(void* data) {
		return ((PassThroughOpenGLContextImpl*)data)->handleEvents();
	}
	static OSVR_CBool getDisplayFrameBufferImpl(void* data, size_t display, GLuint* displayFrameBufferOut) {
		return ((PassThroughOpenGLContextImpl*)data)->getDisplayFrameBuffer(display, displayFrameBufferOut);
	}
	static OSVR_CBool getDisplaySizeOverrideImpl(void* data, size_t display, int* width, int* height) {
		return ((PassThroughOpenGLContextImpl*)data)->getDisplaySizeOverride(display, width, height);
	}


public:
	PassThroughOpenGLContextImpl() {
		memset(&toolkit, 0, sizeof(toolkit));
		toolkit.size = sizeof(toolkit);
		toolkit.data = this;

		toolkit.create = createImpl;
		toolkit.destroy = destroyImpl;
		toolkit.addOpenGLContext = addOpenGLContextImpl;
		toolkit.removeOpenGLContexts = removeOpenGLContextsImpl;
		toolkit.makeCurrent = makeCurrentImpl;
		toolkit.swapBuffers = swapBuffersImpl;
		toolkit.setVerticalSync = setVerticalSyncImpl;
		toolkit.handleEvents = handleEventsImpl;
		toolkit.getDisplaySizeOverride = getDisplaySizeOverrideImpl;
		toolkit.getDisplayFrameBuffer = getDisplayFrameBufferImpl;
	}

	~PassThroughOpenGLContextImpl() {
	}

	const OSVR_OpenGLToolkitFunctions* getToolkit() const { return &toolkit; }

	bool addOpenGLContext(const OSVR_OpenGLContextParams* p) {
		return true;
	}

	bool removeOpenGLContexts() {
		return true;
	}

	bool makeCurrent(size_t display) {
		return true;
	}

	bool swapBuffers(size_t display) {
		return true;
	}

	bool setVerticalSync(bool verticalSync) {
		return true;
	}

	bool handleEvents() {
		return true;
	}
	bool getDisplayFrameBuffer(size_t display, GLuint* displayFrameBufferOut) {
		*displayFrameBufferOut = gFrameBuffer;
		return true;
	}

	bool getDisplaySizeOverride(size_t display, int* width, int* height) {
		*width = gWidth;
		*height = gHeight;
		return false;
	}
};

static GLuint loadShader(GLenum shaderType, const char *pSource) {
        GLuint shader = glCreateShader(shaderType);
        if (shader) {
            glShaderSource(shader, 1, &pSource, NULL);
            glCompileShader(shader);
            GLint compiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
            if (!compiled) {
                GLint infoLen = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
                if (infoLen) {
                    char *buf = (char *) malloc(infoLen);
                    if (buf) {
                        glGetShaderInfoLog(shader, infoLen, NULL, buf);
                        //LOGE("Could not compile shader %d:\n%s\n",
                            // shaderType, buf);
                        free(buf);
                    }
                    glDeleteShader(shader);
                    shader = 0;
                }
            }
        }
        return shader;
    }

static GLuint createProgram(const char *pVertexSource, const char *pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");

        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");

        glBindAttribLocation(program, 0, "vPosition");
        glBindAttribLocation(program, 1, "vColor");
        glBindAttribLocation(program, 2, "vTexCoordinate");

        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char *buf = (char *) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                   // LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

static GLuint createTexture(GLuint width, GLuint height) {
	GLuint ret;
	glGenTextures(1, &ret);
	checkGlError("glGenTextures");

	glBindTexture(GL_TEXTURE_2D, ret);
	checkGlError("glBindTexture");

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	//    // DEBUG CODE - should be passing null here, but then texture is always black.
	GLubyte *dummyBuffer = new GLubyte[width * height * 4];
	for (GLuint i = 0; i < width * height * 4; i++) {
		dummyBuffer[i] = (i % 4 ? 100 : 255);
	}

	// This dummy texture successfully makes it into the texture and renders, but subsequent
	// calls to glTexSubImage2D don't appear to do anything.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		dummyBuffer);
	checkGlError("glTexImage2D");
	delete[] dummyBuffer;

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	checkGlError("glTexParameteri");

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	checkGlError("glTexParameteri");
	return ret;
}

static void updateTexture(GLuint width, GLuint height, GLubyte *data) {

	glBindTexture(GL_TEXTURE_2D, gTextureID);
	checkGlError("glBindTexture");

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);

	// @todo use glTexSubImage2D to be faster here, but add check to make sure height/width are the same.
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
	//checkGlError("glTexSubImage2D");
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	checkGlError("glTexImage2D");
}

static void imagingCallback(void *userdata, const OSVR_TimeValue *timestamp,
	const OSVR_ImagingReport *report) {

	OSVR_ClientContext *ctx = (OSVR_ClientContext *)userdata;

	gReportNumber++;
	GLuint width = report->state.metadata.width;
	GLuint height = report->state.metadata.height;
	gLastFrameWidth = width;
	gLastFrameHeight = height;
	GLuint size = width * height * 4;

	gLastFrame = report->state.data;
}
#if SUPPORT_OPENGL
inline GLuint GetEyeTextureOpenGL(int eye) {
    return (eye == 0) ? gLeftEyeTextureID : gRightEyeTextureID;
}
#endif
static bool setupRenderTextures(OSVR_RenderManager renderManager) {
	try {
		OSVR_ReturnCode rc;
		rc = osvrRenderManagerGetDefaultRenderParams(&gRenderParams);
		checkReturnCode(rc, "osvrRenderManagerGetDefaultRenderParams call failed.");

		gRenderParams.farClipDistanceMeters = 1000000.0f;
		gRenderParams.nearClipDistanceMeters = 0.0000001f;
		RenderInfoCollectionOpenGL renderInfo(renderManager, gRenderParams);

		OSVR_RenderManagerRegisterBufferState state;
		rc = osvrRenderManagerStartRegisterRenderBuffers(&state);
		checkReturnCode(rc, "osvrRenderManagerStartRegisterRenderBuffers call failed.");

		for (OSVR_RenderInfoCount i = 0; i < renderInfo.getNumRenderInfo(); i++) {
			OSVR_RenderInfoOpenGL currentRenderInfo = renderInfo.getRenderInfo(i);

			// Determine the appropriate size for the frame buffer to be used for
			// all eyes when placed horizontally size by side.
			int width = static_cast<int>(currentRenderInfo.viewport.width);
			int height = static_cast<int>(currentRenderInfo.viewport.height);

			GLuint frameBufferName = 0;
			glGenFramebuffers(1, &frameBufferName);
			glBindFramebuffer(GL_FRAMEBUFFER, frameBufferName);

			GLuint renderBufferName = 0;
			glGenRenderbuffers(1, &renderBufferName);

			GLuint colorBufferName = GetEyeTextureOpenGL(i);
			rc = osvrRenderManagerCreateColorBufferOpenGL(width, height, GL_RGBA,
				&colorBufferName);
			checkReturnCode(rc, "osvrRenderManagerCreateColorBufferOpenGL call failed.");

			// bind it to our framebuffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
				colorBufferName, 0);

			// The depth buffer
			GLuint depthBuffer;
			rc = osvrRenderManagerCreateDepthBufferOpenGL(width, height, &depthBuffer);
			checkReturnCode(rc, "osvrRenderManagerCreateDepthBufferOpenGL call failed.");

			glGenRenderbuffers(1, &depthBuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);

			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
				depthBuffer);

			glBindRenderbuffer(GL_RENDERBUFFER, renderBufferName);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBufferName, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderBufferName);


			// unbind the framebuffer
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, gFrameBuffer);

			OSVR_RenderBufferOpenGL buffer = { 0 };
			buffer.colorBufferName = colorBufferName;
			buffer.depthStencilBufferName = depthBuffer;
			rc = osvrRenderManagerRegisterRenderBufferOpenGL(state, buffer);
			checkReturnCode(rc, "osvrRenderManagerRegisterRenderBufferOpenGL call failed.");

			OSVR_RenderTargetInfo renderTarget = { 0 };
			renderTarget.frameBufferName = frameBufferName;
			renderTarget.renderBufferName = renderBufferName;
			renderTarget.colorBufferName = colorBufferName;
			renderTarget.depthBufferName = depthBuffer;
			gRenderTargets.push_back(renderTarget);
		}

		rc = osvrRenderManagerFinishRegisterRenderBuffers(renderManager, state, true);
		checkReturnCode(rc, "osvrRenderManagerFinishRegisterRenderBuffers call failed.");
	}
	catch (...) {
		//LOGE("Error durring render target creation.");
		return false;
	}
	return true;
}

static bool setupOSVR() {
	if (gOSVRInitialized) {
		return true;
	}
	OSVR_ReturnCode rc = 0;
	try {
		// On Android, the current working directory is added to the default plugin search path.
		// it also helps the server find its configuration and display files.
		//            boost::filesystem::current_path("/data/data/com.osvr.android.gles2sample/files");
		//            auto workingDirectory = boost::filesystem::current_path();
		//            //LOGI("[OSVR] Current working directory: %s", workingDirectory.string().c_str());

		// auto-start the server
		osvrClientAttemptServerAutoStart();

		if (!gClientContext) {
			//LOGI("[OSVR] Creating ClientContext...");
			gClientContext = osvrClientInit("com.osvr.android.examples.OSVROpenGL", 0);
			if (!gClientContext) {
				//LOGI("[OSVR] could not create client context");
				return false;
			}

			// temporary workaround to DisplayConfig issue,
			// display sometimes fails waiting for the tree from the server.
			//LOGI("[OSVR] Calling update a few times...");
			for (int i = 0; i < 10000; i++) {
				rc = osvrClientUpdate(gClientContext);
				if (rc != OSVR_RETURN_SUCCESS) {
					//LOGI("[OSVR] Error while updating client context.");
					return false;
				}
			}


			rc = osvrClientCheckStatus(gClientContext);
			if (rc != OSVR_RETURN_SUCCESS) {
				//LOGI("[OSVR] Client context reported bad status.");
				return false;
			}
			else {
				//LOGI("[OSVR] Client context reported good status.");
			}


			//                if (OSVR_RETURN_SUCCESS !=
			//                    osvrClientGetInterface(gClientContext, "/camera", &gCamera)) {
			//                    //LOGI("Error, could not get the camera interface at /camera.");
			//                    return false;
			//                }
			//
			//                // Register the imaging callback.
			//                if (OSVR_RETURN_SUCCESS !=
			//                    osvrRegisterImagingCallback(gCamera, &imagingCallback, &gClientContext)) {
			//                    //LOGI("Error, could not register image callback.");
			//                    return false;
			//                }
		}

		gOSVRInitialized = true;
		return true;
	}
	catch (const std::runtime_error &ex) {
		//LOGI("[OSVR] OSVR initialization failed: %s", ex.what());
		return false;
	}
}

// Idempotent call to setup render manager
static bool setupRenderManager() {
	if (!gOSVRInitialized || !gGraphicsInitializedOnce) {
		return false;
	}
	if (gRenderManagerInitialized) {
		return true;
	}
	try {
		PassThroughOpenGLContextImpl* glContextImpl = new PassThroughOpenGLContextImpl();
		gGraphicsLibrary.toolkit = glContextImpl->getToolkit();

		if (OSVR_RETURN_SUCCESS != osvrCreateRenderManagerOpenGL(
			gClientContext, "OpenGL", gGraphicsLibrary, &gRenderManager, &gRenderManagerOGL)) {
			std::cerr << "Could not create the RenderManager" << std::endl;
			return false;
		}

		

		// Open the display and make sure this worked
		OSVR_OpenResultsOpenGL openResults;
		if (OSVR_RETURN_SUCCESS != osvrRenderManagerOpenDisplayOpenGL(
			gRenderManagerOGL, &openResults) ||
			(openResults.status == OSVR_OPEN_STATUS_FAILURE)) {
			std::cerr << "Could not open display" << std::endl;
			osvrDestroyRenderManager(gRenderManager);
			gRenderManager = gRenderManagerOGL = nullptr;
			return false;
		}

		gRenderManagerInitialized = true;
		return true;
	}
	catch (const std::runtime_error &ex) {
		//LOGI("[OSVR] RenderManager initialization failed: %s", ex.what());
		return false;
	}
}
static const GLfloat gTriangleColors[] = {
	// white
	1.0f, 1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 1.0f,
	1.0f, 1.0f, 1.0f, 1.0f,

	// green
	0.0f, 0.75f, 0.0f, 1.0f,
	0.0f, 0.75f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f,
	0.0f, 0.75f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f,
	0.0f, 1.0f, 0.0f, 1.0f,

	// blue
	0.0f, 0.0f, 0.75f, 1.0f,
	0.0f, 0.0f, 0.75f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 0.75f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,
	0.0f, 0.0f, 1.0f, 1.0f,

	// green/purple
	0.0f, 0.75f, 0.75f, 1.0f,
	0.0f, 0.75f, 0.75f, 1.0f,
	0.0f, 1.0f, 1.0f, 1.0f,
	0.0f, 0.75f, 0.75f, 1.0f,
	0.0f, 1.0f, 1.0f, 1.0f,
	0.0f, 1.0f, 1.0f, 1.0f,

	// red/green
	0.75f, 0.75f, 0.0f, 1.0f,
	0.75f, 0.75f, 0.0f, 1.0f,
	1.0f, 1.0f, 0.0f, 1.0f,
	0.75f, 0.75f, 0.0f, 1.0f,
	1.0f, 1.0f, 0.0f, 1.0f,
	1.0f, 1.0f, 0.0f, 1.0f,

	// red/blue
	0.75f, 0.0f, 0.75f, 1.0f,
	0.75f, 0.0f, 0.75f, 1.0f,
	1.0f, 0.0f, 1.0f, 1.0f,
	0.75f, 0.0f, 0.75f, 1.0f,
	1.0f, 0.0f, 1.0f, 1.0f,
	1.0f, 0.0f, 1.0f, 1.0f
};

static const GLfloat gTriangleTexCoordinates[] = {
	// A cube face (letters are unique vertices)
	// A--B
	// |  |
	// D--C

	// As two triangles (clockwise)
	// A B D
	// B C D

	// white
	1.0f, 0.0f, // A
	1.0f, 1.0f, // B
	0.0f, 0.0f, // D
	1.0f, 1.0f, // B
	0.0f, 1.0f, // C
	0.0f, 0.0f, // D

	// green
	1.0f, 0.0f, // A
	1.0f, 1.0f, // B
	0.0f, 0.0f, // D
	1.0f, 1.0f, // B
	0.0f, 1.0f, // C
	0.0f, 0.0f, // D

	// blue
	1.0f, 1.0f, // A
	0.0f, 1.0f, // B
	1.0f, 0.0f, // D
	0.0f, 1.0f, // B
	0.0f, 0.0f, // C
	1.0f, 0.0f, // D

	// blue-green
	1.0f, 0.0f, // A
	1.0f, 1.0f, // B
	0.0f, 0.0f, // D
	1.0f, 1.0f, // B
	0.0f, 1.0f, // C
	0.0f, 0.0f, // D

	// yellow
	0.0f, 0.0f, // A
	1.0f, 0.0f, // B
	0.0f, 1.0f, // D
	1.0f, 0.0f, // B
	1.0f, 1.0f, // C
	0.0f, 1.0f, // D

	// purple/magenta
	1.0f, 1.0f, // A
	0.0f, 1.0f, // B
	1.0f, 0.0f, // D
	0.0f, 1.0f, // B
	0.0f, 0.0f, // C
	1.0f, 0.0f, // D
};

static const GLfloat gTriangleVertices[] = {
	// A cube face (letters are unique vertices)
	// A--B
	// |  |
	// D--C

	// As two triangles (clockwise)
	// A B D
	// B C D

	//glNormal3f(0.0, 0.0, -1.0);
	1.0f, 1.0f, -1.0f, // A
	1.0f, -1.0f, -1.0f, // B
	-1.0f, 1.0f, -1.0f, // D
	1.0f, -1.0f, -1.0f, // B
	-1.0f, -1.0f, -1.0f, // C
	-1.0f, 1.0f, -1.0f, // D

	//glNormal3f(0.0, 0.0, 1.0);
	-1.0f, 1.0f, 1.0f, // A
	-1.0f, -1.0f, 1.0f, // B
	1.0f, 1.0f, 1.0f, // D
	-1.0f, -1.0f, 1.0f, // B
	1.0f, -1.0f, 1.0f, // C
	1.0f, 1.0f, 1.0f, // D

	//        glNormal3f(0.0, -1.0, 0.0);
	1.0f, -1.0f, 1.0f, // A
	-1.0f, -1.0f, 1.0f, // B
	1.0f, -1.0f, -1.0f, // D
	-1.0f, -1.0f, 1.0f, // B
	-1.0f, -1.0f, -1.0f, // C
	1.0f, -1.0f, -1.0f, // D

	//        glNormal3f(0.0, 1.0, 0.0);
	1.0f, 1.0f, 1.0f, // A
	1.0f, 1.0f, -1.0f, // B
	-1.0f, 1.0f, 1.0f, // D
	1.0f, 1.0f, -1.0f, // B
	-1.0f, 1.0f, -1.0f, // C
	-1.0f, 1.0f, 1.0f, // D

	//        glNormal3f(-1.0, 0.0, 0.0);
	-1.0f, 1.0f, 1.0f, // A
	-1.0f, 1.0f, -1.0f, // B
	-1.0f, -1.0f, 1.0f, // D
	-1.0f, 1.0f, -1.0f, // B
	-1.0f, -1.0f, -1.0f, // C
	-1.0f, -1.0f, 1.0f, // D

	//        glNormal3f(1.0, 0.0, 0.0);
	1.0f, -1.0f, 1.0f, // A
	1.0f, -1.0f, -1.0f, // B
	1.0f, 1.0f, 1.0f, // D
	1.0f, -1.0f, -1.0f, // B
	1.0f, 1.0f, -1.0f, // C
	1.0f, 1.0f, 1.0f // D
};

static bool setupGraphics(int width, int height) {
	//printGLString("Version", GL_VERSION);
	//printGLString("Vendor", GL_VENDOR);
	//printGLString("Renderer", GL_RENDERER);
	//printGLString("Extensions", GL_EXTENSIONS);

	//initializeGLES2Ext();
	GLint frameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, &frameBuffer);
	gFrameBuffer = (GLuint)frameBuffer;
	//LOGI("Window GL_FRAMEBUFFER_BINDING: %d", gFrameBuffer);

	//LOGI("setupGraphics(%d, %d)", width, height);
	gWidth = width;
	gHeight = height;

	//bool osvrSetupSuccess = setupOSVR();

	gProgram = createProgram(gVertexShader, gFragmentShader);
	if (!gProgram) {
		//LOGE("Could not create program.");
		return false;
	}
	gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
	checkGlError("glGetAttribLocation");
	//LOGI("glGetAttribLocation(\"vPosition\") = %d\n", gvPositionHandle);

	gvColorHandle = glGetAttribLocation(gProgram, "vColor");
	checkGlError("glGetAttribLocation");
	//LOGI("glGetAttribLocation(\"vColor\") = %d\n", gvColorHandle);

	gvTexCoordinateHandle = glGetAttribLocation(gProgram, "vTexCoordinate");
	checkGlError("glGetAttribLocation");
	//LOGI("glGetAttribLocation(\"vTexCoordinate\") = %d\n", gvTexCoordinateHandle);

	gvProjectionUniformId = glGetUniformLocation(gProgram, "projection");
	gvViewUniformId = glGetUniformLocation(gProgram, "view");
	gvModelUniformId = glGetUniformLocation(gProgram, "model");
	guTextureUniformId = glGetUniformLocation(gProgram, "uTexture");

	glViewport(0, 0, width, height);
	checkGlError("glViewport");

	glDisable(GL_CULL_FACE);

	// @todo can we resize the texture after it has been created?
	// if not, we may have to delete the dummy one and create a new one after
	// the first imaging report.
	//LOGI("Creating texture... here we go!");

	gTextureID = createTexture(width, height);

	//return osvrSetupSuccess;
	gGraphicsInitializedOnce = true;
	return true;
}

/**
* Just the current frame in the display.
*/
static void renderFrame() {
	if (!gOSVRInitialized) {
		// @todo implement some logging/error handling?
		return;
	}

	// this call is idempotent, so we can make it every frame.
	// have to ensure render manager is setup from the rendering thread with
	// a current GLES context, so this is a lazy setup call
	if (!setupRenderManager()) {
		// @todo implement some logging/error handling?
		return;
	}

	OSVR_ReturnCode rc;
	//glUseProgram(gProgram);
	//glBindFramebuffer(GL_FRAMEBUFFER, gFrameBuffer);

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	checkGlError("glClearColor");
	glViewport(0, 0, gWidth, gHeight);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	checkGlError("glClear");

/*	GLint maxVertexAttribs;
	glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);

	for (GLuint i = 0; i < maxVertexAttribs; i++) {
		glDisableVertexAttribArray(static_cast<GLuint>(i));
	}
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);*/
	//bindVertexArrayOES(0);

	if (gRenderManager && gClientContext) {
		osvrClientUpdate(gClientContext);
		if (gLastFrame != nullptr) {
			updateTexture(gLastFrameWidth, gLastFrameHeight, gLastFrame);
			osvrClientFreeImage(gClientContext, gLastFrame);
			gLastFrame = nullptr;
		}

		OSVR_RenderParams renderParams;
		rc = osvrRenderManagerGetDefaultRenderParams(&renderParams);
		checkReturnCode(rc, "osvrRenderManagerGetDefaultRenderParams call failed.");

		RenderInfoCollectionOpenGL renderInfoCollection(gRenderManager, renderParams);

		// Get the present started
		OSVR_RenderManagerPresentState presentState;
		rc = osvrRenderManagerStartPresentRenderBuffers(&presentState);
		checkReturnCode(rc, "osvrRenderManagerStartPresentRenderBuffers call failed.");

		for (OSVR_RenderInfoCount renderInfoCount = 0;
			renderInfoCount < renderInfoCollection.getNumRenderInfo();
			renderInfoCount++) {

			// get the current render info
			OSVR_RenderInfoOpenGL currentRenderInfo = renderInfoCollection.getRenderInfo(renderInfoCount);
// Set color and depth buffers for the frame buffer
            OSVR_RenderTargetInfo renderTargetInfo = gRenderTargets[renderInfoCount];

			/// get the eye pose for the current render info
			/*double viewMatd[OSVR_MATRIX_SIZE];
			OSVR_PoseState_to_OpenGL(viewMatd, currentRenderInfo.pose);

			// RenderManager's utilities only support doubles, but we need floats in ES2 land
			GLfloat viewMat[OSVR_MATRIX_SIZE];
			for (int i = 0; i < OSVR_MATRIX_SIZE; i++) {
				viewMat[i] = static_cast<GLfloat>(viewMatd[i]);
			}

			// Set color and depth buffers for the frame buffer
			OSVR_RenderTargetInfo renderTargetInfo = gRenderTargets[renderInfoCount];
			glBindFramebuffer(GL_FRAMEBUFFER, renderTargetInfo.frameBufferName);

			// @todo: convert to OpenGL?
			glViewport(static_cast<GLint>(currentRenderInfo.viewport.left),
				static_cast<GLint>(currentRenderInfo.viewport.lower),
				static_cast<GLsizei>(currentRenderInfo.viewport.width),
				static_cast<GLsizei>(currentRenderInfo.viewport.height));

			//                glViewport(static_cast<GLint>(renderInfoCount == 0 ? 0 : currentRenderInfo.viewport.width),
			//                           static_cast<GLint>(currentRenderInfo.viewport.lower),
			//                           static_cast<GLsizei>(currentRenderInfo.viewport.width),
			//                           static_cast<GLsizei>(currentRenderInfo.viewport.height));

			/// Set the OpenGL projection matrix
			double projMatd[OSVR_MATRIX_SIZE];
			OSVR_Projection_to_OpenGL(projMatd, currentRenderInfo.projection);

			// RenderManager's utilities only support doubles, but we need floats in GLES2 land
			GLfloat projMat[OSVR_MATRIX_SIZE];
			for (int i = 0; i < OSVR_MATRIX_SIZE; i++) {
				projMat[i] = static_cast<GLfloat>(projMatd[i]);
			}

			const static GLfloat identityMat4f[16] = {
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f,
			};

			/// Call out to render our scene.
			glUseProgram(gProgram);
			checkGlError("glUseProgram");

			glUniformMatrix4fv(gvProjectionUniformId, 1, GL_FALSE, projMat);
			glUniformMatrix4fv(gvViewUniformId, 1, GL_FALSE, viewMat);
			glUniformMatrix4fv(gvModelUniformId, 1, GL_FALSE, identityMat4f);
			checkGlError("one of the glUniformMatrix4fv calls?");

			glEnableVertexAttribArray(gvPositionHandle);
			checkGlError("glEnableVertexAttribArray");
			glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, gTriangleVertices);
			checkGlError("glVertexAttribPointer");

			glEnableVertexAttribArray(gvColorHandle);
			checkGlError("glEnableVertexAttribArray");
			glVertexAttribPointer(gvColorHandle, 4, GL_FLOAT, GL_FALSE, 0, gTriangleColors);
			checkGlError("glVertexAttribPointer");

			glEnableVertexAttribArray(gvTexCoordinateHandle);
			checkGlError("glEnableVertexAttribArray");
			glVertexAttribPointer(gvTexCoordinateHandle, 2, GL_FLOAT, GL_FALSE, 0, gTriangleTexCoordinates);
			checkGlError("glVertexAttribPointer");

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, gTextureID);
			glUniform1i(guTextureUniformId, 0);

			glDrawArrays(GL_TRIANGLES, 0, 36);
			checkGlError("glDrawArrays");

			// unbind the render target
			glBindFramebuffer(GL_FRAMEBUFFER, gFrameBuffer);
*/

			// present this render target (deferred until the finish call below)
			OSVR_ViewportDescription normalizedViewport = { 0 };
			normalizedViewport.left = 0.0f;
			normalizedViewport.lower = 0.0f;
			normalizedViewport.width = 1.0f;
			normalizedViewport.height = 1.0f;
			OSVR_RenderBufferOpenGL buffer = { 0 };
			buffer.colorBufferName = GetEyeTextureOpenGL(renderInfoCount);
			buffer.depthStencilBufferName = renderTargetInfo.depthBufferName;




            
			rc = osvrRenderManagerPresentRenderBufferOpenGL(
				presentState, buffer, currentRenderInfo, normalizedViewport);
			checkReturnCode(rc, "osvrRenderManagerPresentRenderBufferOpenGL call failed.");
		}

		// actually kick off the present
		rc = osvrRenderManagerFinishPresentRenderBuffers(
			gRenderManager, presentState, renderParams, false);
		checkReturnCode(rc, "osvrRenderManagerFinishPresentRenderBuffers call failed.");
	}

}


static void stop() {
	//LOGI("[OSVR] Shutting down...");

	if (gRenderManager) {
		osvrDestroyRenderManager(gRenderManager);
		gRenderManager = gRenderManagerOGL = nullptr;
	}

	// is this needed? Maybe not. the display config manages the lifetime.
	if (gClientContext != nullptr) {
		osvrClientShutdown(gClientContext);
		gClientContext = nullptr;
	}

	osvrClientReleaseAutoStartedServer();
}


/*extern "C" {
	JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_initGraphics(JNIEnv * env, jobject obj, jint width, jint height);
	JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_initOSVR(JNIEnv *env, jobject obj);
	JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_step(JNIEnv * env, jobject obj);
	JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_stop(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_initGraphics(JNIEnv * env, jobject obj, jint width, jint height)
{
	OSVROpenGL::setupGraphics(width, height);
}

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_initOSVR(JNIEnv *env, jobject obj)
{
	OSVROpenGL::setupOSVR();
}

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_step(JNIEnv * env, jobject obj)
{
	OSVROpenGL::renderFrame();
}

JNIEXPORT void JNICALL Java_com_osvr_android_gles2sample_MainActivityJNILib_stop(JNIEnv * env, jobject obj)
{
	OSVROpenGL::stop();
}*/
////////////////////////////////////////////////////////////////////////////////////////

//JNI///////////////////////////////////////////
static JNIEnv* jniEnvironment = 0;
static int init = 0;
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    jniEnvironment = 0;
    vm->AttachCurrentThread(&jniEnvironment, 0);
    //init = 100;
    //this OnLoad definitely gets called on the Unity path
  return JNI_VERSION_1_6;
}

static jclass mainActivityClass;
static jmethodID mainActivityClassConstructorMID;
static jmethodID logMsgId;
static jobject unityActivityClassInstance;
void createJavaActivityClassObject(JNIEnv* jni_env) {
  mainActivityClass = jni_env->FindClass("org/osvr/osvrunityandroid/MainActivity");         // find class definition
  mainActivityClassConstructorMID = jni_env->GetMethodID(mainActivityClass, "<init>", "()V");      // find constructor method
  logMsgId = jni_env->GetMethodID(mainActivityClass, "logMsg", "(Ljava/lang/String;)V");
  unityActivityClassInstance = jni_env->NewGlobalRef(jni_env->NewObject(mainActivityClass, mainActivityClassConstructorMID));                    
}
///////////////////////////////////////////////////////////////////////////////////

void UNITY_INTERFACE_API ShutdownRenderManager() {
    //DebugLog("[OSVR Rendering Plugin] Shutting down RenderManager.");

	switch (s_deviceType.getDeviceTypeEnum()) {
#if SUPPORT_D3D11
	case OSVRSupportedRenderers::D3D11:
		break;
#endif
#if SUPPORT_OPENGL
	case OSVRSupportedRenderers::OpenGL:
		// Clean up after ourselves.
		/*glDeleteFramebuffers(1, &s_frameBuffer);
		for (size_t i = 0; i < s_renderInfo.size(); i++) {
			glDeleteTextures(1, &s_renderBuffers[i].OpenGL->colorBufferName);
			delete s_renderBuffers[i].OpenGL;
			glDeleteRenderbuffers(1, &depthBuffers[i]);
		}
		*/
		//contextSet = false;
		break;
#endif
        default: break;
	}

	//s_clientContext = nullptr;

}
// --------------------------------------------------------------------------
// GraphicsDeviceEvents

#if SUPPORT_D3D11
// -------------------------------------------------------------------
///  Direct3D 11 setup/teardown code
inline void DoEventGraphicsDeviceD3D11(UnityGfxDeviceEventType eventType) {
    BOOST_ASSERT_MSG(
        s_deviceType,
        "Should only be able to get in here with a valid device type.");
    BOOST_ASSERT_MSG(
        s_deviceType.getDeviceTypeEnum() == OSVRSupportedRenderers::D3D11,
        "Should only be able to get in here if using D3D11 device type.");

    switch (eventType) {
    case kUnityGfxDeviceEventInitialize: {
        IUnityGraphicsD3D11 *d3d11 =
            s_UnityInterfaces->Get<IUnityGraphicsD3D11>();

        // Put the device and context into a structure to let RenderManager
        // know to use this one rather than creating its own.
        s_library.D3D11 = new osvr::renderkit::GraphicsLibraryD3D11;
        s_library.D3D11->device = d3d11->GetDevice();
        ID3D11DeviceContext *ctx = nullptr;
        s_library.D3D11->device->GetImmediateContext(&ctx);
        s_library.D3D11->context = ctx;
        //DebugLog("[OSVR Rendering Plugin] Passed Unity device/context to "
                 "RenderManager library.");
        break;
    }
    case kUnityGfxDeviceEventShutdown: {
        // Close the Renderer interface cleanly.
        // This should be handled in ShutdownRenderManager
        /// @todo delete library.D3D11; library.D3D11 = nullptr; ?
        break;
    }
    }
}
#endif // SUPPORT_D3D11

#if SUPPORT_OPENGL
// -------------------------------------------------------------------
/// OpenGL setup/teardown code
/// @todo OpenGL path not implemented yet
inline void DoEventGraphicsDeviceOpenGL(UnityGfxDeviceEventType eventType) {
    BOOST_ASSERT_MSG(
        s_deviceType,
        "Should only be able to get in here with a valid device type.");
    BOOST_ASSERT_MSG(
        s_deviceType.getDeviceTypeEnum() == OSVRSupportedRenderers::OpenGL,
        "Should only be able to get in here if using OpenGL device type.");
    switch (eventType) {
    case kUnityGfxDeviceEventInitialize:
      //  s_library.OpenGL = new osvr::renderkit::GraphicsLibraryOpenGL;
        //DebugLog("OpenGL Initialize Event");
        break;
    case kUnityGfxDeviceEventShutdown:
        //DebugLog("OpenGL Shutdown Event");
        break;
    default:
        break;
    }
}
#endif // SUPPORT_OPENGL

inline void dispatchEventToRenderer(UnityRendererType renderer,
                                    UnityGfxDeviceEventType eventType) {
    if (!renderer) {
        //DebugLog("[OSVR Rendering Plugin] Current device type not supported");
        return;
    }
    switch (renderer.getDeviceTypeEnum()) {
#if SUPPORT_D3D11
    case OSVRSupportedRenderers::D3D11:
        DoEventGraphicsDeviceD3D11(eventType);
        break;
#endif
#if SUPPORT_OPENGL
    case OSVRSupportedRenderers::OpenGL:
        DoEventGraphicsDeviceOpenGL(eventType);
        break;
#endif
    case OSVRSupportedRenderers::EmptyRenderer:
    default:
        break;
    }
}
#if UNITY_WIN
bool shareContext(SDL_GLContext ctx1, SDL_GLContext ctx2) {
	/*std::string str = "Sharing CONTEXT1, " + std::to_string((int)ctx1) + ", CONTEXT2, " + std::to_string((int)ctx2);
	//DebugLog(str.c_str());
	str = "myDC is, " + std::to_string((int)wglGetCurrentDC());
	//DebugLog(str.c_str());*/
	if (wglShareLists((HGLRC)ctx1, (HGLRC)ctx2)) {
		//DebugLog("[OSVR Rendering Plugin] wglShareLists success!");
		return true;
	}
	else {
		DWORD errorCode = GetLastError();
		str = "[OSVR Rendering Plugin] Context sharing failure... " + std::to_string(errorCode);
		//DebugLog(str.c_str());
		return false;
	}

}
#endif

#if UNITY_WIN
bool InitSDLGL()
{
	// Use SDL to open a window and then get an OpenGL context for us.
	// Note: This window is not the one that will be used for rendering
	// the OSVR display, but one that will be cleared to a slowly-changing
	// constant color so we can see that we're able to render to both
	// contexts.
	if (!osvr::renderkit::SDLInitQuit()) {
		//DebugLog("Could not initialize SDL");
		return false;
	}
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT,1);
	myWindow = SDL_CreateWindow(
		"Test window, not used", 30, 30, 100, 100,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
	if (myWindow == nullptr) {
		//DebugLog("SDL window open failed: Could not get window");
		return false;
	}
    myGLContext = SDL_GL_CreateContext(myWindow);

	if (myGLContext == 0) {
		//DebugLog("RenderManagerOpenGL::addOpenGLContext: Could not get OpenGL context");
		return false;
	}

	/*HDC myGLDC = wglGetCurrentDC();
	std::string str = "InitSDLGL MyGL CONTEXT is " + std::to_string((int)myGLContext);
	//DebugLog(str.c_str());
	str = "InitSDLGL Current CONTEXT is " + std::to_string((int)myGLDC);
	//DebugLog(str.c_str());
	str = "InitSDLGL Current DC is " + std::to_string((int)wglGetCurrentDC());
	//DebugLog(str.c_str());
	//wglMakeCurrent(wglGetCurrentDC(), 0);
	//str = "New Current CONTEXT is " + std::to_string((int)wglGetCurrentContext());
	////DebugLog(str.c_str());
	//shareContext();
	return true;*/
}
#endif

/// Needs the calling convention, even though it's static and not exported,
/// because it's registered as a callback on plugin load.
static void UNITY_INTERFACE_API
OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType) {
    switch (eventType) {
    case kUnityGfxDeviceEventInitialize: {
        //DebugLog(
           // "[OSVR Rendering Plugin] OnGraphicsDeviceEvent(Initialize).\n");
#if UNITY_WIN
		mainContext = wglGetCurrentContext();
		InitSDLGL();
		contextSet = shareContext(mainContext, myGLContext);
#endif
#if UNITY_OSX
        CGLContextObj cglMainContext = CGLGetCurrentContext();
        
#endif
#if UNITY_LINUX
mainActivityClass = jniEnvironment->FindClass("org/osvr/osvrunityandroid/MainActivity");  // try to find the class
    if(mainActivityClass == nullptr) {
        return;
    }
    else {                                  // if class found, continue

        jmethodID logmid = jniEnvironment->GetStaticMethodID(mainActivityClass, "logMsg", "(Ljava/lang/String;)V");  // find method
        jmethodID setGlContextId = jniEnvironment->GetStaticMethodID(mainActivityClass, "setUnityMainContext", "()J");  // find method
         if(setGlContextId == nullptr)
            //cerr << "ERROR: method void mymain() not found !" << endl;
            return;
        else {
            
            jlong currentEglContextHandle = jniEnvironment->CallStaticLongMethod(mainActivityClass, setGlContextId);                      // call method
            long myLongValue = (long) currentEglContextHandle;
            std::string stringy = "[OSVR-Unity-Android]  setCurrentContext with handle:  " + std::to_string(myLongValue);
             jstring jstr2 = jniEnvironment->NewStringUTF(stringy.c_str());
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, logmid, jstr2);   
            contextSet = true;
            //cout << endl;
        }
        //create context
      /*  jmethodID createContextId = jniEnvironment->GetStaticMethodID(mainActivityClass, "createContext", "()J");  // find method

         if(createContextId == nullptr)
            //cerr << "ERROR: method void mymain() not found !" << endl;
            return;
        else {
            
            jlong newContextHandle = jniEnvironment->CallStaticLongMethod(mainActivityClass, createContextId);                      // call method
            long myNewLongValue = (long) newContextHandle;
            std::string stringyer = "[OSVR-Unity-Android] created context with handle: " + std::to_string(myNewLongValue);
             jstring jstr3 = jniEnvironment->NewStringUTF(stringyer.c_str());
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, logmid, jstr3);   
            //cout << endl;
        }*/
    }

#endif
        s_deviceType = s_Graphics->GetRenderer();
        if (!s_deviceType) {
            //DebugLog("[OSVR Rendering Plugin] "
                    // "OnGraphicsDeviceEvent(Initialize): New device type is "
                   //  "not supported!\n");
        }
        break;
    }

    case kUnityGfxDeviceEventShutdown: {
        //DebugLog("[OSVR Rendering Plugin] OnGraphicsDeviceEvent(Shutdown).\n");
        /// Here, we want to dispatch before we reset the device type, so the
        /// right device type gets shut down. Thus we return instead of break.
        dispatchEventToRenderer(s_deviceType, eventType);
        s_deviceType.reset();
        return;
    }

    case kUnityGfxDeviceEventBeforeReset: {
        //DebugLog(
           // "[OSVR Rendering Plugin] OnGraphicsDeviceEvent(BeforeReset).\n");
        break;
    }

    case kUnityGfxDeviceEventAfterReset: {
        //DebugLog(
           // "[OSVR Rendering Plugin] OnGraphicsDeviceEvent(AfterReset).\n");
        break;
    }
    }

    dispatchEventToRenderer(s_deviceType, eventType);
}

// --------------------------------------------------------------------------
// UnitySetInterfaces
void UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces *unityInterfaces) {
#if defined(ENABLE_LOGGING) && defined(ENABLE_LOGFILE)
    s_//DebugLogFile.open("RenderPluginLog.txt");

    // Capture std::cout and std::cerr from RenderManager.
    if (s_//DebugLogFile) {
        s_oldCout = std::cout.rdbuf();
        std::cout.rdbuf(s_//DebugLogFile.rdbuf());

        s_oldCerr = std::cerr.rdbuf();
        std::cerr.rdbuf(s_//DebugLogFile.rdbuf());
    }
#endif // defined(ENABLE_LOGGING) && defined(ENABLE_LOGFILE)
    s_UnityInterfaces = unityInterfaces;
    s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
    s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);

    // Run OnGraphicsDeviceEvent(initialize) manually on plugin load
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

void UNITY_INTERFACE_API UnityPluginUnload() {
    s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
    OnGraphicsDeviceEvent(kUnityGfxDeviceEventShutdown);

#if defined(ENABLE_LOGGING) && defined(ENABLE_LOGFILE)
    if (s_//DebugLogFile) {
        // Restore the buffers
        std::cout.rdbuf(s_oldCout);
        std::cerr.rdbuf(s_oldCerr);
        s_//DebugLogFile.close();
    }
#endif // defined(ENABLE_LOGGING) && defined(ENABLE_LOGFILE)
}

inline void UpdateRenderInfo() {
    //s_renderInfo = s_render->GetRenderInfo(s_renderParams);
}


// Updates the internal "room to world" transformation (applied to all
// tracker data for this client context instance) based on the user's head
// orientation, so that the direction the user is facing becomes -Z to your
// application. Only rotates about the Y axis (yaw).
//
// Note that this method internally calls osvrClientUpdate() to get a head pose
// so your callbacks may be called during its execution!
/// @todo does this actually get called from anywhere or is it dead code?
void SetRoomRotationUsingHead() {/* s_render->SetRoomRotationUsingHead(); */}

// Clears/resets the internal "room to world" transformation back to an
// identity transformation - that is, clears the effect of any other
// manipulation of the room to world transform.
/// @todo does this actually get called from anywhere or is it dead code?
void ClearRoomToWorldTransform() { /*s_render->ClearRoomToWorldTransform();*/ }
/*
bool SetupRendering(osvr::renderkit::GraphicsLibrary library) {
	// Make sure our pointers are filled in correctly.  The config file selects
	// the graphics library to use, and may not match our needs.
	if (library.OpenGL == nullptr) {
		std::cerr << "SetupRendering: No OpenGL GraphicsLibrary, this should "
			"not happen"
			<< std::endl;
		return false;
	}

	osvr::renderkit::GraphicsLibraryOpenGL* glLibrary = library.OpenGL;

	// Turn on depth testing, so we get correct ordering.
	//glEnable(GL_DEPTH_TEST);

	return true;
}*/



/*jstring Java_the_package_MainActivity_getJniString( JNIEnv* env, jobject obj){

    jstring jstr = (*env)->NewStringUTF(env, "This comes from jni.");
    jclass clazz = (*env)->FindClass(env, "com/inceptix/android/t3d/MainActivity");
    jmethodID messageMe = (*env)->GetMethodID(env, clazz, "messageMe", "(Ljava/lang/String;)Ljava/lang/String;");
    jobject result = (*env)->CallObjectMethod(env, obj, messageMe, jstr);

    const char* str = (*env)->GetStringUTFChars(env,(jstring) result, NULL); // should be released but what a heck, it's a tutorial :)
    printf("%s\n", str);

    return (*env)->NewStringUTF(env, str);
}
JNIEXPORT void JNICALL
Java_Callbacks_nativeMethod(JNIEnv *env, jobject obj, jint depth)
{
	jclass cls = (*env)->GetObjectClass(env, obj);
	jmethodID mid = (*env)->GetMethodID(env, cls, "callback", "(I)V");
	if (mid == 0)
		return;
	printf("In C, depth = %d, about to enter Java\n", depth);
	(*env)->CallVoidMethod(env, obj, mid, depth);
	printf("In C, depth = %d, back from Java\n", depth);
}
*/


// Called from Unity to create a RenderManager, passing in a ClientContext
OSVR_ReturnCode UNITY_INTERFACE_API
CreateRenderManagerFromUnity(OSVR_ClientContext context) {
    gClientContext = context;

  //  mainActivityClass = jniEnvironment->FindClass("org/osvr/osvrunityandroid/MainActivity");  // try to find the class
  /*  if(mainActivityClass == nullptr) {
        return 5;
    }
    else {                                  // if class found, continue
       // cout << "Class MyTest found" << endl;
        jmethodID mid = jniEnvironment->GetStaticMethodID(mainActivityClass, "nativeFunction", "()V");  // find method
        if(mid == nullptr)
            //cerr << "ERROR: method void mymain() not found !" << endl;
            return 6;
        else {
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, mid);                      // call method
            //cout << endl;
        }

        jmethodID logmid = jniEnvironment->GetStaticMethodID(mainActivityClass, "logMsg", "(Ljava/lang/String;)V");  // find method
        if(logmid == nullptr)
            //cerr << "ERROR: method void mymain() not found !" << endl;
            return 7;
        else {
            if(contextSet)
            {
            std::string s("this is coming from lala land, context is set");
             jstring jstr1 = jniEnvironment->NewStringUTF(s.c_str());
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, logmid, jstr1); 
            }
            else
            {
            std::string s("this is coming from lala land, context is NOT set");
             jstring jstr1 = jniEnvironment->NewStringUTF(s.c_str());
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, logmid, jstr1); 
            }
                                 // call method
        }

        jmethodID getGlContextId = jniEnvironment->GetStaticMethodID(mainActivityClass, "getCurrentContext", "()J");  // find method
         if(getGlContextId == nullptr)
            //cerr << "ERROR: method void mymain() not found !" << endl;
            return 8;
        else {
            
            jlong currentEglContextHandle = jniEnvironment->CallStaticLongMethod(mainActivityClass, getGlContextId);                      // call method
            long myLongValue = (long) currentEglContextHandle;
            std::string stringy = "[OSVR-Unity-Android]  getCurrentContext with handle:  " + std::to_string(myLongValue);
             jstring jstr2 = jniEnvironment->NewStringUTF(stringy.c_str());
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, logmid, jstr2);   
            //cout << endl;
        }

        //create context
        jmethodID createContextId = jniEnvironment->GetStaticMethodID(mainActivityClass, "createContext", "()J");  // find method

         if(createContextId == nullptr)
            //cerr << "ERROR: method void mymain() not found !" << endl;
            return 8;
        else {
            
            jlong newContextHandle = jniEnvironment->CallStaticLongMethod(mainActivityClass, createContextId);                      // call method
            long myNewLongValue = (long) newContextHandle;
            std::string stringyer = "[OSVR-Unity-Android] created context with handle: " + std::to_string(myNewLongValue);
             jstring jstr3 = jniEnvironment->NewStringUTF(stringyer.c_str());
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, logmid, jstr3);   
            //cout << endl;
        }
    }*/
    if( setupOSVR())
    {
        if(setupGraphics(2560, 1440))
        {
            if(setupRenderManager())
            {
                return OSVR_RETURN_SUCCESS;
            }
            else return 3;
        }
        else return 2;
    }
    else return 1;
   
    return OSVR_RETURN_SUCCESS;
}

/// Helper function that handles doing the loop of constructing buffers, and
/// returning failure if any of them in the loop return failure.
template <typename F, typename G>
inline OSVR_ReturnCode applyRenderBufferConstructor(const int numBuffers,
                                                    F &&bufferConstructor,
                                                    G &&bufferCleanup) {
    /// If we bail any time before the end, we'll automatically clean up the
    /// render buffers with this lambda.
   /* auto cleanupBuffers = osvr::util::finally([&] {
        //DebugLog("[OSVR Rendering Plugin] Cleaning up render buffers.");
        for (auto &rb : s_renderBuffers) {
            bufferCleanup(rb);
        }
        s_renderBuffers.clear();
        //DebugLog("[OSVR Rendering Plugin] Render buffer cleanup complete.");
    });

    /// Construct all the buffers as isntructed
    for (int i = 0; i < numBuffers; ++i) {
        auto ret = bufferConstructor(i);
        if (ret != OSVR_RETURN_SUCCESS) {
            //DebugLog("[OSVR Rendering Plugin] Failed in a buffer constructor!");
            return OSVR_RETURN_FAILURE;
        }
    }

    /// Register our constructed buffers so that we can use them for
    /// presentation.
    if (!s_render->RegisterRenderBuffers(s_renderBuffers)) {
        //DebugLog("RegisterRenderBuffers() returned false, cannot continue");
        return OSVR_RETURN_FAILURE;
    }
    /// Only if we succeed, do we cancel the cleanup and carry on.
    cleanupBuffers.cancel();*/
    return OSVR_RETURN_SUCCESS;
}

#if SUPPORT_OPENGL

inline OSVR_ReturnCode ConstructBuffersOpenGL(int eye) {
    /*
	//DebugLog("[OSVR Rendering Plugin] ConstructBuffersOpenGL");
	//std::string str = "Construct CONTEXT is " + std::to_string((int)wglGetCurrentContext());
	////DebugLog(str.c_str());

	// Determine the appropriate size for the frame buffer to be used for
	// this eye.
	unsigned width = static_cast<unsigned>(s_renderInfo[eye].viewport.width);
	unsigned height = static_cast<unsigned>(s_renderInfo[eye].viewport.height);
	// Initialize the textures with our window's context open,
	// so that they will be associated with it.
	//SDL_GL_MakeCurrent(myWindow, myGLContext);
	//SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
	//str = "SDL CONTEXT is " + std::to_string((int)wglGetCurrentContext());
	////DebugLog(str.c_str());
	
    if (eye == 0) {
        // do this once
		glGenFramebuffers(1, &s_frameBuffer);
        
    }
	
	glBindFramebuffer(GL_FRAMEBUFFER, s_frameBuffer);

	GLuint colorBufferOpenGL = GetEyeTextureOpenGL(eye);
	//glGenTextures(1, &colorBufferOpenGL);
	osvr::renderkit::RenderBuffer rb;
	rb.OpenGL = new osvr::renderkit::RenderBufferOpenGL;
	rb.OpenGL->colorBufferName = colorBufferOpenGL;
	s_renderBuffers.push_back(rb);

	// "Bind" the newly created texture : all future texture
	// functions will modify this texture glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, colorBufferOpenGL);

	// Give an empty image to OpenGL ( the last "0" means "empty" )
	// Note that whether or not the second GL_RGBA is turned into
	// GL_BGRA, the first one should remain GL_RGBA -- it is specifying
	// the size.  If the second is changed to GL_RGB or GL_BGR, then
	// the first should become GL_RGB.
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, 0);

	// Bilinear filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// The depth buffer
	GLuint depthrenderbuffer;
	glGenRenderbuffers(1, &depthrenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width,
		height);
	depthBuffers.push_back(depthrenderbuffer);

	//DebugLog("[OSVR Rendering Plugin] pushed back");
    */

    return OSVR_RETURN_SUCCESS;
}

/*inline void CleanupBufferOpenGL(osvr::renderkit::RenderBuffer &rb) {
    /// @todo incomplete cleanup - but better than leaking in case of failure.
    delete rb.OpenGL;
    rb.OpenGL = nullptr;
}*/
#endif // SUPPORT_OPENGL

/*
#if SUPPORT_D3D11
inline ID3D11Texture2D *GetEyeTextureD3D11(int eye) {
    return reinterpret_cast<ID3D11Texture2D *>(eye == 0 ? s_leftEyeTexturePtr
                                                        : s_rightEyeTexturePtr);
}

inline OSVR_ReturnCode ConstructBuffersD3D11(int eye) {
    //DebugLog("[OSVR Rendering Plugin] ConstructBuffersD3D11");
    HRESULT hr;
    // The color buffer for this eye.  We need to put this into
    // a generic structure for the Present function, but we only need
    // to fill in the Direct3D portion.
    //  Note that this texture format must be RGBA and unsigned byte,
    // so that we can present it to Direct3D for DirectMode.
    ID3D11Texture2D *D3DTexture = GetEyeTextureD3D11(eye);
    unsigned width = static_cast<unsigned>(s_renderInfo[eye].viewport.width);
    unsigned height = static_cast<unsigned>(s_renderInfo[eye].viewport.height);

    D3DTexture->GetDesc(&s_textureDesc);

    // Fill in the resource view for your render texture buffer here
    D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
    // This must match what was created in the texture to be rendered
    /// @todo Figure this out by introspection on the texture?
    // renderTargetViewDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    /// @todo Interesting - change this line to DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    /// and not only do you not get direct mode, you get multicolored static on
    /// the display.
    renderTargetViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Texture2D.MipSlice = 0;

    // Create the render target view.
    ID3D11RenderTargetView *renderTargetView =
        nullptr; //< Pointer to our render target view
    hr = s_renderInfo[eye].library.D3D11->device->CreateRenderTargetView(
        D3DTexture, &renderTargetViewDesc, &renderTargetView);
    if (FAILED(hr)) {
        //DebugLog(
           // "[OSVR Rendering Plugin] Could not create render target for eye");
        return OSVR_RETURN_FAILURE;
    }

    // Push the filled-in RenderBuffer onto the stack.
    std::unique_ptr<osvr::renderkit::RenderBufferD3D11> rbD3D(
        new osvr::renderkit::RenderBufferD3D11);
    rbD3D->colorBuffer = D3DTexture;
    rbD3D->colorBufferView = renderTargetView;
    osvr::renderkit::RenderBuffer rb;
    rb.D3D11 = rbD3D.get();
    s_renderBuffers.push_back(rb);

    // OK, we succeeded, must release ownership of that pointer now that it's in
    // RenderManager's hands.
    rbD3D.release();
    return OSVR_RETURN_SUCCESS;
}

inline void CleanupBufferD3D11(osvr::renderkit::RenderBuffer &rb) {
    delete rb.D3D11;
    rb.D3D11 = nullptr;
}
#endif // SUPPORT_D3D11
*/

OSVR_ReturnCode UNITY_INTERFACE_API ConstructRenderBuffers() {
  
        if(!setupRenderTextures(gRenderManager)) {
            return 10;
        }
        else return OSVR_RETURN_SUCCESS;
}

void UNITY_INTERFACE_API SetNearClipDistance(double distance) {
    s_nearClipDistance = distance;
    //s_renderParams.nearClipDistanceMeters = s_nearClipDistance;
}

void UNITY_INTERFACE_API SetFarClipDistance(double distance) {
    s_farClipDistance = distance;
    //s_renderParams.farClipDistanceMeters = s_farClipDistance;
}

void UNITY_INTERFACE_API SetIPD(double ipdMeters) {
    s_ipd = ipdMeters;
   // s_renderParams.IPDMeters = s_ipd;
}


osvr::renderkit::OSVR_ViewportDescription UNITY_INTERFACE_API
GetViewport(int eye) {
     OSVR_RenderParams renderParams;
    OSVR_ReturnCode rc = osvrRenderManagerGetDefaultRenderParams(&renderParams);
    checkReturnCode(rc, "osvrRenderManagerGetDefaultRenderParams call failed.");
    RenderInfoCollectionOpenGL renderInfoCollection(gRenderManager, renderParams);
    OSVR_RenderInfoOpenGL currentRenderInfo = renderInfoCollection.getRenderInfo(eye);
    osvr::renderkit::OSVR_ViewportDescription viewDesc;
    viewDesc.width = currentRenderInfo.viewport.width;
    viewDesc.height = currentRenderInfo.viewport.height;
    viewDesc.left = currentRenderInfo.viewport.left;
    viewDesc.lower = currentRenderInfo.viewport.lower;
    return viewDesc;
}

osvr::renderkit::OSVR_ProjectionMatrix UNITY_INTERFACE_API
GetProjectionMatrix(int eye) {
     OSVR_RenderParams renderParams;
           OSVR_ReturnCode rc = osvrRenderManagerGetDefaultRenderParams(&renderParams);
            checkReturnCode(rc, "osvrRenderManagerGetDefaultRenderParams call failed.");
    RenderInfoCollectionOpenGL renderInfoCollection(gRenderManager, renderParams);
    OSVR_RenderInfoOpenGL currentRenderInfo = renderInfoCollection.getRenderInfo(eye);
    osvr::renderkit::OSVR_ProjectionMatrix proj;
    proj.left = currentRenderInfo.projection.left;
    proj.right = currentRenderInfo.projection.right;
    proj.top = currentRenderInfo.projection.top;
    proj.bottom = currentRenderInfo.projection.bottom;
    proj.nearClip = currentRenderInfo.projection.nearClip;
    proj.farClip = currentRenderInfo.projection.farClip;
    return proj;
}

OSVR_Pose3 UNITY_INTERFACE_API GetEyePose(int eye) {
     OSVR_RenderParams renderParams;
           OSVR_ReturnCode rc = osvrRenderManagerGetDefaultRenderParams(&renderParams);
            checkReturnCode(rc, "osvrRenderManagerGetDefaultRenderParams call failed.");
    RenderInfoCollectionOpenGL renderInfoCollection(gRenderManager, renderParams);
    OSVR_RenderInfoOpenGL currentRenderInfo = renderInfoCollection.getRenderInfo(eye);
    return currentRenderInfo.pose;
}



// --------------------------------------------------------------------------
// Should pass in eyeRenderTexture.GetNativeTexturePtr(), which gets updated in
// Unity when the camera renders.
// On Direct3D-like devices, GetNativeTexturePtr() returns a pointer to the base
// texture type (IDirect3DBaseTexture9 on D3D9, ID3D11Resource on D3D11). On
// OpenGL-like devices the texture "name" is returned; cast the pointer to
// integer type to get it. On platforms that do not support native code plugins,
// this function always returns NULL.
// Note that calling this function when using multi-threaded rendering will
// synchronize with the rendering thread (a slow operation), so best practice is
// to set up needed texture pointers only at initialization time.
// For more reference, see:
// http://docs.unity3d.com/ScriptReference/Texture.GetNativeTexturePtr.html
int UNITY_INTERFACE_API SetColorBufferFromUnity(void *texturePtr, int eye) {
    if (!s_deviceType) {
        return OSVR_RETURN_FAILURE;
    }

    //DebugLog("[OSVR Rendering Plugin] SetColorBufferFromUnity");
    if (eye == 0) {
        gLeftEyeTextureID = (GLuint)texturePtr;
    } else {
        gRightEyeTextureID = (GLuint)texturePtr;
    }
	/*std::string str = "Set CONTEXT is " + std::to_string((int)wglGetCurrentContext());
	//DebugLog(str.c_str());
	str = "Set Buffername is " + std::to_string((GLuint)texturePtr);
	//DebugLog(str.c_str());*/


    return OSVR_RETURN_SUCCESS;
}
#if SUPPORT_D3D11
// Renders the view from our Unity cameras by copying data at
// Unity.RenderTexture.GetNativeTexturePtr() to RenderManager colorBuffers
void RenderViewD3D11(const osvr::renderkit::RenderInfo &ri,
	ID3D11RenderTargetView *renderTargetView, int eyeIndex) {
	auto context = ri.library.D3D11->context;
	// Set up to render to the textures for this eye
	context->OMSetRenderTargets(1, &renderTargetView, NULL);

	// copy the updated RenderTexture from Unity to RenderManager colorBuffer
	s_renderBuffers[eyeIndex].D3D11->colorBuffer = GetEyeTextureD3D11(eyeIndex);
}
#endif // SUPPORT_D3D11


inline void DoRender() {
    if (!s_deviceType) {
        return;
    }
    //const auto n = static_cast<int>(s_renderInfo.size());

    switch (s_deviceType.getDeviceTypeEnum()) {

#if SUPPORT_OPENGL
    case OSVRSupportedRenderers::OpenGL: {
        renderFrame();
        // OpenGL
        //@todo OpenGL path is not working yet
        // Render into each buffer using the specified information.
		/*std::string str = "Render CONTEXT is " + std::to_string((int)wglGetCurrentContext());
		//DebugLog(str.c_str());
		//wglMakeCurrent(wglGetCurrentDC(), 0);
		//str = "new CONTEXT is " + std::to_string((int)wglGetCurrentContext());
		////DebugLog(str.c_str());
       /for (int i = 0; i < n; ++i) {
			//DebugLog(str.c_str());
            RenderViewOpenGL(s_renderInfo[i], s_frameBuffer,
				s_renderBuffers[i].OpenGL->colorBufferName, depthBuffers[i], i);
			str = "Buffername is " + std::to_string(s_renderBuffers[i].OpenGL->colorBufferName);
			//DebugLog(str.c_str());
        }*/
#if UNITY_WIN
		HGLRC glCont = wglGetCurrentContext();
		HDC glDc = wglGetCurrentDC();
        // Send the rendered results to the screen
       if (!s_render->PresentRenderBuffers(s_renderBuffers, s_renderInfo)) {
            //DebugLog("PresentRenderBuffers() returned false, maybe because "
                     //"it was asked to quit");
        }
	   // Draw something in our window, just looping the background color
	   // Render to the standard framebuffer in our own window
	   // Because we bind a different frame buffer in our draw routine, we
	   // need to put this back here.
	   SDL_GL_MakeCurrent(myWindow, myGLContext);
	   wglMakeCurrent(glDc, glCont);
#endif
#if UNITY_OSX
        CGLContextObj cglCont = CGLGetCurrentContext();
        if (!s_render->PresentRenderBuffers(s_renderBuffers, s_renderInfo)) {
            //DebugLog("PresentRenderBuffers() returned false, maybe because "
                    // "it was asked to quit");
        }
        SDL_GL_MakeCurrent(myWindow, myGLContext);
        CGLSetCurrentContext(cglCont);


#endif
/*#if UNITY_LINUX
        if(mainActivityClass == nullptr) {
        return;
    }
    else {                                  // if class found, continue
    if (!s_render->PresentRenderBuffers(s_renderBuffers, s_renderInfo)) {
                //DebugLog("PresentRenderBuffers() returned false, maybe because "
                        // "it was asked to quit");
            }
        jmethodID logmid = jniEnvironment->GetStaticMethodID(mainActivityClass, "logMsg", "(Ljava/lang/String;)V");  // find method
        jmethodID makeToolkitContextCurrentMID = jniEnvironment->GetStaticMethodID(mainActivityClass, "makeToolkitContextCurrent", "()V");  // find method
        jmethodID makeUnityMainContextCurrentMID = jniEnvironment->GetStaticMethodID(mainActivityClass, "makeUnityMainContextCurrent", "()V");  // find method

         if(makeToolkitContextCurrentMID == nullptr || makeUnityMainContextCurrentMID == nullptr || logmid == nullptr)
            //cerr << "ERROR: method void mymain() not found !" << endl;
            return;
        else {
            
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, makeToolkitContextCurrentMID);                      // call method
            jniEnvironment->CallStaticVoidMethod(mainActivityClass, makeUnityMainContextCurrentMID);   
        }
    }

        #endif*/
	  // glBindFramebuffer(GL_FRAMEBUFFER, 0);
	  // static GLfloat bg = 0;
	  /* glViewport(static_cast<GLint>(0),
		   static_cast<GLint>(0),
		   static_cast<GLint>(0),
		   static_cast<GLint>(0));*/
	 //  glClearColor(bg, bg, bg, 1.0f);
	 //  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	 //  SDL_GL_SwapWindow(myWindow);
	  // bg += 0.003f;
	  // if (bg > 1) { bg = 0; }

        break;
    }
#endif // SUPPORT_OPENGL

    case OSVRSupportedRenderers::EmptyRenderer:
    default:
        break;
    }
}

// --------------------------------------------------------------------------
// UnityRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent.
/// @todo does this actually need to be exported? It seems like
/// GetRenderEventFunc returning it would be sufficient...
void UNITY_INTERFACE_API OnRenderEvent(int eventID) {
    // Unknown graphics device type? Do nothing.
    if (!s_deviceType) {
        return;
    }
	std::string str;
    switch (eventID) {
    // Call the Render loop
    case kOsvrEventID_Render:
        DoRender();
        break;
    case kOsvrEventID_Shutdown:
        break;
    case kOsvrEventID_Update:
        UpdateRenderInfo();
        break;
    case kOsvrEventID_SetRoomRotationUsingHead:
        SetRoomRotationUsingHead();
        break;
    case kOsvrEventID_ClearRoomToWorldTransform:
        ClearRoomToWorldTransform();
        break;
    default:
        break;
    }
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, a function we export which is used to get a
// rendering event callback function.
UnityRenderingEvent UNITY_INTERFACE_API GetRenderEventFunc() {
    return &OnRenderEvent;
}

#include "OpenGLLegacyInterface.h"

#include "Engine.h"
#include "ConVar/ConVar.h"
#include "Camera/Camera.h"
#include "Font/Font.h"
#include "OpenGL/OpenGLImage.h"
#include "OpenGL/OpenGLRenderTarget.h"
#include "OpenGL/OpenGLShader.h"
#include "OpenGL/OpenGLVertexArrayObject.h"

#include "Platform/OpenGLHeaders.h"

#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#define GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#define GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B

#define VBO_FREE_MEMORY_ATI                     0x87FB
#define TEXTURE_FREE_MEMORY_ATI                 0x87FC
#define RENDERBUFFER_FREE_MEMORY_ATI            0x87FD

ConVar r_image_unbind_after_drawimage("r_image_unbind_after_drawimage", true);

OpenGLLegacyInterface::OpenGLLegacyInterface() : Graphics() {
	m_bInScene = false;
	m_vResolution = engine->getScreenSize();

	m_bAntiAliasing = true;
	m_color = 0xffffffff;
	m_fClearZ = 1;
	m_fZ = 1;
}

void OpenGLLegacyInterface::init() {
	const GLubyte* version = glGetString(GL_VERSION);
	debugLog("OpenGL: OpenGL Version %s\n", version);

	GLenum err = glewInit();
	if (GLEW_OK != err) {
		debugLog("glewInit() Error: %s\n", glewGetErrorString(err));
		engine->showMessageErrorFatal("OpenGL Error", "Couldn't glewInit()!\nThe engine will exit now.");
		engine->shutdown();
		return;
	}

	if (!glewIsSupported("GL_VERSION_3_0") && !glewIsSupported("GLEW_VERSION_3_0"))
		engine->showMessageWarning("OpenGL Warning", UString::format("Your GPU does not support OpenGL version 3.0!\nThe engine will try to continue, but probably crash.\nOpenGL version = %s", version));


	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glEnable(GL_COLOR_MATERIAL);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LIGHTING);

	glShadeModel(GL_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendEquation(GL_FUNC_ADD);

	glFrontFace(GL_CCW);
}

OpenGLLegacyInterface::~OpenGLLegacyInterface() {}

void OpenGLLegacyInterface::beginScene() {
	m_bInScene = true;

	Matrix4 defaultProjectionMatrix = Camera::buildMatrixOrtho2D(0, m_vResolution.x, m_vResolution.y, 0, -1.0f, 1.0f);
	pushTransform();
	setProjectionMatrix(defaultProjectionMatrix);
	translate(r_globaloffset_x->getFloat(), r_globaloffset_y->getFloat());
	updateTransform();

	glClearColor(0, 0, 0, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	handleGLErrors();
}

void OpenGLLegacyInterface::endScene() {
	popTransform();

	checkStackLeaks();

	if (m_clipRectStack.size() > 0) {
		engine->showMessageErrorFatal("ClipRect Stack Leak", "Make sure all push*() have a pop*()!");
		engine->shutdown();
	}

	m_bInScene = false;
}

void OpenGLLegacyInterface::clearDepthBuffer() {
	glClear(GL_DEPTH_BUFFER_BIT);
}
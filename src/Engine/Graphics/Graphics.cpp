#include "Graphics.h"

#include "Engine.h"
#include "ConVar/ConVar.h"
#include "Camera/Camera.h"

ConVar r_3dscene_zn("r_3dscene_zn", 5.0f);
ConVar r_3dscene_zf("r_3dscene_zf", 5000.0f);

ConVar _r_globaloffset_x("r_globaloffset_x", 0.0f);
ConVar _r_globaloffset_y("r_globaloffset_y", 0.0f);
ConVar _r_debug_disable_cliprect("r_debug_disable_cliprect", false);
ConVar _r_debug_disable_3dscene("r_debug_disable_3dscene", false);
ConVar _r_debug_flush_drawstring("r_debug_flush_drawstring", false);
ConVar _r_debug_drawimage("r_debug_drawimage", false);

ConVar* Graphics::r_globaloffset_x = &_r_globaloffset_x;
ConVar* Graphics::r_globaloffset_y = &_r_globaloffset_y;
ConVar* Graphics::r_debug_disable_cliprect = &_r_debug_disable_cliprect;
ConVar* Graphics::r_debug_disable_3dscene = &_r_debug_disable_3dscene;
ConVar* Graphics::r_debug_flush_drawstring = &_r_debug_flush_drawstring;
ConVar* Graphics::r_debug_drawimage = &_r_debug_drawimage;

Graphics::Graphics() {
	m_bTransformUpToDate = false;
	m_worldTransformStack.push(Matrix4());

	m_bIs3dScene = false;
	m_3dSceneStack.push(false);
}

void Graphics::pushTransform() {
	m_worldTransformStack.push(Matrix4(m_worldTransformStack.top()));
	m_projectionTransformStack.push(Matrix4(m_projectionTransformStack.top()));
}

void Graphics::popTransform() {
	if (m_worldTransformStack.size() < 2) {
		engine->showMessageErrorFatal("World Transform Stack Underflow", "Too many pop*()s!");
	}
	if (m_projectionTransformStack.size() < 2) {
		engine->showMessageErrorFatal("Projection Transform Stack Underflow", "Too many pop*()s!");
		engine->shutdown();
		return;
	}

	m_worldTransformStack.pop();
	m_projectionTransformStack.pop();
	m_bTransformUpToDate = false;
}

void Graphics::translate(float x, float y, float z) {
	m_worldTransformStack.top().translate(x, y, z);
	m_bTransformUpToDate = false;
}

void Graphics::rotate(float deg, float x, float y, float z) {
	m_worldTransformStack.top().rotate(deg, x, y, z);
	m_bTransformUpToDate = false;
}

void Graphics::scale(float x, float y, float z) {
	m_worldTransformStack.top().scale(x, y, z);
	m_bTransformUpToDate = false;
}

void Graphics::translate3D(float x, float y, float z) {
	Matrix4 translation;

}
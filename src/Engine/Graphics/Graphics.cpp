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
	translation.translate(x, y, z);
	setWorldMatrixMul(translation);
}

void Graphics::rotate3D(float deg, float x, float y, float z) {
	Matrix4 rotation;
	rotation.rotate(deg, x, y, z);
	setWorldMatrixMul(rotation);
}

void Graphics::setWorldMatrix(Matrix4& worldMatrix) {
	m_worldTransformStack.pop();
	m_worldTransformStack.push(worldMatrix);
	m_bTransformUpToDate = false;
}

void Graphics::setWorldMatrixMul(Matrix4& worldMatrix) {
	m_worldTransformStack.top() *= worldMatrix;
	m_bTransformUpToDate = false;
}

void Graphics::setProjectionMatrix(Matrix4& projectionMatrix) {
	m_projectionTransformStack.pop();
	m_projectionTransformStack.push(projectionMatrix);
	m_bTransformUpToDate = false;
}

Matrix4 Graphics::getWorldMatrix() {
	return m_worldTransformStack.top();
}

Matrix4 Graphics::getProjectionMatrix() {
	return m_projectionTransformStack.top();
}

void Graphics::push3DScene(Rects region) {
	if (r_debug_disable_3dscene->getBool()) return;

	if (m_3dSceneStack.top()) {
		m_3dSceneStack.push(false);
		return;
	}
	m_v3dSceneOffset.x = m_v3dSceneOffset.y = m_v3dSceneOffset.z = 0;
	float m_fFov = 60.0f;

	m_bIs3dScene = true;
	m_3dSceneStack.push(true);
	m_3dSceneRegion = region;

	pushTransform();

	float angle = (180.0f - m_fFov) / 2.0f;
	float b = (engine->getScreenHeight() / std::sin(deg2rad(m_fFov))) * std::sin(deg2rad(angle));
	float hc = std::sqrt(std::pow(b, 2.0f) - std::pow((engine->getScreenHeight() / 2.0f), 2.0f));

	Matrix4 trans2 = Matrix4().translate(-1 + (region.getWidth()) / (float)engine->getScreenWidth() + (region.getX() * 2) / (float)engine->getScreenWidth(), 1 - region.getHeight() / (float)engine->getScreenHeight() - (region.getY() * 2) / (float)engine->getScreenHeight(), 0);
	Matrix4 projectionMatrix = trans2 * Camera::buildMatrixPerspectiveFov(deg2rad(m_fFov), ((float)engine->getScreenWidth()) / ((float)engine->getScreenHeight()), r_3dscene_zn.getFloat(), r_3dscene_zf.getFloat());
	m_3dSceneProjectionMatrix = projectionMatrix;

	Matrix4 trans = Matrix4().translate(-(float)region.getWidth() / 2 - region.getX(), -(float)region.getHeight() / 2 - region.getY(), 0);
	m_3dSceneWorldMatrix = Camera::buildMatrixLookAt(Vector3(0, 0, -hc), Vector3(0, 0, 0), Vector3(0, -1, 0)) * trans;

	updateTransform(true);
}

void Graphics::pop3DScene() {
	if (!m_3dSceneStack.top()) return;

	m_3dSceneStack.pop();
	popTransform();
	m_bIs3dScene = false;
}

void Graphics::translate3DScene(float x, float y, float z) {
	if (!m_3dSceneStack.top()) return;

	m_3dSceneWorldMatrix.translate(x, y, z);
	updateTransform(true);
}

void Graphics::rotate3DScene(float rotx, float roty, float rotz) {
	if (!m_3dSceneStack.top()) return;

	Matrix4 rot;
	Vector3 centerVec = Vector3(m_3dSceneRegion.getX() + m_3dSceneRegion.getWidth() / 2 + m_v3dSceneOffset.x, m_3dSceneRegion.getY() + m_3dSceneRegion.getHeight() / 2 + m_v3dSceneOffset.y, m_v3dSceneOffset.z);
	rot.translate(-centerVec);

	if (rotx != 0) rot.rotateX(-rotx);
	if (roty != 0) rot.rotateY(-roty);
	if (rotz != 0) rot.rotateZ(-rotz);

	rot.translate(centerVec);

	m_3dSceneWorldMatrix = m_3dSceneWorldMatrix * rot;
	updateTransform(true);
}

void Graphics::offset3DScene(float x, float y, float z) {
	m_v3dSceneOffset = Vector3(x, y, z);
}

void Graphics::updateTransform(bool force) {
	if (!m_bTransformUpToDate || force) {
		Matrix4 worldMatrixT = m_worldTransformStack.top();
		Matrix4 projectionMatrixT = m_projectionTransformStack.top();

		if (m_bIs3dScene) {
			worldMatrixT = m_3dSceneWorldMatrix * m_worldTransformStack.top();
			projectionMatrixT = m_3dSceneProjectionMatrix;
		}
		onTransformUpdate(projectionMatrixT, worldMatrixT);
		m_bTransformUpToDate = true;
	}
}

void Graphics::checkStackLeaks() {
	if (m_worldTransformStack.size() > 1) {
		engine->showMessageErrorFatal("World stack leak", "Push*() and Pop*() may not be equal");
		engine->shutdown();
	}
	if (m_projectionTransformStack.size() > 1) {
		engine->showMessageErrorFatal("Projection stack leak", "Push*() and Pop*() may not be equal");
		engine->shutdown();
	}
	if (m_3dSceneStack.size() > 1) {
		engine->showMessageErrorFatal("3D Scene stack leak", "Push*() and Pop*() may not be equal");
		engine->shutdown();
	}
}

void _vsync(UString oldValue, UString newValue) {
	if (newValue.length() < 1)
		debugLog("vsync 1 = 0n, vsync 0 = off\n");
	else {
		bool vsync = newValue.toFloat() > 0.0f;
		engine->getGraphics()->setVSync(vsync);
	}
}

void _mat_wireframe(UString oldValue, UString newValue) {
	engine->getGraphics()->setWireframe(newValue.toFloat() > 0.0f);
}

ConVar _mat_wireframe_("mat_wireframe", false, _mat_wireframe);
ConVar _vsync_("vsync", false, _vsync);
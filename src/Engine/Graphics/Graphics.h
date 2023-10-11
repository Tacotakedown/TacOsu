#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <vector>
#include <stack>

#include "Matrices/Matrices.h"
#include "Vectors/Vectors.h"
#include "Rect/Rect.h"

class ConVar;
class UString;

class Image;
class TacoFont;
class Shader;
class RenderTarget;
class VertexArrayObject;

typedef uint32_t Color;

class Graphics {
public:
	enum class PRIMITIVE {
		PRIMITIVE_LINES,
		PRIMITIVE_LINE_STRIP,
		PRIMITIVE_TRIANGLES,
		PRIMITIVE_TRIANGLE_FAN,
		PRIMITIVE_TRIANGLE_STRIP,
		PRIMITIVE_QUADS
	};

	enum class USAGE_TYPE {
		USAGE_STATIC,
		USAGE_DYNAMIC,
		USAGE_STREAM
	};

	enum class DRAWPIXELS_TYPE {
		DRAWPIXELS_UBYTE,
		DRAWPIXELS_FLOAT
	};

	enum class MULTISAMPLE_TYPE {
		MULTISAMPLE_0X,
		MULTISAMPLE_2X,
		MULTISAMPLE_4X,
		MULTISAMPLE_8X,
		MULTISAMPLE_16X
	};

	enum class WRAP_MODE {
		WRAP_MODE_CLAMP,
		WRAP_MODE_REPEAT
	};

	enum class FILTER_MODE {
		FILTER_MODE_NONE,
		FILTER_MODE_LINEAR,
		FILTER_MODE_MIPMAP
	};

	enum class BLEND_MODE {
		BLEND_MODE_ALPHA,			// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) (default)
		BLEND_MODE_ADDITIVE,		// glBlendFunc(GL_SRC_ALPHA, GL_ONE)
		BLEND_MODE_PREMUL_ALPHA,	// glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
		BLEND_MODE_PREMUL_COLOR,	// glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)
	};

public:
	Graphics();
	virtual ~Graphics() {}

	virtual void beginScene() = 0;
	virtual void endScene() = 0;

	virtual void clearDepthBuffer() = 0;

	virtual void setColor(Color color) = 0;
	virtual void setAlpha(float alpha) = 0;

	virtual void drawPixels(int x, int y, int width, int height, Graphics::DRAWPIXELS_TYPE type, const void* pixels) = 0;
	virtual void drawPixel(int x, int y) = 0;
	virtual void drawLine(int x1, int y1, int x2, int y2) = 0;
	virtual void drawLine(Vector2 pos1, Vector2 pos2) = 0;
	virtual void drawRect(int x, int y, int width, int height) = 0;
	virtual void drawRect(int x, int y, int width, int height, Color top, Color right, Color bottom, Color left) = 0;

	virtual void fillRect(int x, int y, int width, int height) = 0;
	virtual void fillRoundedRect(int x, int y, int width, int height, int radius) = 0;
	virtual void fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor, Color bottomLeftColor, Color bottomRightColor) = 0;

	virtual void drawQuad(int x, int y, int width, int height) = 0;
	virtual void drawQuad(Vector2 topLeft, Vector2 topRight, Vector2 bottomRight, Vector2 bottomLeft, Color topLeftColor, Color topRightColor, Color bottomRightColor, Color bottomLeftColor) = 0;

	virtual void drawImage(Image* image) = 0;
	virtual void drawString(TacoFont* font, UString text) = 0;

	virtual void drawVAO(VertexArrayObject* vao) = 0;

	virtual void setClipRect(Rects clipRect) = 0;
	virtual void pushClipRect(Rects clipRect) = 0;
	virtual void popClipRect() = 0;

	virtual void pushStencil() = 0;
	virtual void fillStencil(bool inside) = 0;
	virtual void popStencil() = 0;

	virtual void setClipping(bool enabled) = 0;
	virtual void setBlending(bool enabled) = 0;
	virtual void setBlendMode(BLEND_MODE blendMode) = 0;
	virtual void setDepthBuffer(bool enabled) = 0;
	virtual void setCulling(bool enabled) = 0;
	virtual void setVSync(bool enabled) = 0;
	virtual void setAntialiasing(bool enabled) = 0;
	virtual void setWireframe(bool enabled) = 0;

	virtual void flush() = 0;
	virtual std::vector<unsigned char> getScreenshot() = 0;

	virtual Vector2 getResolution() const = 0;
	virtual UString getVendor() = 0;
	virtual UString getModel() = 0;
	virtual UString getVersion() = 0;
	virtual int getVRAMTotal() = 0;
	virtual int getVRAMRemaining() = 0;

	virtual void onResolutionChange(Vector2 newResolution) = 0;

	virtual Image* createImage(UString filePath, bool mipmapped, bool keepInSystemMemory) = 0;
	virtual Image* createImage(int width, int height, bool mipmapped, bool keepInSystemMemory) = 0;
	virtual RenderTarget* createRenderTarget(int x, int y, int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType) = 0;
	virtual Shader* createShaderFromFile(UString vertexShaderFilePath, UString fragmentShaderFilePath) = 0;
	virtual Shader* createShaderFromSource(UString vertexShader, UString fragmentShader) = 0;
	virtual VertexArrayObject* createVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage, bool keepInSystemMemory) = 0;


public:
	void pushTransform();
	void popTransform();

	void translate(float x, float y, float z = 0);
	void translate(Vector2 translation) { translate(translation.x, translation.y); }
	void translate(Vector3 translation) { translate(translation.x, translation.y, translation.z); }
	void rotate(float deg, float x = 0, float y = 0, float z = 1);
	void rotate(float deg, Vector3 axis) { rotate(deg, axis.x, axis.y, axis.z); }
	void scale(float x, float y, float z = 1);
	void scale(Vector2 scaling) { scale(scaling.x, scaling.y, 1); }
	void scale(Vector3 scaling) { scale(scaling.x, scaling.y, scaling.z); }

	void translate3D(float x, float y, float z);
	void translate3D(Vector3 translation) { translate3D(translation.x, translation.y, translation.z); }
	void rotate3D(float deg, float x, float y, float z);
	void rotate3D(float deg, Vector3 axis) { rotate3D(deg, axis.x, axis.y, axis.z); }
	void setWorldMatrix(Matrix4& worldMatrix);
	void setWorldMatrixMul(Matrix4& worldMatrix);
	void setProjectionMatrix(Matrix4& projectionMatrix);

	Matrix4 getWorldMatrix();
	Matrix4 getProjectionMatrix();

	void push3DScene(Rects region);
	void pop3DScene();
	void translate3DScene(float x, float y, float z = 0);
	void rotate3DScene(float rotx, float roty, float rotz);
	void offset3DScene(float x, float y, float z = 0);

protected:
	static ConVar* r_globaloffset_x;
	static ConVar* r_globaloffset_y;
	static ConVar* r_debug_disable_cliprect;
	static ConVar* r_debug_disable_3dscene;
	static ConVar* r_debug_flush_drawstring;
	static ConVar* r_debug_drawimage;

protected:
	virtual void init() = 0;
	virtual void onTransformUpdate(Matrix4& projectionMatrix, Matrix4& worldMatrix) = 0;

	void updateTransfor(bool force = false);

	void checkStackLeaks();

	friend class Engine;

	bool				m_bTransformUpToDate;
	std::stack<Matrix4> m_worldTransformStack;
	std::stack<Matrix4> m_projectionTransformStack;

	bool				m_bIs3dScene;
	std::stack<bool>	m_3dSceneStack;
	Rects				m_3dSceneRegion;
	Vector3				m_v3dSceneOffset;
	Matrix4				m_3dSceneWorldMatrix;
	Matrix4				m_3dSceneProjectionMatrix;

};

#endif // !GRAPHICS_H

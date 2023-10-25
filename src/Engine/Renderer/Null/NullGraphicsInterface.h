#ifndef NULLGRAPHICSINTERFACE_H
#define NULLGRAPHICSINTERFACE_H

#include "Graphics/Graphics.h"

class NullGraphicsInterface : public Graphics {
public:
	NullGraphicsInterface() : Graphics() { ; }
	virtual ~NullGraphicsInterface() { ; }

	virtual void beginScene() { ; }
	virtual void endScene() { ; }

	virtual void clearDepthBuffer() { ; }

	virtual void setColor(Color color) { ; }
	virtual void setAlpha(float alpha) { ; }

	virtual void drawPixels(int x, int y, int width, int height, Graphics::DRAWPIXELS_TYPE type, const void* pixels) { ; }
	virtual void drawPixel(int x, int y) { ; }
	virtual void drawLine(int x1, int y1, int x2, int y2) { ; }
	virtual void drawLine(Vector2 pos1, Vector2 pos2) { ; }
	virtual void drawRect(int x, int y, int width, int height) { ; }
	virtual void drawRect(int x, int y, int width, int height, Color top, Color right, Color bottom, Color left) { ; }

	virtual void fillRect(int x, int y, int width, int height) { ; }
	virtual void fillRoundedRect(int x, int y, int width, int height, int radius) { ; }
	virtual void fillGradient(int x, int y, int width, int height, Color topLeftColor, Color topRightColor, Color bottomLeftColor, Color bottomRightColor) { ; }

	virtual void drawQuad(int x, int y, int width, int height) { ; }
	virtual void drawQuad(Vector2 topLeft, Vector2 topRight, Vector2 bottomRight, Vector2 bottomLeft, Color topLeftColor, Color topRightColor, Color bottomRightColor, Color bottomLeftColor) { ; }

	virtual void drawImage(Image* image) { ; }
	virtual void drawString(TacoFont* font, UString text);

	virtual void drawVAO(VertexArrayObject* vao) { ; }

	virtual void setClipRect(Rects clipRect) { ; }
	virtual void pushClipRect(Rects clipRect) { ; }
	virtual void popClipRect() { ; }

	virtual void pushStencil() { ; }
	virtual void fillStencil(bool inside) { ; }
	virtual void popStencil() { ; }

	virtual void setClipping(bool enabled) { ; }
	virtual void setBlending(bool enabled) { ; }
	virtual void setBlendMode(BLEND_MODE blendMode) { ; }
	virtual void setDepthBuffer(bool enabled) { ; }
	virtual void setCulling(bool culling) { ; }
	virtual void setVSync(bool vsync) { ; }
	virtual void setAntialiasing(bool aa) { ; }
	virtual void setWireframe(bool enabled) { ; }

	virtual void flush() { ; }
	virtual std::vector<unsigned char> getScreenshot() { return std::vector<unsigned char>(); }

	virtual Vector2 getResolution() const { return m_vResolution; }
	virtual UString getVendor();
	virtual UString getModel();
	virtual UString getVersion();
	virtual int getVRAMTotal() { return -1; }
	virtual int getVRAMRemaining() { return -1; }

	virtual void onResolutionChange(Vector2 newResolution) { m_vResolution = newResolution; }

	virtual Image* createImage(UString filePath, bool mipmapped, bool keepInSystemMemory);
	virtual Image* createImage(int width, int height, bool mipmapped, bool keepInSystemMemory);
	virtual RenderTarget* createRenderTarget(int x, int y, int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType);
	virtual Shader* createShaderFromFile(UString vertexShaderFilePath, UString fragmentShaderFilePath);
	virtual Shader* createShaderFromSource(UString vertexShader, UString fragmentShader);
	virtual VertexArrayObject* createVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage, bool keepInSystemMemory);

protected:
	virtual void init() { ; }
	virtual void onTransformUpdate(Matrix4& projectionMatrix, Matrix4& worldMatrix) { ; }

private:
	Vector2 m_vResolution;
};

#endif // !NULLGRAPHICSINTERFACE_H

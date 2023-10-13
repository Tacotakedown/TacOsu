#ifndef IMAGE_H
#define IMAGE_H

#include "Resource/Resource.h"

class Image : public Resource {
public:
	static void saveToImage(unsigned char* data, unsigned int width, unsigned int height, UString filePath);

	enum class TYPE {
		TYPE_RGBA,
		TYPE_PNG,
		TYPE_JPG
	};

public:
	Image(UString filePath, bool mipmapped = false, bool keepInSystemMemory = false);
	Image(int width, int height, bool mipmapped = false, bool keepInSystemMemory = false);
	virtual ~Image() { ; }

	virtual void bind(unsigned int textureUnit = 0) = 0;
	virtual void unbind() = 0;

	virtual void setFilterMode(Graphics::FILTER_MODE filterMode) = 0;
	virtual void setWrapMode(Graphics::WRAP_MODE wrapMode) = 0;

	void setPixel(int x, int y, Color color);
	void setPixels(const char* data, size_t size, TYPE type);
	void setPixels(const std::vector<unsigned char>& pixels);

	Color getPixel(int x, int y) const;

	inline Image::TYPE getType() const { return m_type; }
	inline int getNumChannels() const { return m_iNumChannels; }
	inline int getWidth() const { return m_iWidth; }
	inline int getHeight() const { return m_iHeight; }
	inline Vector2 getSize() const { return Vector2(m_iWidth, m_iHeight); }

	inline bool hasAplhaChannel() const { return m_bHasAlphaChanel; }

protected:
	virtual void init() = 0;
	virtual void initAsync() = 0;
	virtual void destroy() = 0;

	bool loadRawImage();

	Image::TYPE					m_type;

	int							m_iNumChannels;
	int							m_iWidth;
	int							m_iHeight;

	bool						m_bHasAlphaChanel;
	bool						m_bMipmapped;
	bool						m_bCreatedImage;
	bool						m_bKeepInSystemMemory;

	std::vector<unsigned char>	m_rawImage;
};

#endif // !IMAGE_H

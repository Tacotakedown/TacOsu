#include "Font.h"


#include "ResourceManager/ResourceManager.h"
#include "VertexArrayObject/VertexArrayObject.h"
#include "Engine.h"
#include "ConVar/ConVar.h"

#include <ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftglyph.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>

ConVar r_drawstring_max_string_length("r_drawstring_max_string_length", 65536, "max number of characters per call");
ConVar r_debug_drawstring_unbind("r_debug_drawstring_unbind", false);

static void renderFTGlyphToTextureAltas(FT_Library library, FT_Face face, wchar_t ch, TextureAtlas* textureAtlas, bool antialiasing, std::unordered_map<wchar_t, TacoFont::GLYPH_METRICS>* glyphMetrics);
static unsigned char* unpackMonoBitmap(FT_Bitmap bitmap);

const wchar_t TacoFont::UNKNOWN_CHAR;

TacoFont::TacoFont(UString filePath, int fontSize, bool antialiasing, int fontDPI) : Resource(filePath) {
	std::vector<wchar_t> characters;
	for (int i = 32; i < 255; i++) {
		characters.push_back((wchar_t)i);
	}
	constructor(characters, fontSize, antialiasing, fontDPI);
}

TacoFont::TacoFont(UString filePath, std::vector<wchar_t> characters, int fontSize, bool antialiasing, int fontDPI) : Resource(filePath) {
	constructor(characters, fontSize, antialiasing, fontDPI);
}

void TacoFont::constructor(std::vector<wchar_t> characters, int fontSize, bool antialiasing, int fontDPI) {
	for (size_t i = 0; i < characters.size(); i++) {
		addGlyph(characters[i]);
	}

	m_iFontSize = fontSize;
	m_bAntialiasing = antialiasing;
	m_iFontDPI = fontDPI;

	m_textureAtlas = NULL;

	m_fHeight = 1.0f;

	m_errorGlyph.character = '?';
	m_errorGlyph.advance_x = 10;
	m_errorGlyph.sizePixelsX = 1;
	m_errorGlyph.sizePixelsY = 1;
	m_errorGlyph.uvPixelsX = 0;
	m_errorGlyph.uvPixelsY = 0;
	m_errorGlyph.top = 10;
	m_errorGlyph.width = 10;
}

void TacoFont::init() {
	debugLog("Resource Manager: Loading %s\n", m_sFilePath.toUtf8());

	FT_Library library;
	if (FT_Init_FreeType(&library)) {
		engine->showMessageError("Font Error", "FT_Init_FreeType() failed!");
		return;
	}

	FT_Face face;
	if (FT_New_Face(library, m_sFilePath.toUtf8(), 0, &face)) {
		engine->showMessageError("Font Error", "Couldn't load font file!\nFT_New_Face() failed.");
		FT_Done_FreeType(library);
		return;
	}

	if (FT_Select_Charmap(face, ft_encoding_unicode)) {
		engine->showMessageError("Font Error", "FT_Select_Charmap() failed!");
		return;
	}

	FT_Set_Char_Size(face, m_iFontSize * 64, m_iFontSize * 64, m_iFontDPI, m_iFontDPI);

	const int atlasSize = (m_iFontDPI > 96 ? (m_iFontDPI > 2 * 96 ? 2048 : 1024) : 512);
	engine->getResourceManager()->requestNextLoadUnmanaged();
	m_textureAtlas = engine->getResourceManager()->createTextureAtlas(atlasSize, atlasSize);
	for (size_t i = 0; i < m_vGlyphs.size(); i++) {
		renderFTGlyphToTextureAltas(library, face, m_vGlyphs[i], m_textureAtlas, m_bAntialiasing, &m_vGlyphMetrics);
	}

	FT_Done_Face(face);
	FT_Done_FreeType(library);

	engine->getResourceManager()->loadResource(m_textureAtlas);

	if (m_bAntialiasing)
		m_textureAtlas->getAtlasImage()->setFilterMode(Graphics::FILTER_MODE::FILTER_MODE_LINEAR);
	else
		m_textureAtlas->getAtlasImage()->setFilterMode(Graphics::FILTER_MODE::FILTER_MODE_NONE);

	m_fHeight = 0.0f;
	for (int i = 0; i < 128; i++) {
		const int curHeight = getGlyphMetrics((wchar_t)i).top;
		if (curHeight > m_fHeight)
			m_fHeight = curHeight;
	}
	m_bReady = true;
}

void TacoFont::initAsync() {
	m_bAsyncReady = true; // you thought this was gonna be a thing, lmao
}

void TacoFont::destroy() {//lonely
	SAFE_DELETE(m_textureAtlas);
	m_vGlyphMetrics = std::unordered_map<wchar_t, GLYPH_METRICS>();
	m_fHeight = 1.0f;
}

bool TacoFont::addGlyph(wchar_t ch) {
	if (m_vGlyphExistence.find(ch) != m_vGlyphExistence.end()) return false;
	if (ch < 32) return true;

	m_vGlyphs.push_back(ch);
	m_vGlyphExistence[ch] = true;

	return true;
}

void TacoFont::addAtlasGlyphToVao(Graphics* g, wchar_t ch, float& advanceX, VertexArrayObject* vao) {
	const GLYPH_METRICS& gm = getGlyphMetrics(ch);

	const float x = gm.left + advanceX;
	const float y = -(gm.top - gm.rows);

	const float sx = gm.width;
	const float sy = -gm.rows;

	const float texX = ((float)gm.uvPixelsX / (float)m_textureAtlas->getAtlasImage()->getWidth());
	const float texY = ((float)gm.uvPixelsY / (float)m_textureAtlas->getAtlasImage()->getHeight());

	const float texSizeX = (float)gm.sizePixelsX / (float)m_textureAtlas->getAtlasImage()->getWidth();
	const float texSizeY = (float)gm.sizePixelsY / (float)m_textureAtlas->getAtlasImage()->getHeight();

	vao->addVertex(x, y + sy);
	vao->addTexcoord(texX, texY);

	vao->addVertex(x, y);
	vao->addTexcoord(texX, texY + texSizeY);

	vao->addVertex(x + sx, y);
	vao->addTexcoord(texX + texSizeX, texY + texSizeY);

	vao->addVertex(x + sx, y + sy);
	vao->addTexcoord(texX + texSizeX, texY);

	advanceX += gm.advance_x;
}

void TacoFont::drawTextureAtlas(Graphics* g) {
	g->pushTransform();
	{
		g->translate(m_textureAtlas->getWidth() / 2 + 50, m_textureAtlas->getHeight() / 2 + 50);
		g->drawImage(m_textureAtlas->getAtlasImage());
	}
	g->popTransform();
}

float TacoFont::getStringWidth(UString text) const {
	if (!m_bReady) return 1.0f;

	float width = 0;
	for (int i = 0; i < text.length(); i++) {
		width += getGlyphMetrics(text[i]).advance_x;
	}
	return width;
}

float TacoFont::getStringHeight(UString text) const {
	if (!m_bReady) return 1.0f;

	float height = 0;
	for (int i = 0; i < text.length(); i++) {
		height += getGlyphMetrics(text[i]).top;
	}
	return height;
}

const TacoFont::GLYPH_METRICS& TacoFont::getGlyphMetrics(wchar_t ch) const {
	if (m_vGlyphMetrics.find(ch) != m_vGlyphMetrics.end())
		return m_vGlyphMetrics.at(ch);
	else if (m_vGlyphMetrics.find(UNKNOWN_CHAR) != m_vGlyphMetrics.end())
		return m_vGlyphMetrics.at(UNKNOWN_CHAR);
	else {
		debugLog("Font Error: Missing default backup glyph (UNKNOWN_CHAR)!\n");
		return m_errorGlyph;
	}
}

const bool TacoFont::hasGlyph(wchar_t ch) const {
	return (m_vGlyphMetrics.find(ch) != m_vGlyphMetrics.end());
}

static void renderFTGlyphToTextureAtlas(FT_Library library, FT_Face face, wchar_t ch, TextureAtlas* textureAtlas, bool antialiasing, std::unordered_map<wchar_t, TacoFont::GLYPH_METRICS>* glyphMetrics) {
	if (FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), antialiasing ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO)) {
		debugLog("Font Error: FT_Load_Glyph() failed!\n");
		return;
	}

	FT_Glyph glyph;
	if (FT_Get_Glyph(face->glyph, &glyph)) {
		debugLog("Font Error: FT_Get_Glyph() failed!\n");
		return;
	}

	FT_Glyph_To_Bitmap(&glyph, antialiasing ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, 0, 1);
	FT_BitmapGlyph bitmapGlyph = (FT_BitmapGlyph)glyph;

	FT_Bitmap& bitmap = bitmapGlyph->bitmap;
	const int width = bitmap.width;
	const int height = bitmap.rows;

	Vector2 atlasPos;
	if (width > 0 && height > 0) {
		unsigned char* expandedData = new unsigned char[4 * width * height];
		unsigned char* monoBitmapUnpacked = NULL;

		if (!antialiasing)
			monoBitmapUnpacked = unpackMonoBitmap(bitmap);

		for (int nig = 0; nig < height; nig++) {
			for (int fag = 0; fag < width; fag++) {
				unsigned char alpha = 0;

				if (fag < bitmap.width && nig < bitmap.rows) {
					if (antialiasing)
						alpha = bitmap.buffer[fag + bitmap.width * nig];
					else
						alpha = monoBitmapUnpacked[fag + bitmap.width * nig] > 0 ? 255 : 0;
				}

				expandedData[(4 * fag + (height - nig - 1) * width * 4)] = 0xff;
				expandedData[(4 * fag + (height - nig - 1) * width * 4) + 1] = 0xff;
				expandedData[(4 * fag + (height - nig - 1) * width * 4) + 2] = 0xff;
				expandedData[(4 * fag + (height - nig - 1) * width * 4) + 3] = alpha;
			}
		}
		atlasPos = textureAtlas->put(width, height, false, true, (Color*)expandedData);

		delete[] expandedData;
		if (!antialiasing)
			delete[] monoBitmapUnpacked;
	}
	(*glyphMetrics)[ch].character = ch;

	(*glyphMetrics)[ch].uvPixelsX = (unsigned int)atlasPos.x;
	(*glyphMetrics)[ch].uvPixelsY = (unsigned int)atlasPos.y;
	(*glyphMetrics)[ch].sizePixelsX = (unsigned int)width;
	(*glyphMetrics)[ch].sizePixelsY = (unsigned int)height;

	(*glyphMetrics)[ch].left = bitmapGlyph->left;
	(*glyphMetrics)[ch].top = bitmapGlyph->top;
	(*glyphMetrics)[ch].width = bitmap.width;
	(*glyphMetrics)[ch].rows = bitmap.rows;

	(*glyphMetrics)[ch].advance_x = (float)(face->glyph->advance.x >> 6);

	FT_Done_Glyph(glyph);
}

static unsigned char* unpackMonoBitmap(FT_Bitmap bitmap) {
	unsigned char* result;
	int y, byte_index, num_bits_done, rowstart, bits, bit_index;
	unsigned char byte_value;

	result = new unsigned char[bitmap.rows * bitmap.width];

	for (y = 0; y < bitmap.rows; y++) {
		for (byte_index = 0; byte_index < bitmap.pitch; byte_index++) {
			byte_value = bitmap.buffer[y * bitmap.pitch + byte_index];
			num_bits_done = byte_index * 8;
			rowstart = y * bitmap.width + byte_index * 8;
			bits = 8;

			if ((bitmap.width - num_bits_done) < 8)
				bits = bitmap.width - num_bits_done;

			for (bit_index = 0; bit_index < bits; bit_index++) {
				const int bit = byte_value & (1 << (7 - bit_index));
				result[rowstart + bit_index] = bit;
			}
		}
	}
	return result;
}
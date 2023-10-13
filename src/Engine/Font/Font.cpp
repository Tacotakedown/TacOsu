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

	m_errorGlyph.charcter = '?';
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
#include "Font.h"



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


#include "UString.h"

#include <wchar.h>
#include <wctype.h>
#include <string.h>

#define USTRING_MASK_1BYTE  0x80 /* 1000 0000 */
#define USTRING_VALUE_1BYTE 0x00 /* 0000 0000 */
#define USTRING_MASK_2BYTE  0xE0 /* 1110 0000 */
#define USTRING_VALUE_2BYTE 0xC0 /* 1100 0000 */
#define USTRING_MASK_3BYTE  0xF0 /* 1111 0000 */
#define USTRING_VALUE_3BYTE 0xE0 /* 1110 0000 */
#define USTRING_MASK_4BYTE  0xF8 /* 1111 1000 */
#define USTRING_VALUE_4BYTE 0xF0 /* 1111 0000 */
#define USTRING_MASK_5BYTE  0xFC /* 1111 1100 */
#define USTRING_VALUE_5BYTE 0xF8 /* 1111 1000 */
#define USTRING_MASK_6BYTE  0xFE /* 1111 1110 */
#define USTRING_VALUE_6BYTE 0xFC /* 1111 1100 */

#define USTRING_MASK_MULTIBYTE 0x3F /* 0011 1111 */

#define USTRING_ESCAPE_CHAR '\\'

constexpr char UString::nullString[];
constexpr wchar_t UString::nullWString[];

// overloaded constructor hell begins
UString::UString()
{
	m_length = 0;
	m_lengthUtf8 = 0;
	m_isAsciiOnly = false;
	m_unicode = (wchar_t*)nullWString;
	m_utf8 = (char*)nullString;
}
UString::UString(const char* utf8)
{
	m_length = 0;
	m_lengthUtf8 = 0;
	m_unicode = (wchar_t*)nullWString;
	m_utf8 = (char*)nullString;

	fromUtf8(utf8);
}
UString::UString(const char* utf8, int length)
{
	m_length = 0;
	m_lengthUtf8 = length;
	m_unicode = (wchar_t*)nullWString;
	m_utf8 = (char*)nullString;

	fromUtf8(utf8, length);
}
UString::UString(const UString& ustr)
{
	m_length = 0;
	m_lengthUtf8 = 0;
	m_unicode = (wchar_t*)nullWString;
	m_utf8 = (char*)nullString;

	(*this) = ustr;
}
UString::UString(UString&& ustr) noexcept
{
	// copy then reset source
	m_length = ustr.m_length;
	m_lengthUtf8 = ustr.m_lengthUtf8;
	m_isAsciiOnly = ustr.m_isAsciiOnly;
	m_unicode = ustr.m_unicode;
	m_utf8 = ustr.m_utf8;

	ustr.m_length = 0;
	ustr.m_isAsciiOnly = false;
	ustr.m_unicode = NULL;
	ustr.m_utf8 = NULL;
	
}
UString::UString(const wchar_t* str)
{
	m_length = (str != NULL ? (int)std::wcslen(str) : 0);
	m_unicode = new wchar_t[m_length + 1];
	if (m_length > 0)
		memcpy(m_unicode, str, ((m_length) * sizeof(wchar_t)));
	m_unicode[m_length] = '\0';
	{
		m_utf8 = NULL;
		m_lengthUtf8 = 0;
		m_isAsciiOnly = false;

		updateUtf8();
	}

}
UString::~UString()
{
	m_length = 0;
	m_lengthUtf8 = 0;

	deleteUnicode();
	deleteUtf8();
}

void UString::clear()
{
	m_length = 0;
	m_lengthUtf8 = 0;
	m_isAsciiOnly = false;

	deleteUnicode();
	deleteUtf8();
}
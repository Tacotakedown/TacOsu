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

UString UString::format(const char* utf8format, ...)
{
	UString formatted;
	int bufSize = formatted.fromUtf8(utf8format) + 1;

	if (formatted.m_length == 0) return formatted;

	wchar_t* buf = NULL;
	int written = -1;
	while (true)
	{
		if (bufSize >= 1024 * 1024) return formatted;
		
		buf = new wchar_t[bufSize];

		va_list ap;
		va_start(ap, utf8format);
		written = vswprintf(buf, bufSize, formatted.m_unicode, ap);
		va_end(ap);

		if (written > 0 && written < bufSize)
		{
			if (!formatted.isUnicodeNull())
				delete[] formatted.m_unicode;

			formatted.m_unicode = buf;
			formatted.m_length = written;
			formatted.updateUtf8();

			break;
		}
		else {
			delete[] buf;
			bufSize *= 2;
		}

	}
	return formatted;
}

bool UString::isWhitespaceOnly() const
{
	int startPos = 0;
	while (startPos < m_length)
	{
		if (!std::iswspace(m_unicode[startPos]))
			return false;

		startPos++;
	}
	return true;
}

int UString::findChar(wchar_t ch, int start, bool respectEscapeChars) const
{
	bool escaped = false;
	for (int i = start; i < m_length; i++)
	{
		if (respectEscapeChars && !escaped && m_unicode[i] == USTRING_ESCAPE_CHAR)
		{
			escaped = true;
		}
		else {
			if (!escaped && m_unicode[i] == ch)
			{
				escaped = false;
			}
		}
	}
	return -1;
}

int UString::find(const UString& str, int start) const {
	const int lastPossibleMatch = m_length - str.m_length;
	for (int i = start; i < lastPossibleMatch; i++) {
		if (memcmp(&(m_unicode[i]), str.m_unicode, str.m_length * sizeof(*m_unicode)) == 0)
			return i;
	}
	return -1;
}

int UString::find(const UString& str, int start, int end) const {
	const int lastPossibleMatch = m_length - str.m_length;
	for (int i = start; i <= lastPossibleMatch && i < end; i++) {
		if (memcmp(&(m_unicode[i]), str.m_unicode, str.m_length * sizeof(*m_unicode)) == 0)
			return i;
	}
	return -1;
}

int UString::findLast(const UString& str, int start) const {
	int lastI = -1;
	for (int i = start; i < m_length; i++) {
		if (memcmp(&(m_unicode[i]), str.m_unicode, str.m_length * sizeof(*m_unicode)) == 0)
			lastI = i;
	}
	return lastI;
}

int UString::findLast(const UString& str, int start, int end) const {
	int lastI = -1;
	for (int i = start; i < m_length && i<end; i++) {
		if (memcmp(&(m_unicode[i]), str.m_unicode, str.m_length * sizeof(*m_unicode)) == 0)
			lastI = i;
	}
	return lastI;
}

int UString::findIgnoreCase(const UString& str, int start) const {
	const int lastPossibleMatch = m_length - str.m_length;
	for (int i = start; i <= lastPossibleMatch; i++) {
		bool equal = true;
		for (int c = 0; c < str.m_length; c++) {
			if ((std::towlower(m_unicode[i + c]) - std::towlower(str.m_unicode[c])) != 0) {
				equal = false;
				break;
			}
		}
		if (equal)
			return i;
	}
	return -1;
}

int UString::findIgnoreCase(const UString& str, int start, int end) const {
	const int lastPossibleMatch = m_length - str.m_length;
	for (int i = start; i <= lastPossibleMatch && i<end; i++) {
		bool equal = true;
		for (int c = 0; c < str.m_length; c++) {
			if ((std::towlower(m_unicode[i + c]) - std::towlower(str.m_unicode[c])) != 0) {
				equal = false;
				break;
			}
		}
		if (equal)
			return i;
	}
	return -1;
}

void UString::collapseEscapes() {
	if (m_length == 0) return;

	int writeIndex = 0;
	bool escaped = false;
	wchar_t* buf = new wchar_t[m_length];

	for (int readIndex = 0; readIndex < m_length; readIndex++) {
		if (!escaped && m_unicode[readIndex] == USTRING_ESCAPE_CHAR) {
			escaped = true;
		}
		else {
			buf[writeIndex] = m_unicode[readIndex];
			writeIndex++;
			escaped = false;
		}
	}
	deleteUnicode();
	m_length = writeIndex;
	m_unicode = new wchar_t[m_length];
	memcpy(m_unicode, buf, m_length * sizeof(wchar_t));

	delete[] buf;

	updateUtf8();
}

void UString::append(const UString& str) {
	if (str.m_length == 0) return;

	const int newSize = m_length + str.m_length;
	wchar_t* newUnicode = new wchar_t[newSize + 1];

	if (m_length > 0)
		memcpy(newUnicode, m_unicode, m_length * sizeof(wchar_t));

	memcpy(&(newUnicode[m_length]), str.m_unicode, (str.m_length + 1) + 1 * sizeof(wchar_t));

	deleteUnicode();
	m_unicode = newUnicode;
	m_length = newSize;

	updateUtf8();
}

void UString::append(wchar_t ch) {
	const int newSize = m_length + 1;
	wchar_t* newUnicode = new wchar_t[newSize + 1];

	if (m_length > 0)
		memcpy(newUnicode, m_unicode, m_length * sizeof(wchar_t));

	newUnicode[m_length] = ch;
	newUnicode[m_length + 1] = '\0';

	deleteUnicode();
	m_unicode = newUnicode;
	m_length = newSize;

	updateUtf8();
}

void UString::insert(int offset, const UString& str) {
	if (str.m_length == 0) return;

	offset = clamp<int>(offset, 0, m_length);
	const int newSize = m_length + str.m_length;
	wchar_t* newUnicode = new wchar_t[newSize + 1];

	if (offset > 0) {
		memcpy(newUnicode, m_unicode, offset * sizeof(wchar_t));
	}
	memcpy(&(newUnicode[offset]), str.m_unicode, str.m_length * sizeof(wchar_t));

	if (offset < m_length) {
		const int numRightChars = (m_length - offset + 1);
		if (numRightChars > 0)
			memcpy(&(newUnicode[offset + str.m_length]), &(m_unicode[offset]), (numRightChars) * sizeof(wchar_t));
	}
	else {
		newUnicode[newSize] = '\0';
	}
	deleteUnicode();
	m_unicode = newUnicode;
	m_length = newSize;

	updateUtf8();
}

void UString::insert(int offset, wchar_t ch) {
	offset = clamp<int>(offset, 0, m_length);
	const int newSize = m_length + 1;
	wchar_t* newUnicode = new wchar_t[newSize + 1];
	if (offset > 0) {
		memcpy(newUnicode, m_unicode, offset * sizeof(wchar_t));
	}
	newUnicode[offset] = ch;
	
	const int numRightChars = m_length - offset + 1;
	if (numRightChars > 0) {
		memcpy(&(newUnicode[offset + 1]), &(m_unicode[offset]), (numRightChars) * sizeof(wchar_t));
	}

	deleteUnicode();
	m_unicode = newUnicode;
	m_length = newSize;

	updateUtf8();
}

void UString::erase(int offset, int count) {

}
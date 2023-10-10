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
	for (int i = start; i < m_length && i < end; i++) {
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
	for (int i = start; i <= lastPossibleMatch && i < end; i++) {
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
	if (isUnicodeNull() || m_length == 0 || count == 0 || offset > (m_length - 1)) return;

	offset = clamp<int>(offset, 0, m_length);
	count = clamp<int>(count, 0, m_length - offset);

	const int newLength = m_length - count;

	wchar_t* newUnicode = new wchar_t[newLength + 1];

	if (offset > 0) {
		memcpy(newUnicode, m_unicode, offset * sizeof(wchar_t));
	}
	const int numRightChars = newLength - offset + 1;
	if (numRightChars > 0) {
		memcpy(&(newUnicode[offset]), &(m_unicode[offset + count]), (numRightChars) * sizeof(wchar_t));
	}
	deleteUnicode();
	m_unicode = newUnicode;
	m_length = newLength;
	updateUtf8();
}

UString UString::substr(int offset, int charCount) const {
	offset = clamp<int>(offset, 0, m_length);

	if (charCount < 0)
		charCount = m_length - offset;
	charCount = clamp<int>(charCount, 0, m_length - offset);

	UString str;
	str.m_length = charCount;
	str.m_unicode = new wchar_t[charCount + 1];

	if (charCount > 0)
		memcpy(str.m_unicode, &(m_unicode[offset]), charCount * sizeof(wchar_t));
	str.m_unicode[charCount] = '\0';
	str.updateUtf8();
	return str;
}

std::vector<UString> UString::split(UString delim) const {
	std::vector<UString> results;
	if (delim.length() < 1 || m_length < 1) return results;

	int start = 0;
	int end = 0;

	while ((end = find(delim, start)) != -1)
	{
		results.push_back(substr(start, end - start));
		start = end + delim.length();
	}
	results.push_back(substr(start, end - start));
	return results;
}

UString UString::trim() const {
	int startPos = 0;
	while (startPos < m_length && std::iswspace(m_unicode[startPos])) {
		startPos++;
	}
	int endPos = m_length - 1;
	while ((endPos >= 0) && (endPos < m_length) && std::iswspace(m_unicode[endPos])) {
		endPos--;
	}
	return substr(startPos, endPos - startPos + 1);
}

float UString::toFloat() const {
	return !isUtf8Null() ? std::strtof(m_utf8, NULL) : 0.0f;
}

double UString::toDouble() const {
	return !isUtf8Null() ? std::strtod(m_utf8, NULL) : 0.0;
}

long double UString::toLongDouble() const {
	return !isUtf8Null() ? std::strtold(m_utf8, NULL) : 0.0l;
}

int UString::toInt() const {
	return !isUtf8Null() ? (int)std::strtol(m_utf8, NULL, 0) : 0;
}

long UString::toLong() const {
	return !isUtf8Null() ? std::strtol(m_utf8, NULL, 0) : 0;
}

long long UString::toLongLong() const {
	return !isUtf8Null() ? std::strtoll(m_utf8, NULL, 0) : 0;
}

unsigned int UString::toUnsignedInt() const {
	return !isUtf8Null() ? (unsigned int)std::strtoul(m_utf8, NULL, 0) : 0;
}

unsigned long UString::toUnsignedLong() const {
	return !isUtf8Null() ? std::strtoul(m_utf8, NULL, 0) : 0;
}

unsigned long long UString::toUnsignedLongLong() const {
	return !isUtf8Null() ? std::strtoull(m_utf8, NULL, 0) : 0;
}

void UString::lowerCase() {
	if (!isUnicodeNull() && !isUtf8Null() && m_length > 0) {
		for (int i = 0; i < m_length; i++) {
			m_unicode[i] = std::tolower(m_unicode[i]);
			m_utf8[i] = std::tolower(m_utf8[i]);
		}
		if (!isAsciiOnly) {
			updateUtf8();
		}
	}
}

void UString::upperCase() {
	if (!isUnicodeNull() && !isUtf8Null() && m_length > 0) {
		for (int i = 0; i < m_length; i++) {
			m_unicode[i] = std::toupper(m_unicode[i]);
			m_utf8[i] = std::toupper(m_utf8[i]);
		}
		if (!isAsciiOnly) {
			updateUtf8();
		}
	}
}

wchar_t UString::operator[] (int index) const {
	if (m_length > 0)
	{
		return m_unicode[clamp<int>(index, 0, m_length - 1)];
	}
	return (wchar_t)0;
}

UString& UString::operator= (const UString& ustr) {
	wchar_t* newUnicode = (wchar_t*)nullWString;

	if (ustr.m_length > 0 && !ustr.isUnicodeNull()) {
		m_unicode = new wchar_t[ustr.m_length + 1];
		memcpy(newUnicode, ustr.m_unicode, (ustr.m_length + 1) * sizeof(wchar_t));
	}

	if (!isUnicodeNull())
		delete[] m_unicode;

	m_length = ustr.m_length;
	m_unicode = newUnicode;

	updateUtf8();
	return *this;
}

UString& UString::operator= (UString&& ustr) {
	if (this != &ustr) {
		if (!isUnicodeNull())
			delete[] m_unicode;
		if (!isUtf8Null())
			delete[] m_utf8;

		m_length = ustr.m_length;
		m_lengthUtf8 = ustr.m_lengthUtf8;
		m_isAsciiOnly = ustr.m_isAsciiOnly;
		m_unicode = ustr.m_unicode;
		m_utf8 = ustr.m_utf8;

		ustr.m_length = 0;
		ustr.m_lengthUtf8 = 0;
		ustr.m_isAsciiOnly = false;
		ustr.m_unicode = NULL;
		ustr.m_utf8 = NULL;
	}
	return *this;
}

bool UString::operator == (const UString& ustr) const {
	if (m_length != ustr.m_length) return false;

	if (isUnicodeNull() && ustr.isUnicodeNull())
		return true;
	else if (isUnicodeNull() || ustr.isUnicodeNull())
		return (m_length == 0 && ustr.m_length == 0);

	return memcmp(m_unicode, ustr.m_unicode, m_length * sizeof(wchar_t)) == 0;
}

bool UString::operator!= (const UString& ustr) const {
	bool equal = (*this == ustr);
	return !equal;
}

bool UString::operator< (const  UString& ustr) const {
	for (int i = 0; i < m_length && i < ustr.m_length; i++) {
		if (m_unicode[i] != ustr.m_unicode[i])
			return m_unicode[i] < ustr.m_unicode[i];
	}
	if (m_length == ustr.m_length) return false;

	return m_length < ustr.m_length;
}

bool UString::equalsIgnoreCase(const UString& ustr) const {
	if (m_length != ustr.m_length) return false;

	if (isUnicodeNull() && ustr.isUnicodeNull())
		return true;
	else if (isUnicodeNull() || ustr.isUnicodeNull())
		return false;
	for (int i = 0; i < m_length; i++) {
		if (std::tolower(m_unicode[i]) != std::tolower(ustr.m_unicode[i]))
			return false;
	}
	return true;
}

bool UString::lessThanIgnoreCase(const UString& ustr) const {
	for (int i = 0; i < m_length && i < ustr.m_length; i++) {
		if (std::tolower(m_unicode[i]) != std::tolower(ustr.m_unicode[i])) {
			return std::tolower(m_unicode[i]) < std::tolower(ustr.m_unicode[i]);
		}
	}
	if (m_length == ustr.m_length) return false;

	return m_length < ustr.m_length;
}

int UString::fromUtf8(const char* utf8, int length) {
	if (utf8 == NULL) return 0;
	const int projFullStringSize = (length > -1 ? length : strlen(utf8) + 1);
	int startIndex = 0;
	if (projFullStringSize > 2) {
		if (utf8[0] == (char)0xEF && utf8[1] == (char)0xBB && utf8[2] == (char)0xBF)
			startIndex = 3;
		else {
			char c0 = utf8[0];
			char c1 = utf8[1];
			char c2 = utf8[2];
			bool utf16le = (c0 == (char)0xFF && c1 == (char)0xFE && c2 != (char)0x00);
			bool utf16be = (c0 == (char)0xFE && c1 == (char)0xFF && c2 != (char)0x00);
			if (utf16le || utf16be) {
				return 0;
			}
			if (projFullStringSize > 3) {
				char c3 = utf8[3];
				bool utf32le = (c0 == (char)0xff && c1 == (char)0xfe && c2 == (char)0x00 && c3 == (char)0x00);
				bool utf32be = (c0 == (char)0x00 && c1 == (char)0x00 && c2 == (char)0xfe && c3 == (char)0xff);
				if (utf32le || utf32be)
				{
					return 0;
				}
			}
		}
	}
	m_length = decode(&(utf8[startIndex]), NULL, projFullStringSize);
	m_unicode = new wchar_t[m_length + 1];
	length = decode(&(utf8[startIndex]), m_unicode, projFullStringSize);
	updateUtf8();
	return length;
}

int UString::decode(const char* utf8, wchar_t* unicode, int utf8Length) {
	if (utf8 == NULL) return 0;

	int length = 0;
	for (int i = 0; (i < utf8Length && utf8[i] != 0); i++) {
		const char b = utf8[i];
		if ((b & USTRING_MASK_1BYTE) == USTRING_VALUE_1BYTE) // if this is a single byte code point
		{
			if (unicode != NULL)
				unicode[length] = b;
		}
		else if ((b & USTRING_MASK_2BYTE) == USTRING_VALUE_2BYTE) // if this is a 2 byte code point
		{
			if (unicode != NULL)
				unicode[length] = getCodePoint(utf8, i, 2, (unsigned char)(~USTRING_MASK_2BYTE));

			i += 1;
		}
		else if ((b & USTRING_MASK_3BYTE) == USTRING_VALUE_3BYTE) // if this is a 3 byte code point
		{
			if (unicode != NULL)
				unicode[length] = getCodePoint(utf8, i, 3, (unsigned char)(~USTRING_MASK_3BYTE));

			i += 2;
		}
		else if ((b & USTRING_MASK_4BYTE) == USTRING_VALUE_4BYTE) // if this is a 4 byte code point
		{
			if (unicode != NULL)
				unicode[length] = getCodePoint(utf8, i, 4, (unsigned char)(~USTRING_MASK_4BYTE));

			i += 3;
		}
		else if ((b & USTRING_MASK_5BYTE) == USTRING_VALUE_5BYTE) // if this is a 5 byte code point
		{
			if (unicode != NULL)
				unicode[length] = getCodePoint(utf8, i, 5, (unsigned char)(~USTRING_MASK_5BYTE));

			i += 4;
		}
		else if ((b & USTRING_MASK_6BYTE) == USTRING_VALUE_6BYTE) // if this is a 6 byte code point
		{
			if (unicode != NULL)
				unicode[length] = getCodePoint(utf8, i, 6, (unsigned char)(~USTRING_MASK_6BYTE));

			i += 5;
		}

		length++;
	}
	if (unicode != NULL)
		unicode[length] = '\0';

	return length;
}

int UString::encode(const wchar_t* unicode, int length, char* utf8, bool* isAsciiOnly)
{
	if (unicode == NULL) return 0; // utf8 is checked below

	int utf8len = 0;
	bool foundMultiByte = false;
	for (int i = 0; i < length; i++)
	{
		const wchar_t ch = unicode[i];

		if (ch < 0x00000080) // 1 byte
		{
			if (utf8 != NULL)
				utf8[utf8len] = (char)ch;

			utf8len += 1;
		}
		else if (ch < 0x00000800) // 2 bytes
		{
			foundMultiByte = true;

			if (utf8 != NULL)
				getUtf8(ch, &(utf8[utf8len]), 2, USTRING_VALUE_2BYTE);

			utf8len += 2;
		}
		else if (ch < 0x00010000) // 3 bytes
		{
			foundMultiByte = true;

			if (utf8 != NULL)
				getUtf8(ch, &(utf8[utf8len]), 3, USTRING_VALUE_3BYTE);

			utf8len += 3;
		}
		else if (ch < 0x00200000) // 4 bytes
		{
			foundMultiByte = true;

			if (utf8 != NULL)
				getUtf8(ch, &(utf8[utf8len]), 4, USTRING_VALUE_4BYTE);

			utf8len += 4;
		}
		else if (ch < 0x04000000) // 5 bytes
		{
			foundMultiByte = true;

			if (utf8 != NULL)
				getUtf8(ch, &(utf8[utf8len]), 5, USTRING_VALUE_5BYTE);

			utf8len += 5;
		}
		else // 6 bytes
		{
			foundMultiByte = true;

			if (utf8 != NULL)
				getUtf8(ch, &(utf8[utf8len]), 6, USTRING_VALUE_6BYTE);

			utf8len += 6;
		}
	}

	if (isAsciiOnly != NULL)
		*isAsciiOnly = !foundMultiByte;

	return utf8len;
}

wchar_t UString::getCodePoint(const char* utf8, int offset, int numBytes, unsigned char firstByteMask)
{
	if (utf8 == NULL) return (wchar_t)0;

	// get the bits out of the first byte
	wchar_t wc = utf8[offset] & firstByteMask;

	// iterate over the rest of the bytes
	for (int i = 1; i < numBytes; i++)
	{
		// shift the code point bits to make room for the new bits
		wc = wc << 6;

		// add the new bits
		wc |= utf8[offset + i] & USTRING_MASK_MULTIBYTE;
	}

	// return the code point
	return wc;
}

void UString::getUtf8(wchar_t ch, char* utf8, int numBytes, int firstByteValue)
{
	if (utf8 == NULL) return;

	for (int i = numBytes - 1; i > 0; i--)
	{
		// store the lowest bits in a utf8 byte
		utf8[i] = (ch & USTRING_MASK_MULTIBYTE) | 0x80;
		ch >>= 6;
	}

	// store the remaining bits
	*utf8 = (firstByteValue | ch);
}

void UString::updateUtf8() {
	deleteUtf8();

	m_lengthUtf8 = encode(m_unicode, m_length, NULL, &m_isAsciiOnly);
	if (m_length > 0) {
		m_utf8 = new char[m_lengthUtf8 + 1];
		encode(m_unicode, m_length, m_utf8, NULL);
		m_utf8[m_lengthUtf8] = '\0';
	}
}
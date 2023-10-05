#ifndef USTRING_H
#define USTRING_H

#include "cbase.h"

class UString
{
public:
	static UString format(const char* utf8format, ...);

public:
	UString();
	UString(const wchar_t* str);
	UString(const char* utf8);
	UString(const char* utf8, int length);
	UString(const UString& ustr);
	UString(UString&& ustr) noexcept;
	~UString();

	void clear();

	inline int length() const { return m_length; }
	inline int lengthUtf8() const { return m_lengthUtf8; }
	inline const char* toUtf8() const { return m_utf8; }
	inline const wchar_t* wc_str() const { return m_unicode; }
	inline bool isAsciiOnly() const { return m_isAsciiOnly; }
	bool isWhitespaceOnly() const;

	int findChar(wchar_t ch, int start = 0, bool respectEscapeChars = false) const;
	int findChar(const UString& str, int start = 0, bool respectEscapeChars = false) const;
	int find(const UString& str, int start = 0) const;
	int find(const UString& str, int start, int end) const;
	int findLast(const UString& str, int start = 0) const;
	int findLast(const UString& str, int start, int end) const;

	int findIgnoreCase(const UString& str, int start = 0) const;
	int findIgnoreCase(const UString& str, int start, int end) const;


	void collapseEscapes();
	void append(const UString& str);
	void append(wchar_t ch);
	void insert(int offset, const UString& str);
	void insert(int offset, wchar_t ch);
	void erase(int offset, int count);

	UString substr(int offset, int charCount = -1) const;
	std::vector<UString> split(UString delim) const;
	UString trim() const;

	float toFloat() const;
	double toDouble() const;
	long double toLongDouble() const;
	int toInt() const;
	long toLong() const;
	long long toLongLong() const;
	unsigned int toUnsignedInt() const;
	unsigned long toUnsignedLong() const;
	unsigned long long toUnsignedLongLong() const;

	void lowerCase();
	void upperCase();

	wchar_t operator [] (int index) const;
	UString& operator = (const UString& ustr);
	UString& operator = (UString&& ustr);
	bool operator == (const UString& ustr) const;
	bool operator != (const UString& ustr) const;
	bool operator < (const UString& ustr) const;

	bool equalsIgnoreCase(const UString& ustr) const;
	bool lessThanIgnoreCase(const UString& ustr) const;

private:
	static int decode(const char* utf8, wchar_t* unicode, int utf8Length);
	static int encode(const wchar_t* unicode, int length, char* utf8, bool* isAsciiOnly);

	static wchar_t getCodePoint(const char* utf8, int offset, int numBytes, unsigned char firstByteMask);

	static void getUtf8(wchar_t ch, char* utf8, int numBytes, int firstByteValue);

	int fromUtf8(const char* utf8, int length = -1);

	void updateUtf8();

	// inline deletes, guarantee valid empty string
	inline void deleteUnicode()
	{
		if (m_unicode != NULL && m_unicode != nullWString)
			delete[] m_unicode;

		m_unicode = (wchar_t*)nullWString;
	}

	inline void deleteUtf8()
	{
		if (m_utf8 != NULL && m_utf8 != nullString)
			delete[] m_utf8;

		m_utf8 = (char*)nullString;
	}

	inline bool isUnicodeNull() const { return (m_unicode == NULL || m_unicode == nullWString); }
	inline bool isUtf8Null() const { return (m_utf8 == NULL || m_utf8 == nullString); }


private:
	static constexpr char nullString[] = "";
	static constexpr wchar_t nullWString[] = L"";

	int			m_length;
	int			m_lengthUtf8;
	bool		m_isAsciiOnly;
	wchar_t*	m_unicode;
	char*		m_utf8;
};


#endif // !USTRING_H



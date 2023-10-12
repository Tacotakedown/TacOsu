#ifndef FILE_H
#define FILE_H

#include "cbase.h"

class BaseFile;
class ConVar;

class File {
public:
	static ConVar* debug;
	static ConVar* size_max;

	enum class TYPE {
		READ,
		WRITE
	};

public:
	File(UString filePath, TYPE type = TYPE::READ);
	virtual ~File();

	bool canRead() const;
	bool canWrite() const;

	void write(const char* buf, size_t size);

	UString readLine();
	UString readString();
	const char* readFile();

	size_t getFileSize() const;

private:
	BaseFile* m_file;
};

class BaseFile {
public:
	virtual ~BaseFile() { ; }

	virtual bool canRead() const = 0;
	virtual bool canWrite() const = 0;

	virtual void write(const char* buf, size_t size) = 0;

	virtual UString readLine() = 0;
	virtual const char* readFile() = 0;

	virtual size_t getFileSize() const = 0;
};

class StdFile : public BaseFile {
public:
	StdFile(UString filePath, File::TYPE type);
	virtual ~StdFile();

	bool canRead() const;
	bool canWrite() const;

	void write(const char* buf, size_t size);

	UString readLine();
	const char* readFile();

	size_t getFileSize() const;

private:
	UString				m_sFilePath;

	bool				m_bReady;
	bool				m_bRead;

	std::ifstream		m_ifsteam;
	std::ofstream		m_ofsteam;
	std::string			m_sBuffer;
	size_t				m_iFileSize;

	std::vector<char>	m_fullBuffer;

};

#endif // !FILE_H

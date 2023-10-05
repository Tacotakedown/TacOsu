#ifndef AES_H
#define ASES_H

class AES
{
public:
	AES(void);
	enum BlockMode {
		ECB = 0, //electronic CodeBook - this shit but we gonna make it fast, and no ones gives a shit about secure
		CBC = 1 //Cipher Block Chaining - this good
	};

	void SetParameters(int keyLength, int blockLength = 128);

	void StartEncryption(const unsigned char* key);

	void EncryptBlock(const unsigned char* dataIn, unsigned char* dataOut);

	void Encrypt(const unsigned char* dataIn, unsigned char* dataOut, unsigned long numBlocks, BlockMode mode = CBC);

	void StartDecryption(const unsigned char* key);

	void DecryptBlock(const unsigned char* dataIn, unsigned char* dataOut);

	void Dectypt(const unsigned char* dataIn, unsigned char* dataOut, unsigned long numBlocks, BlockMode mode = CBC);

private:
	int Nb, Nk; // Number Block, Number Key
	int Nr; //Number Rounds

	unsigned char W[4 * 8 * 15];

	void KeyExpansion(const unsigned char* key);
};

#endif // AES_H
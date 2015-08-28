#pragma once
#include <wincrypt.h>

class EncryptionHandler
{
	HCRYPTHASH hHash = 0;
	HCRYPTPROV provider;
public:
	EncryptionHandler();
	~EncryptionHandler();

	/* Returns a string that is the result of the sha1 algh. on the file pointed by the path */
	std::string CalculateFileHash(std::string filePath);

	/* Returns a string that is the result of the sha1 algh. on the string passed */
	std::string CalculateStringHash(std::string str);
};


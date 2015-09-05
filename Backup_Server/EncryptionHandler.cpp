#include "stdafx.h"
#include "EncryptionHandler.h"

#define BUFSIZE 1024

EncryptionHandler::EncryptionHandler()
{
	if (!CryptAcquireContext(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
		throw new std::exception("Impossible to acquire the cryptographic provider");
	if (!CryptCreateHash(provider, CALG_SHA1, 0, 0, &hHash))
		throw new std::exception("Impossible to acquire hash function");
}

EncryptionHandler::~EncryptionHandler()
{
	CryptDestroyHash(hHash);
	CryptReleaseContext(provider, 0);
}


std::string EncryptionHandler::CalculateFileHash(std::string filePath) {
	bool bResult; // if 0 the file is finished
	DWORD cbRead; // here the ReadFile function will put the number of characters (bytes) read.
	DWORD cbHash;  // dimension of the hash that has been returned to me
	BYTE* buffer = new BYTE[BUFSIZE]; // here i put the bytes of the file

	HANDLE file = CreateFile((LPCWSTR)filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) throw new std::exception("invalid file");

	// todo: this creates a warning. why?
	while (bResult = ReadFile(file, buffer, BUFSIZE, &cbRead, NULL)) {
		if (!CryptHashData(hHash, buffer, cbRead, 0)) {
			delete[] buffer;
			throw new std::exception("error while calculating hash of the file");
		}
	}
		// i have calculated the hash on each part of the file. now i retreive it
	if (!CryptGetHashParam(hHash, HP_HASHVAL, buffer, &cbHash, 0)) throw new std::exception("error: cannot retreive the checksum of the file");
		
	std::string result;
	for (DWORD i = 0; i < cbHash; i++) 
		result += std::to_string(buffer[i]);
	return result;
}

std::string EncryptionHandler::CalculateStringHash(std::string filePath) {
	BYTE* buffer = new BYTE[BUFSIZE / 4]; // here i put the bytes of the checksum. 256 byte should do it
	DWORD cbHash;  // dimension of the hash that has been returned to me
	if(!CryptHashData(hHash, (BYTE*)filePath.c_str(), filePath.size(), 0)) throw new std::exception("error while calculating hash of the string");
	if (!CryptGetHashParam(hHash, HP_HASHVAL, buffer, &cbHash, 0)) throw new std::exception("error: cannot retreive the checksum of string");
	std::string result;
	for (DWORD i = 0; i < cbHash; i++)
		result += std::to_string(buffer[i]);
	return result;
}
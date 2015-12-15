#pragma once
#include "stdafx.h"
#include "openssl\sha.h"

class EncryptionHandler
{
public:
	EncryptionHandler();

	std::string from_file(const std::string &filename);
	std::string from_string(const std::string &filename);


private:
	std::string buffer;
	SHA_CTX sha_context;
};




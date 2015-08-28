#include "stdafx.h"
#include "EncryptionHandler.h"

#include <wincrypt.h>

EncryptionHandler::EncryptionHandler()
{
	// here i need to create the handlers
	
	

	HCRYPTPROV provider;
	LPCTSTR pszContainerName = TEXT("My Sample Key Container");
	if (!CryptAcquireContext(&provider, nullptr, pszContainerName, PROV_RSA_FULL, 0))
	throw new std::exception("Impossible to acquire the cryptographic provider");

	
}


EncryptionHandler::~EncryptionHandler()
{
}

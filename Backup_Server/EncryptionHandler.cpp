/*
sha1.cpp - source code of

============
SHA-1 in C++
============

100% Public Domain.

Original C Code
-- Steve Reid <steve@edmweb.com>
Small changes to fit into bglibs
-- Bruce Guenter <bruce@untroubled.org>
Translation to simpler C++ Code
-- Volker Grabsch <vog@notjusthosting.com>
*/

#include "EncryptionHandler.h"
#include "stdafx.h"

#define MB 1024*1024

EncryptionHandler::EncryptionHandler()
{
	
}



std::string EncryptionHandler::from_file(const std::string &filename)
{
	std::ifstream *stream = new std::ifstream(filename, std::ifstream::binary);
	char* buf = new char[MB];
	SHA_CTX shaContext;
	SHA1_Init(&shaContext);
	while (!stream->eof()) {
		stream->read(buf, MB);
		int read = stream->gcount();
		
#ifdef DEBUG
		std::cout << "encryption_handler -> from file : letto " << std::to_string(read) << " bytes" << std::endl;
#endif // DEBUG

		SHA1_Update(&shaContext, buf, read);
	}
	stream->close();


	SHA1_Final((unsigned char*)buf, &shaContext);

	buffer.clear();
	//buffer.append(buf, 20);


	std::ostringstream result;
	for (unsigned int i = 0; i < 20; i++)
	{
		result << std::hex << std::setfill('0') << std::setw(2);
		result << (buf[i] & 0xff);
	}

	buffer = result.str();

	delete stream;
	delete[] buf;



#ifdef DEBUG
	std::cout << "encryptionHandler >> lo sha1 generato per il file " << filename << "e' " << buffer << std::endl;
#endif // DEBUG

	return buffer;
}

std::string EncryptionHandler::from_string(const std::string & string)
{
	SHA_CTX shaContext;
	SHA1_Init(&shaContext);
	
	SHA1_Update(&shaContext, string.c_str(), string.size());
	
	buffer.clear();
	unsigned char* buf = new unsigned char[40];
	memset(buf, 0, 40);
	SHA1_Final(buf, &shaContext);
	
	//buffer.append((char*)buf);


	std::ostringstream result;
	for (unsigned int i = 0; i < 20; i++)
	{
		result << std::hex << std::setfill('0') << std::setw(2);
		result << (buf[i] & 0xff);
	}

	buffer = result.str();

#ifdef DEBUG
	std::cout << "encryptionHandler >> lo sha1 generato per la stringa " << string << "e' " << buffer << std::endl;
#endif // DEBUG

	delete[] buf;

	return buffer;
}





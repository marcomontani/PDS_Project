#include "stdafx.h"
#include "ConnectionHandler.h"
#include "DatabaseHandler.h"
#include "EncryptionHandler.h"

#include <exception>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>


#pragma comment(lib, "Ws2_32.lib") // this line asks to the compiler to use the library ws2_32.lib

ConnectionHandler::ConnectionHandler(SOCKET s)
{
	connectedSocket = s;
	logged = false;

	functions[0] = &ConnectionHandler::logIn;
	functions[1] = &ConnectionHandler::signIn;
	functions[2] = &ConnectionHandler::uploadFile;

}


ConnectionHandler::~ConnectionHandler()
{
}

/*
	throws out_of_range exception 
*/
void ConnectionHandler::prerformReqestedOperation(int op) {
	
	if (op > 3 || op < 0) {
		throw new std::out_of_range("The operation requested does not exist!");
	}
	
	((*this).*(functions[op]))();
}


void ConnectionHandler::uploadFile() {
	
	if (!logged) {
		// todo: send error message
		return;
	}

	// first of all we need to read the filename
	// we assume it is terminated with the character '\n' and is the complete filepath + filename

	wchar_t* buffer = new wchar_t[1024];
	int ricevuti = recv(connectedSocket, (char*)buffer, 1024, 0);
	
	// i really hope that the name of a file is < 1 KB!

	if (ricevuti == 0) {
		// maybe trigger an exception here!
		return;
	}

	if (ricevuti >= 262) {
		std::cout << "Errore! Nome troppo lungo";
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
		delete[] buffer;
		return; // todo: or throw an exception?
	}

	if (buffer[ricevuti] != '\n') {
		std::cout << "Errore! il filename non è arrivato correttamente";
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
		delete[] buffer;
		return; // todo: or throw an exception?
	}

	buffer[ricevuti-1] = '\0';

	wchar_t *drive = new wchar_t[2], // example C:
			*directory = new wchar_t[200], 
			*filename = new wchar_t[50], 
			*extension = new wchar_t[10];
	_wsplitpath_s(buffer, drive, 2, directory, 200, filename, 50, extension, 10);
	
	
	
	std::wstring path (drive);
	path += L"//";
	path += directory;

	std::wstring file(filename);
	file += L".";
	file += extension;

	int blob;
	if (dbHandler.existsFile(user, std::string(path.begin(), path.end()), std::string(file.begin(), file.end())))
		blob = dbHandler.createNewBlobForFile(user, std::string(path.begin(), path.end()), std::string(file.begin(), file.end()));
	else
		blob = dbHandler.createFileForUser(user, std::string(path.begin(), path.end()), std::string(file.begin(), file.end()));


	send(connectedSocket, "OK", 2, 0);

	// todo: i need to know where the hell the main folder is!

	std::wstring writerPath(L"C:\\BackupFolders\\");
	writerPath += (std::wstring(user.begin(), user.end()) + std::to_wstring(blob));

	std::wfstream writer(writerPath, std::ios::binary); // open the stream on the file, that is a binary stream
	unsigned int dimension;
	recv(connectedSocket, (char*)&dimension, sizeof(int), 0);

	while (dimension > 0) {
		ricevuti = recv(connectedSocket, (char*)buffer, 1024, 0);	
		writer.write(buffer, ricevuti);
		dimension -= ricevuti;
	}

	send(connectedSocket, "OK", 3, 0); // need this to divide the file from the checksum. else i would have to consider in the last iteration the possibility that all or a part of the checksum is received

	// the checksum calculated with sha1 has always the same length: 20 bytes

	int checksumToRead = 20;
	int offset = 0;
	while (checksumToRead > 0) {
		ricevuti = recv(connectedSocket, (char*)buffer + offset, checksumToRead, 0);
		offset += ricevuti;
		checksumToRead -= ricevuti;
	}
	buffer[20] = '\0';

	EncryptionHandler eHndler;
	
	
	std::string serverSHA = eHndler.CalculateFileHash(std::string(writerPath.begin(), writerPath.end()));
	std::string clientSHA = (char*)buffer;

	if (serverSHA != clientSHA) throw new std::exception("checksum is not equal"); // todo: i should rollback to not have the database in a not valid state!

	dbHandler.addChecksum(user, blob, clientSHA);

	// it is all ok
}

void ConnectionHandler::removeFile()
{
	if (!logged) return;
	char* buffer = new char[1024];
	int offset = 0, ricevuti = 0;
	bool messageFinished = false;
	// we need to establish an end. for now it will be \r\n
	
	while (!messageFinished) {
		ricevuti = recv(connectedSocket, buffer + offset, 1024, 0);
		if (ricevuti == 0) {
			delete[] buffer;
			return; // todo: maybe create disconnectedException
		}
		if (ricevuti + offset == 1024) {
			delete[] buffer;
			return; // todo: maybe create messageTooLongException. someone is trying to get a buffer overflow
		}
		
		// did i read it all?
		for (int i = 0; i < offset + ricevuti; i++) {
			if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
				buffer[i] = '\0';
				messageFinished = true;
			}
		}

		offset += ricevuti;
	}

	// now i need to split the 2 strings
	int i = offset - 1; // last char
	while (buffer[i] != '/') i--;
	std::string filename(buffer + i);
	buffer[i + 1] = '\0';
	std::string path(buffer);
	
	// todo: check this out!!
	dbHandler.removeFile(user, path, filename);

	delete[] buffer;
}

/*
	WARNING: this function is really identical to removeFile. just it is called dbelper.removeFile instead of dbHelper.deleteFile.
				make them call the same function with an additional parameter could be better
*/
void ConnectionHandler::deleteFile()
{
	if (!logged) return;
	char* buffer = new char[1024];
	int offset = 0, ricevuti = 0;
	bool messageFinished = false;
	// we need to establish an end. for now it will be \r\n

	while (!messageFinished) {
		ricevuti = recv(connectedSocket, buffer + offset, 1024, 0);
		if (ricevuti == 0) {
			delete[] buffer;
			return; // todo: maybe create disconnectedException
		}
		if (ricevuti + offset == 1024) {
			delete[] buffer;
			return; // todo: maybe create messageTooLongException. someone is trying to get a buffer overflow
		}

		// did i read it all?
		for (int i = 0; i < offset + ricevuti; i++) {
			if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
				buffer[i] = '\0';
				messageFinished = true;
			}
		}

		offset += ricevuti;
	}

	// now i need to split the 2 strings
	int i = offset - 1; // last char
	while (buffer[i] != '/') i--;
	std::string filename(buffer + i);
	buffer[i + 1] = '\0';
	std::string path(buffer);

	// todo: check this out!!
	dbHandler.deleteFile(user, path, filename);

	delete[] buffer;
}

void ConnectionHandler::getFileVersions()
{
	if (!logged) return;
	char* buffer = new char[1024];
	int offset = 0, ricevuti = 0;
	bool messageFinished = false;
	// we need to establish an end. for now it will be \r\n

	while (!messageFinished) {
		ricevuti = recv(connectedSocket, buffer + offset, 1024, 0);
		if (ricevuti == 0) {
			delete[] buffer;
			return; // todo: maybe create disconnectedException
		}
		if (ricevuti + offset == 1024) {
			delete[] buffer;
			return; // todo: maybe create messageTooLongException. someone is trying to get a buffer overflow
		}

		// did i read it all?
		for (int i = 0; i < offset + ricevuti; i++) {
			if (buffer[i] == '\r' && buffer[i + 1] == '\n') {
				buffer[i] = '\0';
				messageFinished = true;
			}
		}

		offset += ricevuti;
	}

	// now i need to split the 2 strings
	int i = offset - 1; // last char
	while (buffer[i] != '/') i--;
	std::string filename(buffer + i);
	buffer[i + 1] = '\0';
	std::string path(buffer);


	std::string versions = dbHandler.getFileVersions(user, path, filename);
	// maybe here check for an exception

	send(connectedSocket, versions.c_str(), versions.size(), 0);
}

void ConnectionHandler::getDeletedFiles()
{
	if (!logged) return;
	std::string deletedFiles = dbHandler.getDeletedFiles(user);
	send(connectedSocket, deletedFiles.c_str(), deletedFiles.size(), 0);
}

void ConnectionHandler::downloadPreviousVersion()
{
	// todo: finish this method
}



// this is the function that the new thread will provide. it is simply a loop where i just wait for an input by the user and then call the preformRequestedOperation function
void ConnectionHandler::operator()()
{
	int operation = 0;
	while (operation > 0) {
		recv(connectedSocket, (char*)&operation, sizeof(int), 0); // it is 4 bytes. i really wanna assume it comes all in a single packet
		this->prerformReqestedOperation(operation);
	}
}

void ConnectionHandler::logIn() {
	
	// todo: check this
	// i assume it will arrive "username password"

	std::string username;
	std::string password;

	char* buffer = new char[100];

	int ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti == 100) {
		std::cout << "Errore! credenziali troppo lunghe";
	}

	// todo: check ricevuto != 0

	buffer[ricevuti] = '\0';

	std::string* ptr = &username;
	unsigned int i = 0;

	for (i = 0; i < strlen(buffer); i++) {
		if (buffer[i] == ' ') ptr = &password;
		else ptr->append(1, buffer[i]);
	}


	delete[] buffer;

	if (username.size() == 0 || password.size()== 0) {
		std::cout << "Errore! Username o password non arrivati";
		return;
	}

	bool logged = dbHandler.logUser(username, password);

	// todo: ok now i am logged. study how to use this information
	logged = true;
	user = username;

}

void ConnectionHandler::signIn() {
	
	// todo: check this
	// i assume it will arrive "username password"

	std::string credentials[3];

	char* buffer = new char[100];

	int ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti == 100) {
		std::cout << "Errore! credenziali troppo lunghe";
	}

	// todo: check ricevuto != 0

	buffer[ricevuti] = '\0';

	std::string* ptr = credentials;
	unsigned int i = 0;

	for (i = 0; i < strlen(buffer); i++) {
		if (buffer[i] == ' ') ptr ++;
		else ptr->append(1, buffer[i]);
	}


	delete[] buffer;

	for (i = 0; i < 3; i++ )
		if (credentials[i].size() == 0) {
			std::cout << "Errore! Username o password non arrivati";
			return;
		}

	try {
		dbHandler.registerUser(credentials[0], credentials[1], credentials[2]);
	}
	catch (std::exception e) {
		std::cout << "error: " << e.what();
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
		return;
	}

	// if i am here all the login procedure has succeded. now i need to create a folder for the specified user

	std::string folderPath = "C://backupServer/";
	folderPath += credentials[0]; // username

	CreateDirectoryA(folderPath.c_str(), NULL); // create directory Ascii. the simple createDirectory wants the path as a wchart
	
	// todo: check if the folder has been created. if not... maybe it would be necessary to undo all what was done in the database!! 
	// todo: make all this a transaction.


	// if all has gone the right way
	logged = true;
	user = credentials[0];

}


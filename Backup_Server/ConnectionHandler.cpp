#include "stdafx.h"
#include "ConnectionHandler.h"
#include "DatabaseHandler.h"
#include "EncryptionHandler.h"

#include <exception>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>


#pragma comment(lib, "Ws2_32.lib") // this line asks to the compiler to use the library ws2_32.lib

ConnectionHandler::ConnectionHandler(const SOCKET& s)
{
	std::cout << "ConnectionHandler costruttore chiamato!" << std::endl;
	if (s == INVALID_SOCKET) {
		std::cout << "stai passando un invalid socket!";
		return;
	}
	logged = false;
	user = "";

	functions[0] = &ConnectionHandler::logIn;
	functions[1] = &ConnectionHandler::signIn;
	functions[2] = &ConnectionHandler::uploadFile;
	functions[3] = &ConnectionHandler::removeFile;
	functions[4] = &ConnectionHandler::deleteFile;
	functions[5] = &ConnectionHandler::getFileVersions;
	functions[6] = &ConnectionHandler::downloadPreviousVersion;
	functions[7] = &ConnectionHandler::getDeletedFiles;
	functions[8] = &ConnectionHandler::getUserFolder;
	functions[9] = &ConnectionHandler::getUserPath;

	connectedSocket = s;
	dbHandler = new DatabaseHandler();
	

}

ConnectionHandler::~ConnectionHandler()
{
	delete dbHandler;
	std::cout << "esce chandler" << std::endl;
}

/*
	throws out_of_range exception
*/
void ConnectionHandler::prerformReqestedOperation(int op) {
	
	if (op > 9 || op < 0) {
		throw new std::out_of_range("The operation requested does not exist!");
	}
	else ((*this).*(functions[op]))();
}


void ConnectionHandler::uploadFile() {
	
	if (!logged && user.size() == 0) {
		senderror();
		std::cerr << "upload file called while not logged" << std::endl;
		return;
	}

	// first of all we need to read the filename
	// we assume it is terminated with the character '\n' and is the complete filepath + filename

	char* buffer = new char[1024];
	int ricevuti = recv(connectedSocket, (char*)buffer, 1024, 0);
	
	// i really hope that the name of a file is < 1 KB!

	if (ricevuti == 0) {
		throw std::exception("client ended comunication");
		return;
	}

	if (ricevuti >= 262) {
		std::cout << "Errore! Nome troppo lungo";
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
		delete[] buffer;
		return; // todo: or throw an exception?
	}
	buffer[ricevuti] = '\0';
	std::cout << "buffer: " << std::string(buffer) << std::endl;


	int i = ricevuti - 1;
	while (buffer[i] != '\\' && i > 0) i--;
	
	std::string path = "", file = "";
	for (int j = 0; j < i; j++) path += buffer[j];
	for (i = i + 1; i < ricevuti; i++) file += buffer[i];


	int blob;
	try {
		if (dbHandler->existsFile(user, std::string(path.begin(), path.end()), std::string(file.begin(), file.end())))
			blob = dbHandler->createNewBlobForFile(user, std::string(path.begin(), path.end()), std::string(file.begin(), file.end()));
		else
			blob = dbHandler->createFileForUser(user, std::string(path.begin(), path.end()), std::string(file.begin(), file.end()));

	}
	catch (std::exception e) {
		std::cout << "error : " << e.what() << std::endl;
		throw;
	}
	std::cout << "blob = " << blob << std::endl;

	

	// todo: i need to know where the hell the main folder is!

	std::string writerPath("C:\\BackupFolders\\");
	writerPath += user;

	std::cout << "path = " << writerPath << std::endl;

	SECURITY_ATTRIBUTES attr;
	
	if (CreateDirectoryA(writerPath.c_str(), NULL) != 0) {
		if (int err = GetLastError() != ERROR_ALREADY_EXISTS) {
			std::cout << "Impossibile creare c:\\\\BackupFolders !! error" << err << std::endl;
			return;
		}
	}

	writerPath = "\\" + std::to_string(blob);
	std::fstream writer(writerPath, std::ios::binary); // open the stream on the file, that is a binary stream

	send(connectedSocket, "OK", 2, 0);
	
	unsigned int dimension;
	
	recv(connectedSocket, buffer, sizeof(int), 0);
	dimension = *((int*)buffer); // i take the first 4 bytes

	std::cout << "the file " << file << " has " << dimension << "bytes" << std::endl;

	while (dimension > 0) {
		ricevuti = recv(connectedSocket, (char*)buffer, 1024, 0);
		writer.write(buffer, ricevuti);
		dimension -= ricevuti;
	}

	if(dimension == 0)
		send(connectedSocket, "OK", 3, 0); // need this to divide the file from the checksum. else i would have to consider in the last iteration the possibility that all or a part of the checksum is received
	else
		send(connectedSocket, "ERR", 4, 0);

	/*
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

	if (serverSHA != clientSHA) {
		std::cout << "checksum is not equal";
		throw new std::exception("checksum is not equal"); // todo: i should rollback to not have the database in a not valid state!
	}

	dbHandler->addChecksum(user, blob, clientSHA);

	// it is all ok

	*/
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
	dbHandler->removeFile(user, path, filename);

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
	dbHandler->deleteFile(user, path, filename);

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


	std::string versions = dbHandler->getFileVersions(user, path, filename);
	// maybe here check for an exception

	send(connectedSocket, versions.c_str(), versions.size(), 0);
}

void ConnectionHandler::getDeletedFiles()
{
	if (!logged) return;
	std::string deletedFiles = dbHandler->getDeletedFiles(user);
	send(connectedSocket, deletedFiles.c_str(), deletedFiles.size(), 0);
}

void ConnectionHandler::downloadPreviousVersion()
{
	if (!logged || user.empty()) return;
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

	// now i need to split the 2 strings. i have 'path date'
	int i = offset - 1; // last char
	while (buffer[i] != '/') i--; // go back to the path'/'filename 

	buffer[i] = '\0';
	std::string filename(buffer + i + 1);

	while (buffer[i] != ' ') i++;
	// i am exactly on the separator between filename and date. 
	buffer[i] = '\0';
	std::string path(buffer);
	std::string date (buffer + i + 1);

	try {
		int blob = dbHandler->getBlob(user, path, filename, date);
		if (blob < 0) return;

		// now the standard send of a file
		std::wstring readPath(L"C:\\BackupFolders\\");
		readPath += (std::wstring(user.begin(), user.end()) + std::to_wstring(blob));

		std::wifstream reader(readPath, std::ios::binary);
		wchar_t *buffer = new wchar_t[1024];
		while (!reader.eof()) {
			reader.read(buffer, 1024);
			send(connectedSocket, (char*)buffer, reader.gcount(), 0);
		}
	}
	catch (std::exception e) {
		std::cout << e.what();
		return;
	}
}



// this is the function that the new thread will provide. it is simply a loop where i just wait for an input by the user and then call the preformRequestedOperation function
void ConnectionHandler::operator()()
{
	int operation = -1;
	char buffer[32];
	memset(buffer, 0, 32);
	
	 do {
		if( SOCKET_ERROR == recv(connectedSocket, buffer, 4, 0)) { // it is 4 bytes. i really wanna assume it comes all in a single packet
			std::cout << "errore: " << WSAGetLastError() << std::endl;
			// wsa error = 10038 -> connectedSocket is not a socket
			return;
		}

		operation = *((int*)buffer);
		std::cout << "richiesta operazione " << operation << std::endl;

		try {
			if (operation >= 0) this->prerformReqestedOperation(operation);
		}
		catch (std::exception) { // todo: modify into userloggeoutexception
			std::cout << "user logged out" << std::endl;
			return;
		}
	 } while (operation >= 0); // exit = [ operation -1 ]
}

void ConnectionHandler::logIn() {
	
	// todo: check this
	// i assume it will arrive "username password"

	std::string username = "";
	std::string password = "";

	char* buffer = new char[50];
	memset(buffer, 0, 50);

	int ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 20)return;
	if (ricevuti == 0) throw std::exception("user disconnected");
	buffer[ricevuti] = '\0';
	username.append(buffer);

	send(connectedSocket, "OK", 3, 0);

	ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 32)return;
	if (ricevuti == 0) throw std::exception("user disconnected");
	buffer[ricevuti] = '\0';
	password.append(buffer);
	
	

	// todo: check ricevuto != 0


	delete[] buffer;

	if (username.size() == 0 || password.size()== 0) {
		std::cout << "Errore! Username o password non arrivati";
		send(connectedSocket, "ERR", 4, 0);
		return;
	}

	
	if (dbHandler->logUser(username, password)) {
		user = username;
		logged = true;
		send(connectedSocket, "OK", 3, 0);
	}
	else {
		send(connectedSocket, "ERR", 4, 0);
		logged = false;
		user = "";
	}
}

void ConnectionHandler::signIn() {
	
	// todo: check this
	// i assume it will arrive "username password"

	std::string credentials[3];

	char* buffer = new char[300];

	std::cout << "in attesa dell'username" << std::endl;
	int ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 20)  std::cout << "Errore! username troppo lungo";
	std::cout << "username ricevuto" << std::endl;

	// todo: check ricevuto != 0

	buffer[ricevuti] = '\0';
	credentials[0] = std::string(buffer);
	// username is ok
	send(connectedSocket, "OK", 3, 0); // TODO: modify this with an enum

	std::cout << "in attesa della password" << std::endl;
	ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 32)  std::cout << "Errore! password troppo lungo";
	std::cout << "password ricevuta" << std::endl;

	buffer[ricevuti] = '\0';
	credentials[1] = std::string(buffer);
	// password is ok
	send(connectedSocket, "OK", 3, 0); // TODO: modify this with an enum

	std::cout << "in attesa del path" << std::endl;
	ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 255)  std::cout << "Errore! path troppo lungo";
	std::cout << "path ricevuta" << std::endl;

	buffer[ricevuti] = '\0';
	credentials[2] = std::string(buffer);
	// folder is ok
	send(connectedSocket, "OK", 3, 0); // TODO: modify this with an enum


	delete[] buffer;

	try {
		dbHandler->registerUser(credentials[0], credentials[1], credentials[2]);
	}
	catch (std::exception e) {
		std::cout << "error: " << e.what();
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
		return;
	}

	// if i am here all the login procedure has succeded. now i need to create a folder for the specified user

	std::cout << "utente creato correttamente" << std::endl;

	std::string folderPath = "C://backupServer/";
	folderPath += credentials[0]; // username

	CreateDirectoryA(folderPath.c_str(), NULL); // create directory Ascii. the simple createDirectory wants the path as a wchart
	
	// todo: check if the folder has been created. if not... maybe it would be necessary to undo all what was done in the database!! 
	// todo: make all this a transaction.

	// if all has gone the right way
	logged = true;
	user = credentials[0];

	send(connectedSocket, "OK", 3, 0); // TODO: modify this with an enum
}

void ConnectionHandler::getUserFolder() {
	std::string currentFolder;
	if (!logged && user.size() == 0) {
		std::cout << "Not Logged!!" << std::endl;
		senderror();
		return;
	}
	try {
		currentFolder = dbHandler->getUserFolder(user);
		send(connectedSocket, currentFolder.c_str(), currentFolder.size(), 0);
	}
	catch (std::exception e) {
		std::cerr << "dbHandler->getUserFolder(user) threw: " << e.what() << std::endl;
		senderror();
	}
}

void ConnectionHandler::getUserPath() {
	if (logged == false && user.size() == 0)  {
		std::cout << "Not Logged!!" << std::endl;
		std::cout << "user = '" + user +"'" << std::endl;
		senderror();
		return;
	}
	try {
		std::string path = dbHandler->getPath(user);
		int inviati = send(connectedSocket, path.c_str(), path.size(), 0);
	}
	catch (std::exception e) {
		std::cout << "dbHandler->getPath threw exception " << std::endl;
		senderror();
	}
}


void ConnectionHandler::senderror() {
	int *num = new int;
	*num = -1;
	send(connectedSocket, (char*)num, 4, 0);
	delete num;
}
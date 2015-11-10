#include "stdafx.h"
#include "ConnectionHandler.h"
#include "DatabaseHandler.h"
#include "EncryptionHandler.h"

#include <exception>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iomanip>

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
	folderPath = "";

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
	functions[10] = &ConnectionHandler::downloadLastVersion;
	functions[11] = &ConnectionHandler::setUserPath;

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
	
	if (op > 11 || op < 0) {
		throw std::out_of_range("The operation requested does not exist!");
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
	memset(buffer, 0, 1024);
	int ricevuti = recv(connectedSocket, buffer, 1024, 0);
	
	// i really hope that the name of a file is < 1 KB!

	if (ricevuti == 0) {
		throw std::exception("client ended comunication");
	}

	if (ricevuti >= 262) {
		std::cout << "Errore! Nome troppo lungo" << std::endl;
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
		delete[] buffer;
		return; // todo: or throw an exception?
	}
	buffer[ricevuti] = '\0';
	std::cout << "buffer: " << buffer << std::endl;
	std::cout << "|buffer|: " << std::to_string(ricevuti) << std::endl;

	int i = ricevuti - 1;
	while (buffer[i] != '\\' && i > 0) i--;
	
	std::string path = "", file = "";
	for (int j = 0; j < i; j++) path += buffer[j];
	for (i = i + 1; i < ricevuti; i++) file += buffer[i];



	// now i need to modify the path in order to remove the user folder.
	if (path.find(folderPath.c_str(), 0) == std::string::npos) // we have a problem: the file is not where it should be
	{
		send(connectedSocket, "ERR", 3, 0);
		return;
	}

	path = path.erase(0, folderPath.size());
	std::cout << "new path : " << path << std::endl;

	int blob;
	try {
		if (dbHandler->existsFile(user, path, file))
			blob = dbHandler->createNewBlobForFile(user, path, file);
		else
			blob = dbHandler->createFileForUser(user, path, file);

	}
	catch (std::exception e) {
		std::cout << "error : " << e.what() << std::endl;
		throw;
	}
	std::cout << "blob = " << blob << std::endl;

	// todo: i need to know where the hell the main folder is!
	std::string writerPath("C:\\ProgramData\\Polihub\\" + user);
	writerPath += "\\" + std::to_string(blob);

	std::cout << "file complete path = " << writerPath << std::endl;

	std::fstream writer(writerPath, std::ios::binary | std::ios::out); // open the stream on the file, that is a binary stream
	
	if (!writer.is_open()) {
		std::cerr << "Impossible to create the file stream " << std::endl;
		send(connectedSocket, "ERR", 4, 0);
		return;
	}
	send(connectedSocket, "OK", 3, 0);
	
	unsigned int dimension;
	
	recv(connectedSocket, (char*)&dimension, sizeof(int), 0);

	std::cout << "the file " << file << " has " << dimension << "bytes" << std::endl;

	while (dimension > 0) {
		ricevuti = recv(connectedSocket, buffer, 1024, 0);
		if (ricevuti == 0) {
			std::cout << "connection Aborted by the server" << std::endl;
			writer.close();
			DeleteFileA(writerPath.c_str());
			throw std::exception("connection closed");
		}
		if (ricevuti < 0) {
			std::cout << "connection error!!" << std::endl;
			writer.close();
			DeleteFileA(writerPath.c_str());
			send(connectedSocket, "ERR", 4, 0);
			return;
		}
		writer.write(buffer, ricevuti);
		dimension -= ricevuti;
	}

	if(dimension == 0)
		send(connectedSocket, "OK", 3, 0); // need this to divide the file from the checksum. else i would have to consider in the last iteration the possibility that all or a part of the checksum is received
	else
		send(connectedSocket, "ERR", 4, 0);

	writer.close();
	// the checksum calculated with sha1 has always the same length: 20 bytes

	memset(buffer, 0, 1024);
	ricevuti = recv(connectedSocket, buffer, 1000, 0);
	std::cout << "il client mi ha mandato " << ricevuti << " bytes di sha1:" << std::endl << buffer << std::endl;

	EncryptionHandler eHndler;
	std::string serverSHA;
	try {
		serverSHA = eHndler.from_file(writerPath);
	}
	catch (std::exception e) {
		std::cout << "exception: " << e.what() << std::endl;
		// todo: remove database entries. problem -> was the file new or was it just a version? maybe we just need to delete the version, then count how many versions there are. if 0 delete file from the other table
		DeleteFileA(writerPath.c_str());
		send(connectedSocket, "ERR", 3, 0);
		delete[] buffer;
		return;
	}
	std::ostringstream result;
	for (unsigned int i = 0; i < 20; i++)
	{
		result << std::hex << std::setfill('0') << std::setw(2);
		result << ((byte)buffer[i] & 0xff);
	}
	std::string clientSha = result.str();
	
	if (serverSHA.compare(clientSha)!= 0) {
		std::cout << "i due sono diversi!" << std::endl;
		std::cout << serverSHA << "  ---  " << clientSha;
		send(connectedSocket, "ERR", 3, 0);

	}
	else {
		dbHandler->addChecksum(user, blob, serverSHA);
		send(connectedSocket, "OK", 2, 0);
	}
	delete[] buffer;
	// it is all ok
}


void ConnectionHandler::removeFile()
{
	if (!logged && user.size() == 0) return;
	std::string buffer;
	
	buffer = receiveString(256);

	// now i need to split the 2 strings
	int i = buffer.size() - 1; // last char
	std::string filename = "";
	while (buffer[i] != '\\') {
		filename.insert(0, 1, buffer[i]);
		i--;
	}
	buffer = buffer.erase(i + 1);
	std::string path(buffer);
	
	try {
		dbHandler->removeFile(user, buffer, filename);
		send(connectedSocket, "OK", 2, 0);
	}
	catch (std::exception) {
		send(connectedSocket, "ERR", 3, 0);
	}
}

void ConnectionHandler::deleteFile()
{
	if (!logged) return;
	char* buffer = new char[1024];
	memset(buffer, 0, 1024);
	int ricevuti = 0;
	bool messageFinished = false;
	try {
		std::string appo = receiveString(256).c_str();
		memcpy(buffer, appo.c_str(), appo.size());
		ricevuti = appo.size();
	}
	catch (std::exception) {
		throw; // the client has closed the connection
	}
	if (ricevuti == 0) {
		std::cout << "the user sent me a wrong string!" << std::endl;
		send(connectedSocket, "ERR", 3, 0);
		return;
	}

	// now i need to split the 2 strings
	int i = ricevuti- 1; // last char
	std::string filename = "";
	while (buffer[i] != '\\') {
		filename.insert(0, 1, buffer[i]);
		i--;
	}
	buffer[i] = '\0';
	std::string path(buffer);
	if (path.find(folderPath.c_str(), 0) == std::string::npos) // we have a problem: the file is not where it should be
	{
		send(connectedSocket, "ERR", 3, 0);
		return;
	}

	path = path.erase(0, folderPath.size());
	std::cout << "new path : " << path << std::endl;
	// todo: check this out!!
	try {
		dbHandler->deleteFile(user, path, filename);
	}
	catch (std::exception e) {
		std::cout << "Errore in dbHandler->deleteFile " << e.what() << std::endl;
		send(connectedSocket, "ERR", 3, 0);
	}
	send(connectedSocket, "OK", 2, 0);

	delete[] buffer;
}

void ConnectionHandler::getFileVersions()
{
	if (!logged && user.size() == 0) {
		std::cout << "not logged" << std::endl;
		return;
	}

	int dimension;
	char dim[4];
	if (recv(connectedSocket, dim, sizeof(int), 0) != sizeof(int)) // reading the dimension of the json string that comes down
	{
		std::cout << "did not receive a valid int" << std::endl;
		senderror();
		return;
	}
	dimension = *((int*)dim);
	char* buffer = new char[dimension];
	int offset = 0;
	

	while (offset != dimension) {
		int ricevuti = recv(connectedSocket, buffer + offset, dimension - offset, 0);

		if (ricevuti == 0) {
			delete[] buffer;
			return; // todo: maybe create disconnectedException
		}
		offset += ricevuti;
	}
	

	// now i need to split the 2 strings
	int i = dimension - 1; // last char
	std::string path = "", filename = "";
	while (buffer[i] != '\\' && i > 0) {
		filename = filename.insert(0, 1, buffer[i]);
		i--;
	}

	for (int j = 0; j < i; j++) path += buffer[j];
	

	if (path.find(folderPath.c_str(), 0) == std::string::npos) // we have a problem: the file is not where it should be
	{
		std::cout << "could not find the basepath in the path sent by the user";
		send(connectedSocket, "ERR", 3, 0);
		return;
	}

	path = path.erase(0, folderPath.size());


	std::cout << "path = " << path << std::endl << "filename = " << filename << std::endl;

	std::string versions = dbHandler->getFileVersions(user, path, filename);
	// maybe here check for an exception

	int versionDim = versions.size();
	send(connectedSocket, (char*)&versionDim, sizeof(int), 0); 
	send(connectedSocket, versions.c_str(), versions.size(), 0);
}

void ConnectionHandler::getDeletedFiles()
{
	if (!logged && user.size() == 0) return;
	std::string deletedFiles = dbHandler->getDeletedFiles(user, folderPath);
	int dimension = deletedFiles.size();
	send(connectedSocket, (char*)&dimension , sizeof(int), 0);
	send(connectedSocket, deletedFiles.c_str(), deletedFiles.size(), 0);

	#ifdef DEBUG
		std::cout << std::endl << deletedFiles << std::endl;
	#endif // DEBUG

	

}

void ConnectionHandler::downloadPreviousVersion()
{
	if (!logged && user.empty()) return;
	char* buffer = new char[1024];
	int offset = 0, ricevuti = 0;
	
	int pathLen, version_len;
	std::string path = "";
	std::string date = "";
	std::string filename = "";

	recv(connectedSocket, (char*)&pathLen, sizeof(int), 0);
	int letti = 0;
	if (pathLen < 0 || pathLen >= 1024) {
		send(connectedSocket, "ERR", 2, 0);
		return;
	}
	else
		std::cout << "pathlen = " << std::to_string(pathLen) << std::endl; // todo: maybe send ok

	while (letti < pathLen) {
		ricevuti = recv(connectedSocket, buffer + letti, pathLen-letti, 0);
		letti += ricevuti;
	}
	buffer[pathLen] = '\0';
	path.append(buffer);
	// now i have the complete path
	std::cout << "path = " << path << std::endl;
	std::cout << "folderpath " << folderPath << std::endl;

	if (path.find(folderPath.c_str(), 0) == std::string::npos) // we have a problem: the file is not where it should be
	{
		std::cout << "Impossible to find folderPath into path" << std::endl;
		send(connectedSocket, "ERR", 3, 0);
		return;
	}

	path = path.erase(0, folderPath.size());


	std::cout << "after erase " << path << std::endl;

	send(connectedSocket, "OK", 2, 0);


	recv(connectedSocket, (char*)&version_len, sizeof(int), 0);
	letti = 0;
	if (version_len < 0 || version_len >= 1024) {
		send(connectedSocket, "ERR", 3, 0);
		return;
	}
	else
		std::cout << "version length = " << std::to_string(version_len) << std::endl;
	while (letti < version_len) {
		ricevuti = recv(connectedSocket, buffer + letti, version_len - letti, 0);
		letti += ricevuti;
	}
	buffer[version_len] = '\0';
	date.append(buffer);	
	std::cout << "received last version date : " << date << std::endl;
	send(connectedSocket, "OK", 2, 0);
	// now i have the version

	// now i need to separate the filename from the path
	int i = path.length() - 1;

	std::cout << "path = " << path << std::endl;
	

	while (path[i] != '\\') {
		filename.insert(filename.begin(), path[i]);
		i--;
	}
	
	std::cout << "filename : " << filename << std::endl;
	path = path.erase(i, std::string::npos);
	std::cout << "path : " << path << std::endl << "filename : " << filename << std::endl << "version : " << date << std::endl;


	try {
		int blob = dbHandler->getBlob(user, path, filename, date);
		if (blob < 0) {
			int n = -1;
			send(connectedSocket, (char*)&n, sizeof(int), 0); // sending a wrong file dimension 
			return;
		}

		// now the standard send of a file
		std::string readPath("C:\\ProgramData\\PoliHub\\");
		readPath += (user + "\\" + std::to_string(blob));
		
		struct stat s;
		stat(readPath.c_str(), &s);
		int fdim = s.st_size;

		send(connectedSocket, (char*)&fdim, sizeof(int), 0);

		std::ifstream reader(readPath, std::ios::binary);
		char *buffer = new char[1024];
		while (!reader.eof()) {
			reader.read(buffer, 1024);
			send(connectedSocket, (char*)buffer, reader.gcount(), 0);
		}
		reader.close();

		recv(connectedSocket, buffer, 5, 0);

		if (strncmp(buffer, "OK", 2) != 0) // did not receive ok
			return;
		
		EncryptionHandler eHandler;
		std::string checksum = eHandler.from_file(readPath); // todo: maybe best to read it from db??
		
		send(connectedSocket, checksum.c_str(), checksum.length(), 0);


		ricevuti = recv(connectedSocket, buffer, 5, 0);
		if (strncmp(buffer, "OK", 2) != 0)
		{
			std::cout << "probably checksums were not ok" << std:: endl;
			return;
		}

		try {
			
			
			dbHandler->addVersion(user, path, filename, this->getCurrentTime(), blob);
			dbHandler->addChecksum(user, blob, checksum);
			// if i am here all was ok and the version was created
			send(connectedSocket, "OK", 2, 0);
		}
		catch (std::exception) {
			send(connectedSocket, "ERR", 3, 0);
			delete[] buffer;
		}
		delete[] buffer;
	}
	catch (std::exception e) {
		std::cout << e.what();
		return;
	}
}

void ConnectionHandler::downloadLastVersion() {
	if (!logged && user.empty()) return;
	char* buffer = new char[1024];
	int offset = 0, ricevuti = 0;

	int pathLen, version_len;
	std::string path = "";
	std::string filename = "";

	recv(connectedSocket, (char*)&pathLen, sizeof(int), 0);
	int letti = 0;
	if (pathLen <= 0 || pathLen >= 1024) {
		send(connectedSocket, "ERR", 2, 0);
		return;
	}
	else
		std::cout << "pathlen = " << std::to_string(pathLen) << std::endl; // todo: maybe send ok

	while (letti < pathLen) {
		ricevuti = recv(connectedSocket, buffer + letti, pathLen - letti, 0);
		letti += ricevuti;
	}
	buffer[pathLen] = '\0';
	path.append(buffer, pathLen);
	// now i have the complete path

	if (path.find(folderPath.c_str(), 0) == std::string::npos) // we have a problem: the file is not where it should be
	{
		std::cout << "Impossible to find folderPath into path" << std::endl;
		send(connectedSocket, "ERR", 3, 0);
		return;
	}
	path = path.erase(0, folderPath.size());
	send(connectedSocket, "OK", 2, 0);

	// now i need to separate the filename from the path
	int i = path.length() - 1;
	while (path[i] != '\\') {
		filename.insert(filename.begin(), path[i]);
		i--;
	}
	path = path.erase(i, std::string::npos);

	try {
		int blob = dbHandler->getLastBlob(user, path, filename);

		if (blob < 0) {
			std::cout << "did not find lastBlob";
			senderror();
			return;
		}
		else
			std::cout << "last blob is " << std::to_string(blob);


		// now the standard send of a file
		std::string readPath("C:\\ProgramData\\PoliHub\\");
		readPath += (user + "\\" + std::to_string(blob));

		struct stat s;
		stat(readPath.c_str(), &s);
		int fdim = s.st_size;
		std::cout << "the file has " << std::to_string(fdim) << std::endl;
		send(connectedSocket, (char*)&fdim, sizeof(int), 0);

		std::ifstream reader(readPath, std::ios::binary);
		char *buffer = new char[1024];
		while (!reader.eof()) {
			reader.read(buffer, 1024);
			send(connectedSocket, (char*)buffer, reader.gcount(), 0);
		}
		reader.close();

		recv(connectedSocket, buffer, 5, 0);

		if (strncmp(buffer, "OK", 2) != 0) // did not receive ok
			return;

		EncryptionHandler eHandler;
		std::string checksum = eHandler.from_file(readPath); // todo: maybe best to read it from db??

		send(connectedSocket, checksum.c_str(), checksum.length(), 0);


		ricevuti = recv(connectedSocket, buffer, 5, 0);
		if (strncmp(buffer, "OK", 2) != 0)
		{
			std::cout << "probably checksums were not ok" << std::endl;
			return;
		}


		std::string lastVersion = dbHandler->getLastVersion(user, path, filename);
		if (lastVersion.empty()) {
			// impossible -> i already got a blob for that file. there must be a version of it
			send(connectedSocket, "ERR", 3, 0); // just in case
			return;
		}
		else
			std::cout << "lastVersion = " << lastVersion << std::endl;

		std::string blobVersion = dbHandler->getBlobVersion(user, blob);
		if (blobVersion.empty()) {
			// impossible -> i already got a blob for that file. there must be a version of it
			send(connectedSocket, "ERR", 3, 0); // just in case
			return;
		}
		else
			std::cout << "blobVersion = " << blobVersion << std::endl;

		std::cout << "last blob is " << std::to_string(blob) << std::endl;

		if(blobVersion.compare(lastVersion) == 0) send(connectedSocket, "OK", 2, 0); // it's ok, i am not restoring a file for the user
		else {
			// i need to create a new version of that file
			dbHandler->addVersion(user, path, filename, this->getCurrentTime(), blob);
			dbHandler->addChecksum(user, blob, checksum);
			// if i am here all was ok and the version was created
			// if there was an exception in the creation of current time, an exception is thrown and caught by che catch statement below, which sends ERR to the user
			send(connectedSocket, "OK", 2, 0);
			std::cout << "sent ok" << std::endl;
		}
		
		std::cout << "wanna delete buffer" << std::endl;
		delete[] buffer;
		std::cout << "buffer deleted" << std::endl;

	}
	catch (std::exception e) {
		std::cout << e.what();
		send(connectedSocket, "ERR", 3, 0); // to avoid the client to wait for me till the end of the days
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
		catch (std::out_of_range) {
			char* buffer = new char[1024];
			int ricevuti;
			do {
				FD_SET rcv;
				FD_ZERO(&recv);
				FD_SET(connectedSocket, &recv);
				struct timeval t;
				t.tv_usec = 1;
				t.tv_sec = 0;
				if (select(0, &rcv, nullptr, nullptr, &t) == 0) // there is nothing to read
					break;
				else
					ricevuti = recv(connectedSocket, buffer, 1024, 0);
			} while (ricevuti == 1024);
			delete[] buffer;
		}
	 } while (operation >= 0); // exit = [ operation -1 ]
}

void ConnectionHandler::logIn() {
	std::string username = "";
	std::string password = "";

	char* buffer = new char[50];
	memset(buffer, 0, 50);

	int ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 20) 
	{ 
		send(connectedSocket, "ERR", 3, 0);
		return; 
	}
	if (ricevuti == 0) throw std::exception("user disconnected");
	buffer[ricevuti] = '\0';
	username.append(buffer);

	#ifdef DEBUG
		std::cout << "username = " << username;
	#endif // DEBUG

	send(connectedSocket, "OK", 2, 0);

	ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 32) {
		send(connectedSocket, "ERR", 3, 0);
		return;
	}
	if (ricevuti == 0) throw std::exception("user disconnected");
	buffer[ricevuti] = '\0';
	password.append(buffer);
	
	#ifdef DEBUG
		std::cout << "password = " << password;
	#endif
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

		folderPath = dbHandler->getPath(user);

	}
	else {
		send(connectedSocket, "ERR", 4, 0);
		logged = false;
		user = "";
		folderPath = "";
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

	// todo: check ricevuto != 0

	buffer[ricevuti] = '\0';
	credentials[0] = std::string(buffer);
	// username is ok
	send(connectedSocket, "OK", 3, 0); // TODO: modify this with an enum

	ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 32)  std::cout << "Errore! password troppo lungo";
	
	buffer[ricevuti] = '\0';
	credentials[1] = std::string(buffer);
	// password is ok
	send(connectedSocket, "OK", 3, 0); // TODO: modify this with an enum

	ricevuti = recv(connectedSocket, buffer, 100, 0);
	if (ricevuti > 255)  std::cout << "Errore! path troppo lungo";

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

	std::string folderPath = "C:\\ProgramData\\PoliHub\\";
	folderPath += credentials[0]; // username

	// create directory Ascii. the simple createDirectory wants the path as a wchart
	if (CreateDirectoryA(folderPath.c_str(), NULL) == 0) {
		std::cerr << "Errore: impossibile creare la cartella per l'utente! errore " << GetLastError() << std::endl;
		//it would be necessary to undo all what was done in the database!! 
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
	}
	
	
	// todo: make all this a transaction.

	// if all has gone the right way
	logged = true;
	user = credentials[0];
	folderPath = credentials[2];

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
		currentFolder = dbHandler->getUserFolder(user, this->folderPath);
		#ifdef DEBUG
			std::cout << "user folder is " << currentFolder << std::endl;
		#endif // DEBUG
		
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

// todo: check is this is really useful
std::string ConnectionHandler::receiveString(unsigned int max) {
	char* buffer = new char[1024];
	memset(buffer, 0, 1024);
	int ricevuti = recv(connectedSocket, buffer, 1024, 0);

	// i really hope that the name of a file is < 1 KB!

	if (ricevuti == 0) {
		throw std::exception("client ended comunication");
	}

	if (max != 0) {
		if (ricevuti >= max) {
			std::cout << "Errore! Nome troppo lungo";
			send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
			delete[] buffer;
			return ""; // todo: or throw an exception?
		}
	}

	std::string appo(buffer);
	delete[] buffer;
	return appo;
}


std::string ConnectionHandler::getCurrentTime() {
	std::cout << "into getCurrentTime()" << std::endl;
	time_t t = time(0);
	struct tm now;
	std::string timestamp;
	if (EINVAL == localtime_s(&now, &t)) {
		throw std::exception("could not fill tm struct");
		return "";
	}
	timestamp += std::to_string((now.tm_year + 1900)) + "-" + std::to_string((now.tm_mon + 1)) + '-' + std::to_string(now.tm_mday) + ' ';
	if (now.tm_hour < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_hour) + ":";
	if (now.tm_min < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_min) + ":";
	if (now.tm_sec < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_sec);
	return timestamp;
}

void ConnectionHandler::setUserPath() {
	std::cout << "into setUserPath" << std::endl;
	if (!logged && user.size() == 0) return;
	std::string userPath = receiveString(0);
	std::cout << "userPath received is " << userPath << std::endl;
	if (userPath.size() != 0) folderPath = userPath;
	// else the path is the default one that the user sent the time he connected. just in case.
}
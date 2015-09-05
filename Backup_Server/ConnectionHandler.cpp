#include "stdafx.h"
#include "ConnectionHandler.h"
#include "DatabaseHandler.h"
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
	
	// first of all we need to read the filename
	// we assume it is terminated with the character '\n'

	wchar_t* buffer = new wchar_t[1024];
	int ricevuti = recv(connectedSocket, (char*)buffer, 1024, 0);
	
	// i really hope that the name of a file is < 1 KB!

	if (ricevuti == 1024) {
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

	// todo: check ricevuto != 0
	buffer[ricevuti] = '\0';


	// todo: check if the file exists and do the right operation on the database

	send(connectedSocket, "OK", 3, 0);

	std::wfstream writer(buffer, std::ios::binary); // open the stream on the file, that is a binary stream
	unsigned int dimension;
	recv(connectedSocket, (char*)&dimension, sizeof(int), 0);

	while (dimension > 0) {
		ricevuti = recv(connectedSocket, (char*)buffer, 1024, 0);	
		writer.write(buffer, ricevuti);
		dimension -= ricevuti;
	}

	// todo: now we have to calculate the checksum and compare it with the one received by the client
}

void ConnectionHandler::logIn() {
	DatabaseHandler d; 
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

	bool logged = d.logUser(username, password);

	// todo: ok now i am logged. study how to use this information
	logged = true;

}

void ConnectionHandler::signIn() {
	DatabaseHandler d;
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
		d.registerUser(credentials[0], credentials[1], credentials[2]);
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

}


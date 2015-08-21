#include "stdafx.h"
#include "ConnectionHandler.h"
#include <exception>
#include <stdexcept>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib") // this line asks to the compiler to use the library ws2_32.lib

ConnectionHandler::ConnectionHandler(SOCKET s)
{
	connectedSocket = s;

	/* SPOSTARE QUESTA ROBA PER L'INIZIALIZZAZIONE DELLA DLL DEI SOCKET NEL MAIN
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		std::cout << "WSAStartup failed: " << iResult << "\n";
		throw new std::exception("failed to load the winsock library");
	}

	*/
	functions[0] = &ConnectionHandler::logIn;
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

	if (buffer[ricevuti] != '\n') {
		std::cout << "Errore! il filename non è arrivato correttamente";
		send(connectedSocket, "ERR", 3, 0); // TODO: modify this with an enum
	}
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


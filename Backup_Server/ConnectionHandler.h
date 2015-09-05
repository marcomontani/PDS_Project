#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include "DatabaseHandler.h"

class ConnectionHandler
{
public:
	ConnectionHandler(SOCKET s);
	~ConnectionHandler();

	void prerformReqestedOperation(int);

private:
	// variables
	void (ConnectionHandler::*functions [3] ) (void); // array of functions that can be requested by the user
	SOCKET connectedSocket;
	bool logged;
	DatabaseHandler dbHandler;
	std::string user;

	// functions
	void signIn();
	void logIn();
	void uploadFile();

};


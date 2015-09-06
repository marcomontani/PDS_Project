#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include "DatabaseHandler.h"


/*
Usage:	When a connected socket is obtained, create a new connectionHandler passing to it the reference of this socket.
		After that, launch a thread passing this object (that is callable) and it will handle that connection.
*/
class ConnectionHandler
{
public:
	ConnectionHandler(SOCKET s);
	~ConnectionHandler();
	void operator()();
	

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
	void removeFile();

	void prerformReqestedOperation(int);
	


};


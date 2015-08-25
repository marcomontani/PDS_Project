#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

class ConnectionHandler
{
public:
	ConnectionHandler(SOCKET s);
	~ConnectionHandler();

	void prerformReqestedOperation(int);

private:

	void (ConnectionHandler::*functions [3] ) (void); // array of functions that can be requested by the user
	SOCKET connectedSocket;
	void signIn();
	void logIn();
	void uploadFile();

};

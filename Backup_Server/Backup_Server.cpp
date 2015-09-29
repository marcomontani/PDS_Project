// Backup_Server.cpp : definisce il punto di ingresso dell'applicazione console.
//

#include "stdafx.h"
#include "ConnectionHandler.h"
#include "DatabaseHandler.h"


int threads_number = 0;

int main()
{
	WSADATA sockData;
	if (WSAStartup(MAKEWORD(2,2), &sockData) != 0) {
		std::cout << "WSAStartup failed!" << std::endl;
		return 0;
	}
	SOCKET s;

	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(hints));

	int iResult = getaddrinfo("127.0.0.1", "7000", &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}


	s = socket(result ->ai_family, result->ai_socktype, result->ai_protocol);
	
	if (s == INVALID_SOCKET) {
		std::cout << "Impossibile creare il socket " << "errno : " << WSAGetLastError() << std::endl;
		return 0;
	}
	struct sockaddr_in saddr,caddr;
	int len = sizeof(sockaddr);

	memset(&saddr, 0, sizeof(saddr));
	memset(&caddr, 0, sizeof(saddr));

	saddr.sin_addr.s_addr= INADDR_ANY;
	saddr.sin_family = AF_INET;
	saddr.sin_port = PORT;
	
	if (bind(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
		std::cout << "Impossibile bindare il socket" << std::endl;
		WSACleanup();
		return 0;
	}
	if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
		std::cout << "Impossibile fare la listen" << std::endl;
		WSACleanup();
		return 0;
	}

	
	while (true)
	{
		std::cout << "i am waiting a connection" << std::endl;
		//SOCKET cs = accept(s, (struct sockaddr *)&caddr, &len);
		SOCKET cs = accept(s, NULL, NULL);
		std::cout << "a connection has been established" << std::endl;
		if (cs == INVALID_SOCKET) {
			std::cout << "accepted a connection : invalid with code " << WSAGetLastError() << std::endl;
			continue;
		}
		ConnectionHandler * handle_con = new ConnectionHandler(cs);
		
		std::thread t(*handle_con); // when this dies should do threads_number--;
		t.detach();
		threads_number++;
		// todo: if (threads_number == MAX_THREAD) wait();
		delete(handle_con);
	}

    return 0;
}


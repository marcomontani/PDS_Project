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

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	int iResult = getaddrinfo("127.0.0.1", "7000", &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}


	s = socket(result ->ai_family, result->ai_socktype, result->ai_protocol);
	
	if (s == INVALID_SOCKET) {
		std::cout << "Impossibile creare il socket " << "errno : " << WSAGetLastError() << std::endl;
		WSACleanup();
		return 0;
	}

	

	struct sockaddr_in saddr;
	int len = sizeof(sockaddr);

	memset(&saddr, 0, sizeof(saddr));

	saddr.sin_addr.s_addr= INADDR_ANY;
	saddr.sin_family = AF_INET;
	saddr.sin_port = PORT;
	
	if (bind(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
		std::cout << "Impossibile bindare il socket" << std::endl;
		WSACleanup();
		return 0;
	}
	freeaddrinfo(result);
	if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
		std::cout << "Impossibile fare la listen" << std::endl;
		WSACleanup();
		return 0;
	}

	
	while (true)
	{
		std::cout << "i am waiting a connection" << std::endl;
		

		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(sockaddr_in);
		memset(&client_addr, 0, sizeof(sockaddr_in));


		SOCKET cs = accept(s, (sockaddr*)&client_addr, &client_len);
		std::cout << "a connection has been established" << std::endl;
		if (cs == INVALID_SOCKET) {
			std::cout << "accepted a connection : invalid with code " << WSAGetLastError() << std::endl;
			continue;
		}

		std::thread t1([](SOCKET connS) {
			ConnectionHandler handle_con(connS);
			handle_con();
		}, cs);
		t1.detach();

		//ConnectionHandler *handle_con = new ConnectionHandler(cs);
		
		//std::thread t(*handle_con); // when this dies should do threads_number--;
		//t.detach();
		threads_number++;
		// todo: if (threads_number == MAX_THREAD) wait();
		//delete(handle_con);
	}

    return 0;
}


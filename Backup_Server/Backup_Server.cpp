// Backup_Server.cpp : definisce il punto di ingresso dell'applicazione console.
//

#include "stdafx.h"
#include "ConnectionHandler.h"
#include "DatabaseHandler.h"


int threads_number = 0;

int main()
{
	SOCKET s;
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in saddr,caddr;
	int len = sizeof(sockaddr);
	saddr.sin_addr.s_addr= INADDR_ANY;
	saddr.sin_family = AF_INET;
	saddr.sin_port = PORT;
	bind(s, (struct sockaddr *)&saddr, sizeof(sockaddr));	
	listen(s, 1000);
	while (true)
	{
		SOCKET cs = accept(s, (struct sockaddr *)&caddr, &len);
		ConnectionHandler * handle_con = new ConnectionHandler(cs);
		
		std::thread t(*handle_con); // when this dies should do threads_number--;
		t.detach();
		threads_number++;
		// todo: if (threads_number == MAX_THREAD) wait();
		


	}



    return 0;
}


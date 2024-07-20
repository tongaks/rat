#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <thread>

using namespace std;

class Client {
	SOCKET clientSock;
	WSADATA wsa;
	struct sockaddr_in serverInfo;
	bool isConnected = false;

public:
	void StartClient();
	bool ConnectToServer();
	void ReceiveCommands();
};

void Client::StartClient() {
 	serverInfo.sin_family = AF_INET;
	serverInfo.sin_port = htons(3455);
	serverInfo.sin_addr.s_addr = inet_addr("127.0.0.1");

	printf("\nInitialising Winsock...");
	if (WSAStartup(MAKEWORD(2,2),&wsa) != 0) {
		printf("Failed. Error Code : %d",WSAGetLastError());
		return;
	} printf("Initialised.\n");

	if ((clientSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		perror("Failed to open socket.");
		return;
	} 

	printf("Socket opened. Connecting to server...\n");
	if (ConnectToServer()) {
		thread obj([this](){this->ReceiveCommands();});
		obj.join();
	}
}

bool Client::ConnectToServer() {
	while (!isConnected) {
		if (connect(clientSock, (struct sockaddr*) &serverInfo, sizeof(serverInfo)) == 0) {
			isConnected = true;
			printf("[+] Connected to server.\n");
			return true;
		}
	}
	return false;
}

void Client::ReceiveCommands() {
   while (isConnected) {
	  char buffer[1024];
	  int bytesReceived = recv(clientSock, buffer, sizeof(buffer), 0);
	  
	  if (bytesReceived < 0 && !isConnected) {
	    perror("Error receiving data from Server");
    	continue;
	  } else if (bytesReceived == 0 && isConnected) {
 			isConnected = false;
    	cout << "Disconnected from the server." << endl;
    	continue;
   } else {
    	buffer[bytesReceived] = '\0';
    	cout << "Received from Server: " << buffer << endl;
	   }   	
   }
}
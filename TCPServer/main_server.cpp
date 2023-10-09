#pragma once

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// First, make it work (messy), then organize

#define DEFAULT_PORT "8412"

std::vector<SOCKET> gClientList;

int main(int arg, char** argv)
{
	// Initialize WinSock
	WSADATA wsaData;
	int result;

	// Set version 2.2 with MAKEWORD(2,2)
	result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		printf("WSAStartup failed with error %d\n", result);
		return 1;
	}
	printf("WSAStartup successfully!\n");

	struct addrinfo* info = nullptr;
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));	// ensure we don't have garbage data 
	hints.ai_family = AF_INET;			// IPv4
	hints.ai_socktype = SOCK_STREAM;	// Stream
	hints.ai_protocol = IPPROTO_TCP;	// TCP
	hints.ai_flags = AI_PASSIVE;


	result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &info);
	if (result != 0) {
		printf("getaddrinfo failed with error %d\n", result);
		WSACleanup();
		return 1;
	}
	printf("getaddrinfo successfully!\n");

	// Socket
	SOCKET listenSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (listenSocket == INVALID_SOCKET) {
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("socket created successfully!\n");

	// Bind 
	result = bind(listenSocket, info->ai_addr, (int)info->ai_addrlen);
	if (result == SOCKET_ERROR) {
		printf("bind failed with error %d\n", WSAGetLastError());
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("bind was successful!\n");

	// Listen
	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR) {
		printf("listen failed with error %d\n", WSAGetLastError());
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("listen successful\n");

	// Accept (First blocking call)
	SOCKET newClientSocket = accept(listenSocket, NULL, NULL);
	if (newClientSocket == INVALID_SOCKET) {
		printf("Accept failed with error %d\n", WSAGetLastError());
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("Client connected on Socket: %d\n", newClientSocket);

	while (true)
	{
		// Read
		char buffer[512];
		result = recv(newClientSocket, buffer, 512, 0);
		if (result == SOCKET_ERROR) {
			printf("recv failed with error %d\n", WSAGetLastError());
			closesocket(listenSocket);
			freeaddrinfo(info);
			WSACleanup();
			return 1;
		}

		printf("Client sent: %s\n", buffer);

		// Write
		result = send(newClientSocket, buffer, 512, 0);
		if (result == SOCKET_ERROR) {
			printf("send failed with error %d\n", WSAGetLastError());
			closesocket(listenSocket);
			freeaddrinfo(info);
			WSACleanup();
			return 1;
		}
	}
	// Close
	freeaddrinfo(info);
	closesocket(listenSocket);

	// TODO Close connection for each client socket
	//closesocket(clientSocket);
	WSACleanup();

	return 0;
}
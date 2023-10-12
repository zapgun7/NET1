#pragma once

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>

#include "buffer.h"

struct PacketHeader
{
	uint32_t packetSize;
	uint32_t messageType;
};

struct ChatMessage
{
	PacketHeader header;
	uint32_t messageLength;
	std::string message;
};

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// First, make it work (messy), then organize

#define DEFAULT_PORT "8412"

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


	result = getaddrinfo("127.0.0.1", DEFAULT_PORT, &hints, &info);
	if (result != 0) {
		printf("getaddrinfo failed with error %d\n", result);
		WSACleanup();
		return 1;
	}
	printf("getaddrinfo successfully!\n");

	// Socket
	SOCKET serverSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (serverSocket == INVALID_SOCKET) {
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("socket created successfully!\n");

	// Connect
	result = connect(serverSocket, info->ai_addr, (int)info->ai_addrlen);
	if (serverSocket == INVALID_SOCKET) {
		printf("connect failed with error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("Connect to the server successfully!\n");

// 	ChatMessage msg;
// 	msg.message = "hello";
// 	msg.messageLength = msg.message.length();
// 	msg.header.messageType = 1;// Can use an enum to determine this
// 	msg.header.packetSize =
// 		msg.message.length()				// 5 'hello' has 5 bytes in it
// 		+ sizeof(msg.messageLength)			// 4, uint32_t is 4 bytes
// 		+ sizeof(msg.header.messageType)	// 4, uint32_t is 4 bytes
// 		+ sizeof(msg.header.packetSize);	// 4, uint32_t is 4 bytes
// 	// 5 + 4 + 4 + 4 = 17
// 
// 	const int bufSize = 512;
// 	Buffer buffer(bufSize);
// 
// 	// Write our packet to the buffer
// 	buffer.WriteUInt32LE(msg.header.packetSize);	// should be 17
// 	buffer.WriteUInt32LE(msg.header.messageType);	// 1
// 	buffer.WriteUInt32LE(msg.messageLength);		// 5
// 	buffer.WriteString(msg.message);				// hello

// 	for (int i = 0; i < 5; i++)
// 	{
// 		// Write
// 		result = send(serverSocket, (const char*)(&buffer.m_BufferData[0]), msg.header.packetSize, 0);
// 		if (result == SOCKET_ERROR) {
// 			printf("send failed with error %d\n", WSAGetLastError());
// 			closesocket(serverSocket);
// 			freeaddrinfo(info);
// 			WSACleanup();
// 			return 1;
// 		}
// 		// Write
// 		result = send(serverSocket, (const char*)(&buffer.m_BufferData[0]), msg.header.packetSize, 0);
// 		if (result == SOCKET_ERROR) {
// 			printf("send failed with error %d\n", WSAGetLastError());
// 			closesocket(serverSocket);
// 			freeaddrinfo(info);
// 			WSACleanup();
// 			return 1;
// 		}
// 		// Write
// 		result = send(serverSocket, (const char*)(&buffer.m_BufferData[0]), msg.header.packetSize, 0);
// 		if (result == SOCKET_ERROR) {
// 			printf("send failed with error %d\n", WSAGetLastError());
// 			closesocket(serverSocket);
// 			freeaddrinfo(info);
// 			WSACleanup();
// 			return 1;
// 		}
// 
// 		//// Read
// 		//result = recv(serverSocket, buffer, 512, 0);
// 		//if (result == SOCKET_ERROR) {
// 		//	printf("recv failed with error %d\n", WSAGetLastError());
// 		//	closesocket(serverSocket);
// 		//	freeaddrinfo(info);
// 		//	WSACleanup();
// 		//	return 1;
// 		//}
// 
// 		system("Pause"); // Force the user to press enter to continue;
// 
// 		//printf("Server sent: %s\n", buffer);
// 	}
	while (true) // The main loop
	{
		// set up select stuff like on the server, though can prob just set it up for the one server socket
		// Add non-blocking input for the user, so chat messages can be updated while the user types up a message
	}


	// Close
	freeaddrinfo(info);
	closesocket(serverSocket);
	WSACleanup();

	return 0;
}
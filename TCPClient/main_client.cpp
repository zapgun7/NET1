#pragma once

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>
#include <thread> // Need it for non-blocking input
#include <iostream>
#include <sstream>

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

// Function signatures
Buffer buildBuffer(ChatMessage msg);
int sendMessage(SOCKET socket, ChatMessage msg);


void userInput(std::string input)
{
	std::string temp;
	std::cin >> temp;
	input = temp;
	return;
}


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
	printf("Connected to the server successfully!\n");

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

	// Set to compare to the one and only Server Socket
	FD_SET socketsReadyForReading;
	FD_ZERO(&socketsReadyForReading);

	// Use a timeval to prevent select from waiting forever.
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;


	//std::string* usrMsg = new std::string("");
	//std::thread th1(userInput, *usrMsg);


	while (true) // The main loop
	{
		// set up select stuff like on the server, though can prob just set it up for the one server socket
		// Add non-blocking input for the user, so chat messages can be updated while the user types up a message





		// USER INPUT AREA - VERY BLOCKING
		std::string usrMsg = "";
		std::cin >> usrMsg; // Should find a way to make this non-blocking

		if (usrMsg.length() > 0) // If user typed SOMETHING at all
		{
			ChatMessage msgToSend;
			msgToSend.header.messageType = 1; // Message type 1 to client is something to output into their chat window
			std::string finalMessage = usrMsg;


			if ((const char*)usrMsg[0] == "!") // If a command: this modifies the message and message type
			{
				std::vector<std::string> tokens;
				std::stringstream check(usrMsg);
				std::string intermediate;

				finalMessage = "";

				getline(check, intermediate, ' ');
				if (intermediate == "!join")          // Join room 
				{
					msgToSend.header.messageType = 2;
					while (getline(check, intermediate, ' ')) // Copy rest of message into new variable (command type is set in the message type)
					{
						finalMessage += intermediate + " ";																			    //!!!!!!!!!!!!!!!!!!!! Can we getline with '' rather than ' '?     Test later
					}
				}
				else if (intermediate == "!leave")    // Leave room
				{
					msgToSend.header.messageType = 3;
					while (getline(check, intermediate, ' '))
					{
						finalMessage += intermediate + " ";
					}
				}
				else if (intermediate == "!nick")     // Change username
				{
					msgToSend.header.messageType = 4;
					while (getline(check, intermediate, ' '))
					{
						finalMessage += intermediate + " ";
					}
				}
			}


			msgToSend.message = finalMessage;

			msgToSend.messageLength = msgToSend.message.length();
			msgToSend.header.packetSize = 10 + msgToSend.messageLength;
			sendMessage(serverSocket, msgToSend);
		}








		// Reset the socketsReadyForReading
		FD_ZERO(&socketsReadyForReading);

		// Add server socket to set to check for new messages
		FD_SET(serverSocket, &socketsReadyForReading);


		int count = select(0, &socketsReadyForReading, NULL, NULL, &tv);
		if (count == 0)
		{
			// Timevalue expired
			continue;
		}
		if (count == SOCKET_ERROR)
		{
			// Handle an error
			printf("select had an error %d\n", WSAGetLastError());
			continue;
		}

		// Here, assuming our one socket (server) is ready for reading

		const int bufSize = 512;
		Buffer buffer(bufSize);

		int result = recv(serverSocket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);

		if (result == 0)
		{
			// Server has shut down lmao
		}

		// If an error, check if it just a buffer space issue
		if ((result == SOCKET_ERROR) && (WSAGetLastError() == WSAEMSGSIZE))                           // Can optimize a little by saving the space if we need more than 512, but haven't received all of the message
		{
			Buffer buffer(buffer.ReadUInt32BE()); // Initialize new buffer with exactly enough room
			result = recv(serverSocket, (char*)(&buffer.m_BufferData[0]), bufSize, 0); // Then receive again!
		}
		// If not a space issue
		if (result == SOCKET_ERROR) {
			printf("recv failed with error %d\n", WSAGetLastError());
			closesocket(serverSocket);
			freeaddrinfo(info);
			WSACleanup();
			break;
		}
		printf("Received %d bytes from the server!\n", result);

		// We must receive 4 bytes before we know how long the packet actually is
		// We must receive the entire packet before we can handle the message.
		// Our protocol says we have a HEADER[pktsize, messagetype];
		uint32_t packetSize = buffer.ReadUInt32BE();

		if (buffer.m_BufferData.size() >= packetSize)
		{
			// We can finally handle our message
		}
		else
		{
			// Cannot handle message yet, go to next for iteration
			continue;
		}
		uint16_t messageType = buffer.ReadUInt16BE(); // Won't need much space to hold message type


		if (messageType == 1) // A message to put into the client's chat window
		{
			uint32_t messageLength = buffer.ReadUInt32LE();
			std::string msg = buffer.ReadString(messageLength);

			printf("%s\n", msg.c_str());
		}

	}

	// Close
	freeaddrinfo(info);
	closesocket(serverSocket); // This sends a message to the server announcing user's disconnect
	WSACleanup();

	return 0;
}



int sendMessage(SOCKET socket, ChatMessage msg) // For sending from the broadcast, cuts buffer creation down
{
	Buffer buffer = buildBuffer(msg);
	return send(socket, (const char*)(&buffer.m_BufferData[0]), msg.header.packetSize, 0);
}

Buffer buildBuffer(ChatMessage msg)
{
	Buffer buffer(msg.header.packetSize);
	buffer.WriteUInt32BE(msg.header.packetSize);
	buffer.WriteUInt16BE(msg.header.messageType);
	buffer.WriteUInt32LE(msg.messageLength);
	buffer.WriteString(msg.message);

	return buffer;
}
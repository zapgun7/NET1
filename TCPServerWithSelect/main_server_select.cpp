#pragma once

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>

#include <vector>
#include <string>
#include <map>
#include "buffer.h"
#include "cUserRoomInfoHandler.h"

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// First, make it work (messy), then organize

#define DEFAULT_PORT "8412"

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

// Function Signatures
int broadcast(std::vector<SOCKET> sockets, ChatMessage msg);
int sendMessage(SOCKET socket, ChatMessage msg);
int sendMessage(SOCKET socket, Buffer buffer, int size);
Buffer buildBuffer(ChatMessage msg);


std::vector<SOCKET> gClientList;

int main(int arg, char** argv)
{
	// Initialize WinSock
	WSADATA wsaData;
	int result;

	// Set version 2.2 with MAKEWORD(2,2)
	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsastartup
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

	// https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-getaddrinfo
	result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &info);
	if (result != 0) {
		printf("getaddrinfo failed with error %d\n", result);
		WSACleanup();
		return 1;
	}
	printf("getaddrinfo successfully!\n");

	// Socket
	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket
	SOCKET listenSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
	if (listenSocket == INVALID_SOCKET) {
		printf("socket failed with error %d\n", WSAGetLastError());
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("socket created successfully!\n");

	// Bind
	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-bind
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
	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-listen
	result = listen(listenSocket, SOMAXCONN);
	if (result == SOCKET_ERROR) {
		printf("listen failed with error %d\n", WSAGetLastError());
		closesocket(listenSocket);
		freeaddrinfo(info);
		WSACleanup();
		return 1;
	}
	printf("listen successful\n"); /// ////////////////////////////////////////////////////////////listening.....


	std::vector<SOCKET> activeConnections;
	cUserRoomInfoHandler sessionInfo; // Class variable to keep track of all room and user related information

	FD_SET activeSockets;				// List of all of the clients ready to read.
	FD_SET socketsReadyForReading;		// List of all of the connections

	FD_ZERO(&activeSockets);			// Initialize the sets
	FD_ZERO(&socketsReadyForReading);

	// Use a timeval to prevent select from waiting forever.
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	while (true)
	{
		// Reset the socketsReadyForReading
		FD_ZERO(&socketsReadyForReading);
		
		// Add our listenSocket to our set to check for new connections
		// This will remain set if there is a "connect" call from a 
		// client to our server.
		FD_SET(listenSocket, &socketsReadyForReading);

		// Add all of our active connections to our socketsReadyForReading
		// set.
		for (int i = 0; i < activeConnections.size(); i++)
		{
			FD_SET(activeConnections[i], &socketsReadyForReading);
		}

		// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-select
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
			//continue;
		}

		// Loop through socketsReadyForReading
		//   recv
		for (int i = 0; i < activeConnections.size(); i++)
		{
			SOCKET socket = activeConnections[i];
			if (FD_ISSET(socket, &socketsReadyForReading))
			{
				// Handle receiving data with a recv call
				//char buffer[bufSize];
				const int bufSize = 512;
				Buffer buffer(bufSize);

				// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-recv
				// result 
				//		-1 : SOCKET_ERROR (More info received from WSAGetLastError() after)
				//		0 : client disconnected
				//		>0: The number of bytes received.
				int result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0);

				//------------------------------------------------CLIENT DISCONNECT-----------------------------------------------------//
				// Client has disconnected, need to purge their socket from the system
				if (result == 0)
				{
					for (unsigned int connIndex = 0; connIndex < activeConnections.size(); connIndex++)
					{
						if (activeConnections[connIndex] == socket)
						{
							activeConnections.erase(activeConnections.begin() + connIndex); // remove from active connections     
						}
					}
					std::string username = sessionInfo.getUsername(socket);          // Get user info before we wipe it
					std::vector<int> userRooms = sessionInfo.getUserRooms(socket);   // So we can send out room leaving notifications
					sessionInfo.removeFromRoom(socket, -1); // Purging user from system                                

					// Broadcasting to all rooms the user was in
					ChatMessage msgToBroadcast;
					msgToBroadcast.header.messageType = 2;
					msgToBroadcast.message = "~SYSTEM~ user [";
					msgToBroadcast.message += username;  
					msgToBroadcast.message += "] has left the room.";
					msgToBroadcast.messageLength = msgToBroadcast.message.length();
					msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

					for (unsigned int roomIndex = 0; roomIndex < userRooms.size(); roomIndex++)
					{
						result = broadcast(sessionInfo.getRoomUsers(userRooms[roomIndex]), msgToBroadcast);
					}

					continue; // Skip rest since the socket has been removed
				}
				//-------------------------------------------------------------------------------------------------------------//

				// If an error, check if it just a buffer space issue
				if ((result == SOCKET_ERROR) && (WSAGetLastError() == WSAEMSGSIZE))                           // Can optimize a little by saving the space if we need more than 512, but haven't received all of the message
				{
					Buffer buffer(buffer.ReadUInt32BE()); // Initialize new buffer with exactly enough room
					result = recv(socket, (char*)(&buffer.m_BufferData[0]), bufSize, 0); // Then receive again!
				}

				if (result == SOCKET_ERROR) {
					printf("recv failed with error %d\n", WSAGetLastError());
					closesocket(listenSocket);
					freeaddrinfo(info);
					WSACleanup();
					break;
				}

				printf("Received %d bytes from the client!\n", result);

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

				if (messageType == 1) // Chat message
				{
					std::cout << "Basic chat message" << std::endl;
					// We know this is a ChatMessage
					uint32_t messageLength = buffer.ReadUInt32LE();
					std::string msg = buffer.ReadString(messageLength);


					ChatMessage msgToBroadcast;
					//msgToBroadcast.header.packetSize = messageLength;
					msgToBroadcast.header.messageType = 1;
					msgToBroadcast.message = "[";
					msgToBroadcast.message += sessionInfo.getUsername(socket);     // Add username sending the message to the start of the message
					msgToBroadcast.message += "] ";
					msgToBroadcast.message += msg;                               
					msgToBroadcast.messageLength = msgToBroadcast.message.length();
					msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

					// Must braoadcast to all rooms the user is in
					std::vector<int> rooms = sessionInfo.getUserRooms(socket);
					
					for (unsigned int roomIndex = 0; roomIndex < rooms.size(); roomIndex++) // Iterate through all rooms the user sending the message is in
					{
						 result = broadcast(sessionInfo.getRoomUsers(rooms[roomIndex]), msgToBroadcast); // Get each rooms' vector of users to broadcast to
					}
					// Must use .c_str() if printing from a std::string, to return as a 'const char*'
// 					printf("PacketSize:%d\nMessageType:%d\nMessageLength:%d\nMessage:%s\n", 
// 						packetSize, messageType, messageLength, msg.c_str());
				}
				else if (messageType == 2) // Room join request
				{
					std::cout << "Adding user to room" << std::endl;
					uint32_t room = buffer.ReadUInt32LE();
					sessionInfo.addToRoom(socket, room); // Add user to the room first

					ChatMessage msgToBroadcast;
					msgToBroadcast.header.messageType = 1;
					msgToBroadcast.message = "~SYSTEM~ user [";
					msgToBroadcast.message += sessionInfo.getUsername(socket);     // Add username sending the message to the start of the message
					msgToBroadcast.message += "] has joined the room.";
					msgToBroadcast.messageLength = msgToBroadcast.message.length();
					msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

					result = broadcast(sessionInfo.getRoomUsers(room), msgToBroadcast);
				}
				else if (messageType == 3) // Room leave request
				{
					std::cout << "Removing user from room" << std::endl;
					uint32_t room = buffer.ReadUInt32LE();
					sessionInfo.removeFromRoom(socket, room);
					

					ChatMessage msgToBroadcast;
					msgToBroadcast.header.messageType = 1;
					msgToBroadcast.message = "~SYSTEM~ user [";
					msgToBroadcast.message += sessionInfo.getUsername(socket);     // Add username sending the message to the start of the message
					msgToBroadcast.message += "] has left the room.";
					msgToBroadcast.messageLength = msgToBroadcast.message.length();
					msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

					result = broadcast(sessionInfo.getRoomUsers(room), msgToBroadcast);
					
				}
				else if (messageType == 4) // Username change request
				{
					std::cout << "changing username" << std::endl;
					// Should we broadcast that the user has changed their name?
					uint32_t usernameLength = buffer.ReadUInt32LE(); // Get username length
					std::string newName = buffer.ReadString(usernameLength);
					std::string oldName = sessionInfo.getUsername(socket);
					sessionInfo.setUsername(socket, newName);


					ChatMessage msgToBroadcast;
					msgToBroadcast.header.messageType = 1;
					msgToBroadcast.message = "~SYSTEM~ user [";
					msgToBroadcast.message += oldName;     // Add username sending the message to the start of the message
					msgToBroadcast.message += "] has changed their name to [";
					msgToBroadcast.message += newName;
					msgToBroadcast.message += "].";
					msgToBroadcast.messageLength = msgToBroadcast.message.length();
					msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

					std::vector<int> rooms = sessionInfo.getUserRooms(socket);

					for (unsigned int roomIndex = 0; roomIndex < rooms.size(); roomIndex++) // Iterate through all rooms the user is in to notify name change
					{
						result = broadcast(sessionInfo.getRoomUsers(rooms[roomIndex]), msgToBroadcast); // Get each rooms' vector of users to broadcast to
					}
				}
				if (result == SOCKET_ERROR) // Check for errors from any part of the message type handling above
				{
					printf("send failed with error %d\n", WSAGetLastError());
					closesocket(listenSocket);
					freeaddrinfo(info);
					WSACleanup();
					break;
				}

				//// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-send
				//result = send(socket, buffer, 512, 0);
				//if (result == SOCKET_ERROR) {
				//	printf("send failed with error %d\n", WSAGetLastError());
				//	closesocket(listenSocket);
				//	freeaddrinfo(info);
				//	WSACleanup();
				//	break;
				//}

				FD_CLR(socket, &socketsReadyForReading);
				count--;
			}
		}

		// Handle any new connections, if count is not 0 then we have 
		// a socketReadyForReading that is not an active connection,
		// which means we have a 'connect' request from a client.
		//   accept
		if (count > 0)
		{
			if (FD_ISSET(listenSocket, &socketsReadyForReading))
			{
				printf("About to accept a new connection\n");
				// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-accept
				SOCKET newConnection = accept(listenSocket, NULL, NULL);
				printf("Accepted a new connection\n");
				if (newConnection == INVALID_SOCKET)
				{
					// Handle errors
					printf("accept failed with error: %d\n", WSAGetLastError());
				}
				else
				{
					//// io = input, output, ctl = control
					//// input output control socket
					//// https://learn.microsoft.com/en-us/windows/win32/api/winsock/nf-winsock-ioctlsocket
					////-------------------------
					//// Set the socket I/O mode: In this case FIONBIO
					//// enables or disables the blocking mode for the 
					//// socket based on the numerical value of iMode.
					//// If iMode = 0, blocking is enabled; 
					//// If iMode != 0, non-blocking mode is enabled.
					// unsigned long NonBlock = 1;
					// ioctlsocket(newConnection, FIONBIO, &NonBlock);

					// Handle successful connection
					activeConnections.push_back(newConnection);
					sessionInfo.initializeUser(newConnection); // Add new user to the "database"                                                 !!! must prompt the user to enter their username
					FD_SET(newConnection, &activeSockets);
					FD_CLR(listenSocket, &socketsReadyForReading);


					printf("Client connected with Socket: %d\n", (int)newConnection);


					ChatMessage msgToSend;
					msgToSend.header.messageType = 1; // Message type 1 to client is something to output into their chat window
					msgToSend.message = "~SYSTEM~ Please set a username with !nick";

					msgToSend.messageLength = msgToSend.message.length();
					msgToSend.header.packetSize = 10 + msgToSend.messageLength;
					sendMessage(newConnection, msgToSend);
				}
			}
		}
	}

	// Cleanup
	// https://learn.microsoft.com/en-us/windows/win32/api/ws2tcpip/nf-ws2tcpip-freeaddrinfo
	freeaddrinfo(info);

	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-closesocket
	closesocket(listenSocket);

	// TODO Close connection for each client socket
	// closesocket(clientSocket);
	// https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsacleanup
	WSACleanup();

	return 0;
}

int broadcast(std::vector<SOCKET> sockets, ChatMessage msg) // Send to a bunch of rooms; just calls sendMessage a bunch
{
	int errorInt = 0;
	int tempInt;
	Buffer buffer = buildBuffer(msg);
// 	buffer.WriteUInt32BE(msg.header.packetSize);
// 	buffer.WriteUInt16BE(msg.header.messageType);
// 	buffer.WriteUInt32LE(msg.messageLength);
// 	buffer.WriteString(msg.message);


	for (unsigned int i = 0; i < sockets.size(); i++)
	{
		tempInt = sendMessage(sockets[i], buffer, msg.header.packetSize);
		 errorInt = errorInt > tempInt ? tempInt:errorInt;
	}
	return errorInt;
}
int sendMessage(SOCKET socket, ChatMessage msg) // Send to one user
{                                                // ChatMessage will be fully filled out
	Buffer buffer = buildBuffer(msg);
// 	buffer.WriteUInt32BE(msg.header.packetSize);
// 	buffer.WriteUInt16BE(msg.header.messageType);
// 	buffer.WriteUInt32LE(msg.messageLength);
// 	buffer.WriteString(msg.message);

	return sendMessage(socket, buffer, msg.header.packetSize);
}
int sendMessage(SOCKET socket, Buffer buffer, int size) // For sending from the broadcast, cuts buffer creation down
{
	return send(socket, (const char*)(&buffer.m_BufferData[0]), size, 0);
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
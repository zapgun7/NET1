#pragma once

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <time.h>

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
	srand(time(NULL)); // Make our rand better (randomizes welcome mat)
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

	std::string finalMessage = ""; // For parsing user commands
	std::string const systemSignature = "{System"; // Don't close, leaves us with ability to put room number in there... or not!
	std::string const welcome = "Welcome to my little chat program :)  Start your session by setting your nickname with !nick and use !help to view all commands. Enjoy!\n\n\n\n";//
	std::string const chattingNoRoom = "You are not in a room. To chat, please join a room first with !join or use !help to look at possible commands"; // When a user sends a chat message while roomless
	std::string const chattingNoRoomNorNick = "Please set a nickname with !nick and then join a room with !join to start chatting."; // When a user tries to chat with unset nick and roomless
	std::string const nickCommNoNick = "To use !nick put a space after the command, followed by your desired username!"; // When a user just calls !nick
	std::string commands =  "///////////////////////////////////////////////////////////////////////////////////////////////////\n";
	commands +=             "/////  !help           Displays all available commands and their syntax                       /////\n";
	commands +=				"/////  !nick name      Sets the user's nickname as name; must set this to join rooms          /////\n";
	commands +=				"/////  !join room#     Attempts to join the specified room number (rooms can only be numbers) /////\n";
	commands +=				"/////                  If the room doesn't exist, it will create one with the provided number /////\n";
	commands +=				"/////  !leave room#    Leaves the specified room, room#, if the user is in it                 /////\n";
	commands +=				"/////  !dc             Disconnects and terminates the user, effectively closing the client    /////\n";
	commands +=				"///////////////////////////////////////////////////////////////////////////////////////////////////\n";

	//////////////////////////////////////// WELCOME MAT GENERATION //////////////////////////////////////////////////

	std::ifstream theWelcomMatsFile("welcomemats.txt");

	if (!theWelcomMatsFile.is_open())
	{
		// didn't open :(
		std::cout << theWelcomMatsFile.is_open();
	}
	std::string line = "";
	std::vector<std::string> welcomeMats;
	int numOfMats;
	std::getline(theWelcomMatsFile, line);

	numOfMats = std::stoi(line); // Get number of mats to initialize
	for (int i = 0; i < numOfMats; i++)
	{
		welcomeMats.push_back(""); // Initialize enough space
	}


	int indexTracker = 0;
	bool prevLineVacancy = false;

	while (std::getline(theWelcomMatsFile, line))
	{
		if ((line.length() > 0) && (prevLineVacancy))
		{
			indexTracker++;
			prevLineVacancy = false;
		}
		if (line.length() == 0)
		{
			prevLineVacancy = true;
			continue;
		}

		welcomeMats[indexTracker] += line + "\n";
	}

	theWelcomMatsFile.close();
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////



	bool trigger = true;
	int debugIndex = 0;

	while (true)
	{
// 		if ((activeConnections.size() > 0) && (debugIndex < numOfMats) && (trigger)) // Welcome mat debugging area
// 		{
// 			SOCKET socket = activeConnections[0];
// 			ChatMessage messageToSend;
// 			messageToSend.header.messageType = 1;
// 			messageToSend.message = welcomeMats[3]/*welcomeMats[debugIndex++]*/; // Inform them on correct usage of !nick
// 			messageToSend.messageLength = messageToSend.message.length();
// 			messageToSend.header.packetSize = 10 + messageToSend.messageLength;
// 			result = sendMessage(socket, messageToSend);
// 			if (result == SOCKET_ERROR) // Check for errors from any part of the message type handling above
// 			{
// 				printf("send failed with error %d\n", WSAGetLastError());
// 				closesocket(listenSocket);
// 				freeaddrinfo(info);
// 				WSACleanup();
// 				break;
// 			}
// 			trigger = false;
// 			//continue;
// 		}

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
				// Client has disconnected (gracefully or otherwise), need to purge their socket from the system
				if ((result == 0) || (result == -1 && (WSAGetLastError() == 10054)))
				{
					std::string msgBase;
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
					msgToBroadcast.header.messageType = 1;
					msgBase = "user [";
					msgBase += username;
					msgBase += "] has left the room.";
// 					msgToBroadcast.messageLength = msgToBroadcast.message.length();
// 					msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

					for (unsigned int roomIndex = 0; roomIndex < userRooms.size(); roomIndex++)
					{
						msgToBroadcast.message = systemSignature + "-";// Start system signature
						msgToBroadcast.message += std::to_string(userRooms[roomIndex]) + "} "; // End system signature
						msgToBroadcast.message += msgBase;
						msgToBroadcast.messageLength = msgToBroadcast.message.length();
						msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;
						result = broadcast(sessionInfo.getRoomUsers(userRooms[roomIndex]), msgToBroadcast);
					}

					continue; // Skip rest since the socket has been removed
				}
				//-------------------------------------------------------------------------------------------------------------//

				// If an error, check if it just a buffer space issue
				if ((result == SOCKET_ERROR) && (WSAGetLastError() == WSAEMSGSIZE))// This error doesn't happen, so we manually compare buffer size after recv and the packetSize of the message                           // Can optimize a little by saving the space if we need more than 512, but haven't received all of the message
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
				if (packetSize > buffer.m_BufferData.size()) // If buffer isn't big enough to fit packet
				{
					buffer.reSize(packetSize); // Initialize new buffer with exactly enough room
					result = recv(socket, (char*)(&buffer.m_BufferData[512]), bufSize, 0); // Then receive the rest
				}
				

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
					std::string msgBase;

					std::vector<int> rooms = sessionInfo.getUserRooms(socket);


					ChatMessage msgToBroadcast;
					//msgToBroadcast.header.packetSize = messageLength;
					msgToBroadcast.header.messageType = 1;
					msgBase += "[";
					msgBase += sessionInfo.getUsername(socket);     // Add username sending the message to the start of the message
					msgBase += "] ";
					msgBase += msg;
					//msgToBroadcast.messageLength = msgToBroadcast.message.length();
					//msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

					// Must broadcast to all rooms the user is in
					for (unsigned int roomIndex = 0; roomIndex < rooms.size(); roomIndex++) // Iterate through all rooms the user sending the message is in
					{
						msgToBroadcast.message = "-";
						msgToBroadcast.message += std::to_string(rooms[roomIndex]);
						msgToBroadcast.message += "- ";
						msgToBroadcast.message += msgBase;
						msgToBroadcast.messageLength = msgToBroadcast.message.length();
						msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;
						result = broadcast(sessionInfo.getRoomUsers(rooms[roomIndex]), msgToBroadcast); // Get each rooms' vector of users to broadcast to
					}
					if (sessionInfo.getUsername(socket).length() == 0) // If the user hasn't set a nickname yet
					{
						msgToBroadcast.message = systemSignature + "} " + chattingNoRoomNorNick;
						msgToBroadcast.messageLength = msgToBroadcast.message.length();
						msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;
						result = sendMessage(socket, msgToBroadcast);
					}
					else if (rooms.size() == 0) // If user isn't in any rooms, let them know
					{
						msgToBroadcast.message = systemSignature + "} " + chattingNoRoom;
						msgToBroadcast.messageLength = msgToBroadcast.message.length();
						msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;
						result = sendMessage(socket, msgToBroadcast);
					}
				}
				else if (messageType == 2) // User calls a command
				{
					uint32_t messageLength = buffer.ReadUInt32LE();
					std::string msg = buffer.ReadString(messageLength);


					std::vector<std::string> tokens;
					std::stringstream check(msg);
					std::string intermediate;

					finalMessage = "";

					getline(check, intermediate, ' ');
					if (intermediate == "!join")          // Join room 
					{
						messageType = 2; // Reuse message type for following set of if else's
						(getline(check, intermediate, ' '));
						finalMessage += intermediate;              // Add the first one without the space before it
						while (getline(check, intermediate, ' ')) 
						{
							finalMessage += " " + intermediate;		// Add a space before each subsequent token							    //!!!!!!!!!!!!!!!!!!!! Can we getline with '' rather than ' '?     Test later
						}
					}
					else if (intermediate == "!leave")    // Leave room
					{
						messageType = 3;
						(getline(check, intermediate, ' '));
						finalMessage += intermediate;
						while (getline(check, intermediate, ' '))
						{
							finalMessage += " " + intermediate;
						}
					}
					else if (intermediate == "!nick")     // Change username
					{
						messageType = 4;
						intermediate = "";
						(getline(check, intermediate, ' '));
						finalMessage += intermediate; 
						while (getline(check, intermediate, ' '))
						{
							finalMessage += " " + intermediate;
						}
					}
					else if (intermediate == "!help")    // Requesting commands
					{
						messageType = 50;     // TOMDO (to maybe do)     Depending on what comes after !help, provide specific info on the command (i.e. '!help join' would give extra details on the join command)
					}
					else
					{
						messageType = 51;
					}
					// 					else if (intermediate == "!dc") // Completely disconnect user
					// 					{
					// 						break; // Exits main loop and cleans up socket stuff, while also notifying server of its disconnect 
					// 					}          // allowing it to gracefully purge the user from its system

					if (messageType == 2) // Room join request
					{
						bool isValidInput = true;
						std::cout << "Adding user to room" << std::endl;
						// Make sure finalMessage contains valid int for conversion //
						for (unsigned int c = 0; c < finalMessage.length(); c++)
						{
							if ((finalMessage[c] < 48) || (finalMessage[c] > 57)) // 48-57
							{
								isValidInput = false;
							}
						}


						if (isValidInput)// Proven valid
						{
							if (sessionInfo.getUsername(socket).length() == 0) // If username not set yet
							{
								ChatMessage msgToSend;
								msgToSend.header.messageType = 1;
								msgToSend.message = systemSignature + "} " + "Please set a nickname with !nick before joining a room";
								msgToSend.messageLength = msgToSend.message.length();
								msgToSend.header.packetSize = 10 + msgToSend.messageLength;

								result = sendMessage(socket, msgToSend);
								continue;
							}


							int room = stoi(finalMessage);                                                                          // TODO maybe should check if this is a valid number before conversion
							//sessionInfo.addToRoom(socket, room); // Add user to the room first

							ChatMessage msgToBroadcast;
							msgToBroadcast.header.messageType = 1;
							msgToBroadcast.message = systemSignature + "-"; 
							msgToBroadcast.message += std::to_string(room) + "} "; // End of system signature
							msgToBroadcast.message += "user [";
							msgToBroadcast.message += sessionInfo.getUsername(socket);     // Add username sending the message to the start of the message
							msgToBroadcast.message += "] has joined the room.";
							msgToBroadcast.messageLength = msgToBroadcast.message.length();
							msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;

							result = broadcast(sessionInfo.getRoomUsers(room), msgToBroadcast);
							if (result == SOCKET_ERROR) // Check for errors from any part of the message type handling above
							{
								printf("send failed with error %d\n", WSAGetLastError());
								closesocket(listenSocket);
								freeaddrinfo(info);
								WSACleanup();
								break;
							}


							sessionInfo.addToRoom(socket, room); // Add to room after to only send general joined room messgage to everyone that was in the room before this user joined

							ChatMessage msgToSend;
							msgToSend.header.messageType = 1;
							msgToSend.message = systemSignature + "} " + "You have joined room ";
							msgToSend.message += std::to_string(room);     // Add username sending the message to the start of the message
							msgToSend.message += ".";
							msgToSend.messageLength = msgToSend.message.length();
							msgToSend.header.packetSize = 10 + msgToSend.messageLength;

							result = sendMessage(socket, msgToSend); // Send unique message to just the joining user
						}
						else // Invalid input, notify user
						{
							ChatMessage msgToSend;
							msgToSend.header.messageType = 1;
							msgToSend.message = systemSignature + "} " + "ERROR: Invalid Room | please use only numbers with no spaces :)";
							msgToSend.messageLength = msgToSend.message.length();
							msgToSend.header.packetSize = 10 + msgToSend.messageLength;

							result = sendMessage(socket, msgToSend);
						}
					}
					else if (messageType == 3) // Room leave request
					{
						bool isValidInput = true;
						std::cout << "Removing user from room" << std::endl;

						for (unsigned int c = 0; c < finalMessage.length(); c++)
						{
							if ((finalMessage[c] < 48) || (finalMessage[c] > 57)) // 48-57
							{
								isValidInput = false;
							}
						}

						if (isValidInput)
						{
							//uint32_t room = buffer.ReadUInt32LE();
							int room = stoi(finalMessage);
							if (sessionInfo.removeFromRoom(socket, room)) // If the user was successfully removed from the specified room
							{
								ChatMessage msgToBroadcast;
								msgToBroadcast.header.messageType = 1;
								msgToBroadcast.message = systemSignature + "-";
								msgToBroadcast.message += std::to_string(room) + "} "; // End of system signature
								msgToBroadcast.message += "user [";
								msgToBroadcast.message += sessionInfo.getUsername(socket);     // Add username sending the message to the start of the message
								msgToBroadcast.message += "] has left the room.";
								msgToBroadcast.messageLength = msgToBroadcast.message.length();
								msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;
								result = broadcast(sessionInfo.getRoomUsers(room), msgToBroadcast); // Broad cast to the room about the success
								if (result == SOCKET_ERROR) // Check for errors from any part of the message type handling above
								{
									printf("send failed with error %d\n", WSAGetLastError());
									closesocket(listenSocket);
									freeaddrinfo(info);
									WSACleanup();
									break;
								}

								// Send personal message to user who left room
								ChatMessage msgToSend;
								msgToSend.message = systemSignature + "} " + "You have left room ";
								msgToSend.message += std::to_string(room);     // Add username sending the message to the start of the message
								msgToSend.message += ".";
								msgToSend.messageLength = msgToSend.message.length();
								msgToSend.header.packetSize = 10 + msgToSend.messageLength;
								result = sendMessage(socket, msgToSend);
							}
							else // If the user was not found to be in the room
							{
								ChatMessage msgToSend;
								msgToSend.header.messageType = 1;
								msgToSend.message = systemSignature + "} " + "You are not in this room.";
								//msgToBroadcast.message += sessionInfo.getUsername(socket);     // Add username sending the message to the start of the message
								//msgToBroadcast.message += "] has left the room.";
								msgToSend.messageLength = msgToSend.message.length();
								msgToSend.header.packetSize = 10 + msgToSend.messageLength;
								result = sendMessage(socket, msgToSend);   // Send the failure notification to just the user 
							}
						}
						else // Invalid input, notify user
						{
							ChatMessage msgToSend;
							msgToSend.header.messageType = 1;
							msgToSend.message = systemSignature + "} " + "ERROR: Invalid Room | please use only numbers with no spaces :)";
							msgToSend.messageLength = msgToSend.message.length();
							msgToSend.header.packetSize = 10 + msgToSend.messageLength;

							result = sendMessage(socket, msgToSend);
						}
					}
					else if (messageType == 4) // Username change request
					{
						std::cout << "changing nickname" << std::endl;

						if (finalMessage.length() == 0) // If user only did !nick
						{
							ChatMessage messageToSend;
							messageToSend.header.messageType = 1;
							messageToSend.message = nickCommNoNick; // Inform them on correct usage of !nick
							messageToSend.messageLength = messageToSend.message.length();
							messageToSend.header.packetSize = 10 + messageToSend.messageLength;
							result = sendMessage(socket, messageToSend);
							if (result == SOCKET_ERROR) // Check for errors from any part of the message type handling above
							{
								printf("send failed with error %d\n", WSAGetLastError());
								closesocket(listenSocket);
								freeaddrinfo(info);
								WSACleanup();
								break;
							}
							continue;
						}
						// Should we broadcast that the user has changed their name?
						//uint32_t usernameLength = buffer.ReadUInt32LE(); // Get username length
						//std::string newName = buffer.ReadString(usernameLength);
						std::string newName = finalMessage;
						std::string oldName = sessionInfo.getUsername(socket);
						sessionInfo.setUsername(socket, newName);

						std::vector<int> rooms = sessionInfo.getUserRooms(socket); // Get user's rooms up here to branch off into 2 conditions (in 1+ rooms || in no rooms)
						if (!rooms.empty()) // User is in at least one room
						{
							std::string msgBase;
							ChatMessage msgToBroadcast;
							msgToBroadcast.header.messageType = 1;
							//msgToBroadcast.message = systemSignature + "user [";
							msgBase = "user [ " + oldName;     // Add username sending the message to the start of the message
							msgBase += "] has changed their nickname to [";
							msgBase += newName;
							msgBase += "].";
							//msgToBroadcast.messageLength = msgToBroadcast.message.length();
							//msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;
							for (unsigned int roomIndex = 0; roomIndex < rooms.size(); roomIndex++) // Iterate through all rooms the user is in to notify name change
							{
								msgToBroadcast.message = systemSignature + "-";// Start system signature
								msgToBroadcast.message += std::to_string(rooms[roomIndex]) + "} "; // End system signature
								msgToBroadcast.message += msgBase;
								msgToBroadcast.messageLength = msgToBroadcast.message.length();
								msgToBroadcast.header.packetSize = 10 + msgToBroadcast.messageLength;
								result = broadcast(sessionInfo.getRoomUsers(rooms[roomIndex]), msgToBroadcast); // Get each rooms' vector of users to broadcast to
							}
						}
						else // User is not in any rooms
						{
							ChatMessage msgToSend;
							msgToSend.header.messageType = 1;
							msgToSend.message = systemSignature + "} " + "you are now known as [";
							msgToSend.message += newName + "].";
							msgToSend.messageLength = msgToSend.message.length();
							msgToSend.header.packetSize = 10 + msgToSend.messageLength;

							result = sendMessage(socket, msgToSend);
						}

						

					}
					else if (messageType == 50)   // TODO NOT WORKING, suspect is buffer resize on client's end
					{
						std::cout << "Sending Help\n";
						ChatMessage msgToSend;
						msgToSend.header.messageType = 1;
						msgToSend.message = commands;
						msgToSend.messageLength = msgToSend.message.length();
						msgToSend.header.packetSize = 10 + msgToSend.messageLength;

						result = sendMessage(socket, msgToSend);
					}
					else if (messageType == 51)
					{
						// TODO inform user they have typed in an invalid command, suggest !help for list of commands
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
					sessionInfo.initializeUser(newConnection); // Add new user to the "database" 
					FD_SET(newConnection, &activeSockets);
					FD_CLR(listenSocket, &socketsReadyForReading);


					printf("Client connected with Socket: %d\n", (int)newConnection);

					int randomMat = rand() % numOfMats;
					ChatMessage msgToSend;                                                                                       // INITIAL MESSAGE TO NEW USERS     TODO should we also output just !help or the entirety
					msgToSend.header.messageType = 1; // Message type 1 to client is something to output into their chat window
					msgToSend.message = "\n\n\n\n\n\n\n" + welcomeMats[randomMat] + "\n\n\n\n";
					msgToSend.message += systemSignature + "} " + welcome;

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
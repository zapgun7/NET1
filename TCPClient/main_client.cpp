#pragma once

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <string>
#include <conio.h> // Need it for non-blocking input
#include <iostream>
#include <sstream>
#include <ConsoleApi.h> // Need it for dealing with closing console window with the X button

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

void clearLine(int columns)
{
	std::cout << "\r"; // Go to start of current line
	for (int i = 0; i < columns; i++) // Create enough blank space below where the user is currently typing
		std::cout << " ";
	std::cout << "\r"; // go back to start of line to prepare for input
}

SOCKET serverSocket;
struct addrinfo* info;


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

	/*struct addrinfo* */info = nullptr;
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


	

	// Set to compare to the one and only Server Socket
	FD_SET socketsReadyForReading;
	FD_ZERO(&socketsReadyForReading);

	// Use a timeval to prevent select from waiting forever.
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 100;



	std::string commandstarter = "!";
	std::string userInputBuffer = "";
	bool wantsToSend = false;
	
	CONSOLE_SCREEN_BUFFER_INFO csbi; 
	int columns = 0;
	int prevColumns = 0;
	int rowsDown = 0; // To keep track of how many rows deep we are 
	bool isRowMoveUp = false;
	bool isRowMoveDown = false;

	system(" "); // This enables vt100 codes when launching as an exe
	std::cout << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"; // "Clear" the screen


	while (true) // The main loop
	{
		// set up select stuff like on the server, though can prob just set it up for the one server socket
		// Add non-blocking input for the user, so chat messages can be updated while the user types up a message

		prevColumns = columns; // Update last frames console width

		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
		columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;  // Get the character count before wrapping starts






		// USER INPUT AREA - VERY BLOCKING

		if (_kbhit()) // If the user types something
		{
			int key_hit = _getch();
			if (key_hit == 8 && userInputBuffer.length() > 0) // Backspace, will remove most recent char from the input buffer
			{
				userInputBuffer.erase(userInputBuffer.end() - 1);
			}
			else if ((key_hit == 13) && (userInputBuffer.length() > 0)) // User presses enter and has SOMETHING in the input buffer
			{
				wantsToSend = true;
			}
			else if (key_hit >= 32 && key_hit <= 126) // If within range of valid keys to input to message
			{
				userInputBuffer += char(key_hit);
			}
			prevColumns = columns; // Proc a 'redraw' of the user input
		}




		
		// ////////////////////// START OF FORMATTING ///////////////////////////////////////////////
		// I use VT100 escape codes here to do some magic with console manipulation          \33[2k wipes the line the cursor is on (console cursor)       \33[A traverses up a line     \33[B traverses down a line  
		if ((columns >= userInputBuffer.length()) && (prevColumns == columns)) // No worries (don't need to format input for word wrapping)
		{
			if (rowsDown > 0) // If we're recovering from being 1+ rows down
			{
				for (int i = 0; i < rowsDown; i++)
					std::cout << "\33[2K\33[A"; // Wipe line, then traverse up a row
				rowsDown = 0;
			}

			clearLine(columns);
			std::cout << userInputBuffer;  
		}
		else if (columns < userInputBuffer.length()) // Worries (need to format input)
		{
			if (rowsDown > (userInputBuffer.length() - 1) / columns) // Check if the row moved up
				isRowMoveUp = true;
			else if (rowsDown < (userInputBuffer.length() - 1) / columns) // Or if it moved down
				isRowMoveDown = true;

			rowsDown = (userInputBuffer.length()-1) / columns;


			if (isRowMoveDown)/*(userInputBuffer.length() % columns == 1)*/ // Pushes to the next line before code that assumes that it is 
			{
				//std::cout << "\33[B";
				std::cout << " "; // Should bump it down a line
				//std::cout << userInputBuffer.substr(columns * (userInputBuffer.length() / columns), userInputBuffer.length() - 1);
				isRowMoveDown = false;
			}


			clearLine(columns);

			if (isRowMoveUp) // Move up a line 
			{
				std::cout << " \33[A\33[2k\r"; // Move up a line and clear it before drawing below
				isRowMoveUp = false;
			}
			std::cout << userInputBuffer.substr(columns*rowsDown, userInputBuffer.length() - 1) /*<< "\33[D"*/;

		}





		///////////////////////////////////////// SEND MESSAGE /////////////////////////////////////////////////
		if ((wantsToSend) &&(userInputBuffer.length() > 0)) // If user typed SOMETHING at all and has pressed enter
		{
			ChatMessage msgToSend;
			msgToSend.header.messageType = 1; // Message type 1 to client is something to output into their chat window
			std::string finalMessage = userInputBuffer;


			if (userInputBuffer[0] == commandstarter[0]) // If a command: this modifies the message and message type
			{
				msgToSend.header.messageType = 2; // Let the server know the message is a command (or an attempt at one)
				std::vector<std::string> tokens;
				std::stringstream check(userInputBuffer);
				std::string intermediate;

				getline(check, intermediate, ' '); // Get first token to see if a disconnect command


				if (intermediate == "!dc") // Completely disconnect user
				{
					break; // Exits main loop and cleans up socket stuff, while also notifying server of its disconnect 
				}          // allowing it to gracefully purge the user from its system
			}


			msgToSend.message = userInputBuffer;

			msgToSend.messageLength = msgToSend.message.length();
			msgToSend.header.packetSize = 10 + msgToSend.messageLength;
			sendMessage(serverSocket, msgToSend);

			for (unsigned int l = 0; l < rowsDown; l++) // Clear all user input
			{
				clearLine(columns);
				std::cout << "\33[A"; // Go up a line
			}
			clearLine(columns);
			

			rowsDown = 0;
			userInputBuffer = ""; // Reset the user input buffer
			wantsToSend = false; 
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
		if ((result == SOCKET_ERROR) && (WSAGetLastError() == WSAEMSGSIZE))// Doesn't trigger when buffer isn't big enough???          // Can optimize a little by saving the space if we need more than 512, but haven't received all of the message
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

		// We must receive 4 bytes before we know how long the packet actually is
		// We must 
		//  the entire packet before we can handle the message.
		// Our protocol says we have a HEADER[pktsize, messagetype];


		uint32_t packetSize = buffer.ReadUInt32BE();
		if (packetSize > buffer.m_BufferData.size()) // If buffer isn't big enought to fit packet
		{
			buffer.reSize(packetSize); // Initialize new buffer with exactly enough room
			result = recv(serverSocket, (char*)(&buffer.m_BufferData[512]), bufSize, 0); // Then receive the rest
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


		if (messageType == 1) // A message to put into the client's chat window
		{
			uint32_t messageLength = buffer.ReadUInt32LE();
			std::string msg = buffer.ReadString(messageLength);

			clearLine(columns);
			for (int lines = 0; lines < rowsDown; lines++)
			{
				std::cout << "\33[A"; // Go up a line
				clearLine(columns);	// Clear line
			}

			std::cout << msg << std::endl; // Go to start of line with \r, then write server message
			std::cout << userInputBuffer; // Write user buffer again
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
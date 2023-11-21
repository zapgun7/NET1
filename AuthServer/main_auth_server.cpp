#include "../Shared/authentication.pb.h"

// WinSock2 Windows Sockets
#define WIN32_LEAN_AND_MEAN

#include <string.h>
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
#include "sha256.h"


#include "mysqlutil.h"

// Need to link Ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

// First, make it work (messy), then organize

#define DEFAULT_PORT "8412"


MySQLUtil g_MySQLDB;


enum class StatementType
{
	// CreateAccount sql calls
	CheckExisting = 0, // SELECT where email is the new email
	AddAccount = 1,
	// AuthAccount sql calls
	GetAccount = 2, // SELECT where email is presumed account; would then check if it returned anyhting (if existing account) then hash password attempt and compare  the one in the db
	UpdateUser = 3   // Just update the lastlogin timestamp
};

// Make sure to cleanup (delete these pointers) when done.
std::map<int, sql::PreparedStatement*> g_PreparedStatements;

void UpdateUser(uint64_t id);

// Returns true if a table entry exists with the email
bool CheckExisting(const char* email)
{
	sql::PreparedStatement* pStmt = g_PreparedStatements[(int)StatementType::CheckExisting];
	pStmt->setString(1, email);

	//int count = pStmt->executeUpdate();
	sql::ResultSet* result;
	result = pStmt->executeQuery();
	if (result->next()) // If an entry exists
	{
		delete result;
		return true;
	}
	delete result;
	return false;
}

int AddAccount(const char* email, const char* password) // Calling this assumes all info is fine (long enought password, email doesn't already exist)
{
	// Start by making the user
	//long userID = AddUser();
	g_MySQLDB.Insert("INSERT INTO user (creation_date) VALUES(CURRENT_TIMESTAMP);"); // Set a value, rest is filled in automatically
	sql::ResultSet* idResult = g_MySQLDB.Select("SELECT id FROM user ORDER BY id DESC;"); // Just select the first entry, which should be the just added user

	idResult->next(); // Should put it on the right one?
	uint64_t userID = idResult->getUInt64("id");

	// Must generate a salt and hash the password with said salt, storing both afterwards
	SHA256 hasher;
	std::string newSalt = "";
	for (unsigned int i = 0; i < 10; i++) // Generate a salt (totally safe and how salt is generated in an actual app :)
	{
		int randChar = rand() % 25 + 97;
		newSalt += char(randChar);
	}
	std::string hashedPass = hasher(newSalt + password);

	sql::PreparedStatement* pStmt = g_PreparedStatements[(int)StatementType::AddAccount];
	pStmt->setString(1, email); // email
	pStmt->setString(2, newSalt); // salt
	pStmt->setString(3, hashedPass); // hashed pass
	pStmt->setInt(4, userID); // user id

	int count = pStmt->executeUpdate();

	return userID;
}

// Attempts to authorize the account with the provided credentials
int GetAccount(const char* email, const char* password) // Returns > 0 (uid) if sucess, -1 if invalid pass, -2 if server error
{
	sql::PreparedStatement* pStmt = g_PreparedStatements[(int)StatementType::GetAccount];
	pStmt->setString(1, email); // email

	sql::ResultSet* result;
	result = pStmt->executeQuery();

	result->next(); // I think this places it on the first row

	std::string dbHashedPass = result->getString("hashed_password");
	std::string dbSalt = result->getString("salt");

	SHA256 hasher;

	if (hasher(dbSalt + password) == dbHashedPass)
	{
		// Passwords match! Successful authorization

		uint64_t userID = result->getUInt64("userId");
		UpdateUser(userID); // Update last login time
// 		std::string updateCommand = "UPDATE user SET last_login = CURRENT_TIMESTAMP where id = ";
// 		updateCommand += std::to_string(userID);
// 		updateCommand += ";";
// 
// 		// Update last login time
// 		g_MySQLDB.Update(updateCommand.c_str());


		delete result;
		return userID;
	}
	else
	{
		// Passwords did not match
		delete result;
		return -1;
	}
	return -2; // server error
}

// Just updates the last login time
void UpdateUser(uint64_t id)
{
	sql::PreparedStatement* pStmt = g_PreparedStatements[(int)StatementType::UpdateUser];
	pStmt->setInt(1, id); // userid

	pStmt->executeUpdate();

	return;
}

// Returns the creation date of the user corresponding to the provided uid
std::string GetCreationDate(int uid)
{
	std::string selectCall = "SELECT creation_date FROM user WHERE id = ";
	selectCall += std::to_string(uid) + ";";

	sql::ResultSet* dateResult = g_MySQLDB.Select(selectCall.c_str());
	dateResult->next();

	std::string creatDate = dateResult->getString("creation_date");

	return creatDate;
}

// Generates a new user, returns the auto-incremented id
// long AddUser()
// {
// 	sql::PreparedStatement* pStmt = g_PreparedStatements[(int)StatementType::AddAccount];
// 	pStmt->setDateTime(1, )
// 
// }



struct PacketHeader
{
	uint32_t packetSize;
	uint32_t messageType; // 9 = webCreate success   10 = fail     11 = auth Success   12 = fail
};

struct ChatMessage
{
	PacketHeader header;
	uint32_t messageLength;
	std::string message;
};

int sendMessage(SOCKET socket, ChatMessage msg);
int sendMessage(SOCKET socket, Buffer buffer, int size);
Buffer buildBuffer(ChatMessage msg);


int main(int argc, char** argv)
{
	srand(time(NULL));
	GOOGLE_PROTOBUF_VERIFY_VERSION;

	// Initialize MySQL driver and connect to our database
	g_MySQLDB.ConnectToDatabase("127.0.0.1:3306", "root", "root", "authschema");

	//  Prepare our statements
// 	g_PreparedStatements[(int)StatementType::AddEnemy] = g_MySQLDB.PrepareStatement(
// 		"INSERT INTO enemy (name, health, speed, damage) VALUES (?, ?, ?, ?);");
// 
// 	g_PreparedStatements[(int)StatementType::UpdateEnemy] = g_MySQLDB.PrepareStatement(
// 		"UPDATE enemy SET name = ?, health = ?, speed = ?, damage = ? WHERE id = ?");

	g_PreparedStatements[(int)StatementType::CheckExisting] = g_MySQLDB.PrepareStatement(
		"SELECT email FROM web_auth WHERE email = ?;");

	g_PreparedStatements[(int)StatementType::AddAccount] = g_MySQLDB.PrepareStatement(
		"INSERT INTO web_auth (email, salt, hashed_password, userId) VALUES (?, ?, ?, ?);");

	//g_PreparedStatements[(int)StatementType::AddUser] = g_MySQLDB.PrepareStatement(
	//	"INSERT INTO user (creation_date) VALUES (?);");

	g_PreparedStatements[(int)StatementType::GetAccount] = g_MySQLDB.PrepareStatement(
		"SELECT * FROM web_auth WHERE email = ?;");

	g_PreparedStatements[(int)StatementType::UpdateUser] = g_MySQLDB.PrepareStatement(
		"UPDATE user SET last_login = CURRENT_TIMESTAMP WHERE id = ?");

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
	result = getaddrinfo(NULL, DEFAULT_PORT + 1, &hints, &info);
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
	//cUserRoomInfoHandler sessionInfo; // Class variable to keep track of all room and user related information

	FD_SET activeSockets;				// List of all of the clients ready to read.
	FD_SET socketsReadyForReading;		// List of all of the connections

	FD_ZERO(&activeSockets);			// Initialize the sets
	FD_ZERO(&socketsReadyForReading);

	// Use a timeval to prevent select from waiting forever.
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	//////////////////////////////////// END OF SERVER SETUP //////////////////////////////////////////////////

	// Call INSERT
	//AddEnemy("ogre", 500, 3, 100);
	//UpdateEnemy(6, "dwarf", 500, 3, 100);

	// Call SELECT
	sql::ResultSet* sqlresult = g_MySQLDB.Select("SELECT * FROM web_auth;");

	// return the result set, or a copy of the result set into a structure
	// we can iterate through.
	while (sqlresult->next())
	{
		// Reading each row
		// Get column data
		sql::SQLString email = sqlresult->getString("email");
		int32_t id = sqlresult->getInt("id");

		printf("Email: %s id=%d\n", email.c_str(), id);
	}

	//return 0;




// 
// 	auth::AuthenticateWeb testweb;
// 	testweb.set_email("ihate@effyuu.ca");
// 	testweb.set_plaintextpassword("dumbasspass");
// 
// 	std::string serializedString;
// 	testweb.SerializeToString(&serializedString);
// 
// 	std::cout << serializedString << std::endl;
// 	for (int idxString = 0; idxString < serializedString.length(); idxString++) {
// 		printf("%02X ", serializedString[idxString]);
// 	}
// 
// 
// 	auth::AuthenticateWeb dstestweb;
// 	bool success = dstestweb.ParseFromString(serializedString);
// 	if (!success)
// 		std::cout << "Failed to parse the AuthenticateWeb" << std::endl;
// 	std::cout << "Parsing successful" << std::endl;
// 
// 	std::cout << "Email: " << dstestweb.email() << std::endl;
// 	std::cout << "Password: " << dstestweb.plaintextpassword() << std::endl;


	//////////////////////////////// SERVER LOOP ////////////////////////////////////

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


				/////////////////////////////////////// MESSAGE HANDLING /////////////////////////////////


				// These are the only msg types that will be sent here
				if (messageType == 1) // CreateAccountWeb    user requested to create account
				{
					// Start by deserializing the CreatAccountWeb object
					uint32_t messageLength = buffer.ReadUInt32LE();
					std::string msg = buffer.ReadString(messageLength);

					auth::CreateAccountWeb dsCreate;
					bool success = dsCreate.ParseFromString(msg);
					if (!success)
						std::cout << "Failed to parse the CreateAccountWeb" << std::endl;

					long requestID = dsCreate.requestid(); // To attach to the return message

					// Now we query web_auth to see if an entry with this email exists already
					if (CheckExisting(dsCreate.email().c_str())) // If email exists
					{
						// Construct and return error

						auth::CreateAccountWebFailure createFail;
						std::string serializedFailure;
						createFail.set_requestid(requestID); // Set request id
						createFail.set_type(auth::CreateAccountWebFailure_reason_ACCOUNT_ALREADY_EXISTS);
						//serializedFailure = createFail.SerializeAsString();
						createFail.SerializeToString(&serializedFailure);

						ChatMessage msgToSend;
						msgToSend.message = serializedFailure;
						msgToSend.header.messageType = 10; // Web create failure
						msgToSend.messageLength = msgToSend.message.length();
						msgToSend.header.packetSize = 10 + msgToSend.messageLength;
						result = sendMessage(socket, msgToSend);

						continue;
					}
					else if (dsCreate.plaintextpassword().length() >= 4)// Add new account and return success
					{
						int uid = AddAccount(dsCreate.email().c_str(), dsCreate.plaintextpassword().c_str());
						if (uid >= 0)
						{
							// Returned true, a success!
							auth::CreateAccountWebSuccess createSucc;
							std::string serializedSuccess;
							createSucc.set_requestid(requestID);
							createSucc.set_userid(uid);
							createSucc.SerializeToString(&serializedSuccess);

							ChatMessage msgToSend;
							msgToSend.message = serializedSuccess;
							msgToSend.header.messageType = 9; // Web create success
							msgToSend.messageLength = msgToSend.message.length();
							msgToSend.header.packetSize = 10 + msgToSend.messageLength;
							result = sendMessage(socket, msgToSend);

							continue;
						}
						else // uid = 1-   (error)
						{
							// Probably an internal server error TODO
						}
					}
					else // Bad password
					{
						// Construct and return error

						auth::CreateAccountWebFailure createFail;
						std::string serializedFailure;
						createFail.set_requestid(requestID); // Set request id
						createFail.set_type(auth::CreateAccountWebFailure_reason_INVALID_PASSWORD);
						//serializedFailure = createFail.SerializeAsString();
						createFail.SerializeToString(&serializedFailure);

						ChatMessage msgToSend;
						msgToSend.message = serializedFailure;
						msgToSend.header.messageType = 10; // Web create failure
						msgToSend.messageLength = msgToSend.message.length();
						msgToSend.header.packetSize = 10 + msgToSend.messageLength;
						result = sendMessage(socket, msgToSend);

						continue;
					}
				}
				else if (messageType == 2) // AuthenticateWeb       user requested to login
				{
					// Start by deserializing the CreatAccountWeb object
					uint32_t messageLength = buffer.ReadUInt32LE();
					std::string msg = buffer.ReadString(messageLength);

					auth::AuthenticateWeb dsAuth;
					bool success = dsAuth.ParseFromString(msg);
					if (!success)
						std::cout << "Failed to parse the AuthenticateWeb!" << std::endl;

					long requestID = dsAuth.requestid(); // To attach to the return message

					// Prepare these in case of failure
					auth::AuthenticateWebFailure authFail;
					std::string serializedFail;


					// Now check if the email exists in the first place
					if (CheckExisting(dsAuth.email().c_str())) // If email exists
					{
						// It does exist! Attempt authorizing it

						int authResult = GetAccount(dsAuth.email().c_str(), dsAuth.plaintextpassword().c_str());

						if (authResult >= 0) // Success, this value is the user id
						{
							auth::AuthenticateWebSuccess authSucc;
							std::string serializedSuccess;
							authSucc.set_requestid(requestID);
							authSucc.set_userid(authResult);

							std::string creatDate = GetCreationDate(authResult);
							authSucc.set_creationdate(creatDate);


							authSucc.SerializeToString(&serializedSuccess);

							ChatMessage msgToSend;
							msgToSend.message = serializedSuccess;
							msgToSend.header.messageType = 11; // Web auth success
							msgToSend.messageLength = msgToSend.message.length();
							msgToSend.header.packetSize = 10 + msgToSend.messageLength;
							result = sendMessage(socket, msgToSend);
							std::cout << "Auth success" << std::endl;


							continue;
						}
						else if (authResult == -1) // Wrong password
						{
							authFail.set_requestid(requestID);
							authFail.set_type(auth::AuthenticateWebFailure_reason_INVALID_CREDENTIALS);
							
						}
						else // -2   internal server error
						{
							authFail.set_requestid(requestID);
							authFail.set_type(auth::AuthenticateWebFailure_reason_INTERNAL_SERVER_ERROR);
						}

						// Continuation of 1 of the error paths above
						authFail.SerializeToString(&serializedFail);

						ChatMessage msgToSend;
						msgToSend.message = serializedFail;
						msgToSend.header.messageType = 12; // Web auth fail
						msgToSend.messageLength = msgToSend.message.length();
						msgToSend.header.packetSize = 10 + msgToSend.messageLength;
						result = sendMessage(socket, msgToSend);
						std::cout << "Auth failure" << std::endl;
						continue;
					}
					else
					{
						// Email doesn't exist
						auth::AuthenticateWebFailure authFail;
						std::string serializedFail;
						authFail.set_requestid(requestID);
						authFail.set_type(auth::AuthenticateWebFailure_reason_INVALID_CREDENTIALS);
						authFail.SerializeToString(&serializedFail);

						ChatMessage msgToSend;
						msgToSend.message = serializedFail;
						msgToSend.header.messageType = 12; // Web auth fail
						msgToSend.messageLength = msgToSend.message.length();
						msgToSend.header.packetSize = 10 + msgToSend.messageLength;
						result = sendMessage(socket, msgToSend);

						continue;
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
					//sessionInfo.initializeUser(newConnection); // Add new user to the "database" 
					FD_SET(newConnection, &activeSockets);
					FD_CLR(listenSocket, &socketsReadyForReading);


					printf("Client connected with Socket: %d\n", (int)newConnection);

					//int randomMat = rand() % numOfMats;
					//ChatMessage msgToSend;                                                                                       // INITIAL MESSAGE TO NEW USERS     TODO should we also output just !help or the entirety
					//msgToSend.header.messageType = 1; // Message type 1 to client is something to output into their chat window
					//msgToSend.message = "\n\n\n\n\n\n\n" + welcomeMats[randomMat] + "\n\n\n\n";
					//msgToSend.message += systemSignature + "} " + welcome;

					//msgToSend.messageLength = msgToSend.message.length();
					//msgToSend.header.packetSize = 10 + msgToSend.messageLength;
					//sendMessage(newConnection, msgToSend);
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
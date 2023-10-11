#pragma once

#include <vector>
#include <map>
#include <string>

#include <WinSock2.h>

struct UserInfo
{
	std::string username;
	std::vector<int> rooms;
};

class cUserRoomInfoHandler
{
public:
	cUserRoomInfoHandler();
	~cUserRoomInfoHandler();

	// Room actions
	std::vector<int> removeFromRoom(SOCKET socket, int roomToLeave);   // Should probably split this in 2 (remove from room and purge user)
	bool addToRoom(SOCKET socket, int room);

	//Getters
	std::vector<SOCKET> getRoomUsers(int room);

	// Initializers/setters
	void initializeUser(SOCKET socket);
	bool setUsername(SOCKET socket, std::string username);


private:
	std::map<int, std::vector<SOCKET>> room_map; // Map with room numbers as keys, and vectors of user sockets as values
	std::map<SOCKET, UserInfo> userinfo_map; // Map of userinfo, identifiable by their unique socket
};


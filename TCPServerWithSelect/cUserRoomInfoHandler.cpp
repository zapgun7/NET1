#include "cUserRoomInfoHandler.h"

cUserRoomInfoHandler::cUserRoomInfoHandler()
{

}

cUserRoomInfoHandler::~cUserRoomInfoHandler()
{

}




std::vector<int> cUserRoomInfoHandler::removeFromRoom(SOCKET socket, int roomToLeave)
{
	std::vector<int> returnRooms;
	int singleRoomToErase = -1;
	std::map<SOCKET, UserInfo>::iterator itInfo = userinfo_map.find(socket); // find userinfo associated with socket

	for (unsigned int room = 0; room < itInfo->second.rooms.size(); room++) // Iterate through all rooms the user is in
	{
		if (roomToLeave == -1) // If requested to leave all rooms (user disconnects)
		{
			// Do nothing, 
		}
		// Otherwise check if current room iteration is not the one requested the user leave
		else if (roomToLeave != itInfo->second.rooms[room]) // If request is only one room
		{
			continue;
		}

		std::map< int, std::vector<SOCKET>>::iterator itUserVector = room_map.find(itInfo->second.rooms[room]); // Find vector associated with that room iteration

		for (unsigned int i = 0; i < itUserVector->second.size(); i++) // Iterate through users in that room 
		{
			if (itUserVector->second[i] == socket)
			{
				itUserVector->second.erase(itUserVector->second.begin() + i); // remove from the room
				break; 
			}
		}
		singleRoomToErase = room; // Remember the room the user left in the case of just leaving a single room

		// Add to list of rooms we remove the user from 
		returnRooms.push_back(room);    // Will only add one if only removing user from one, otherwise adds all rooms the user is a part of
	} // User room iteration


	// If we're dealing with a user full disconnecting from the server
	if (roomToLeave == -1)
		userinfo_map.erase(socket); // Remove user from the userinfo map
	else
		itInfo->second.rooms.erase(itInfo->second.rooms.begin() + singleRoomToErase); // Remove the single room from the user's active rooms

	return returnRooms; // Return room identifier(s) to broadcast user leaving
}

bool cUserRoomInfoHandler::addToRoom(SOCKET socket, int room)
{
	std::map<SOCKET, UserInfo>::iterator itInfo = userinfo_map.find(socket); // Find user information in map
	if (itInfo == userinfo_map.end())
	{
		// Didn't find user information
		return false;
	}

	// Verify that user isn't already in the room
	for (unsigned int i = 0; i < itInfo->second.rooms.size(); i++)
	{
		if (itInfo->second.rooms[i] == room)
			return false;
	}


	std::map< int, std::vector<SOCKET>>::iterator itUserVector = room_map.find(room); // Find user vector associated with the desired room
	if (itUserVector == room_map.end()) // Room doesn't exist, so create it
	{
		std::vector<SOCKET> initVect;
		initVect.push_back(socket); // Add user to the new vector
		room_map.insert({ room, initVect });
	}
	itUserVector->second.push_back(socket); // Room exists, so add socket to the vector
	itInfo->second.rooms.push_back(room); // Add newly joined room to individual user info

	return true; // Successfully added to room
}



std::vector<SOCKET> cUserRoomInfoHandler::getRoomUsers(int room)
{
	std::map<int, std::vector<SOCKET>>::iterator itUserVector = room_map.find(room); // Find user information in map
	if (itUserVector == room_map.end()) // If room doesn't exist
	{
		// Didn't find user information
		std::vector<SOCKET> emptyReturn;
		return emptyReturn;
	}
	return itUserVector->second; // Return vector of users
}

std::vector<int> cUserRoomInfoHandler::getUserRooms(SOCKET socket)
{
	std::map<SOCKET, UserInfo>::iterator itInfo = userinfo_map.find(socket); // Find user information in map
	if (itInfo == userinfo_map.end())
	{
		// Didn't find user information
		std::vector<int> emptyReturn;
		return emptyReturn;
	}


	return itInfo->second.rooms;
}

std::string cUserRoomInfoHandler::getUsername(SOCKET socket)
{
	std::map<SOCKET, UserInfo>::iterator itInfo = userinfo_map.find(socket); // Find user information in map
	if (itInfo == userinfo_map.end())
	{
		// Didn't find user information
		std::string emptyReturn = "";
		return emptyReturn;
	}

	return itInfo->second.username;
}



void cUserRoomInfoHandler::initializeUser(SOCKET socket)
{
	UserInfo initInfo;
	initInfo.username = nullptr; // Don't let user enter rooms unless their name doesn't point to null

	userinfo_map.insert({ socket,initInfo });
}

bool cUserRoomInfoHandler::setUsername(SOCKET socket, std::string username)  // Perhaps here to set limitations on usernames? But should output reason for username refusal
{
	std::map<SOCKET, UserInfo>::iterator itInfo = userinfo_map.find(socket);
	
	if (itInfo == userinfo_map.end())
	{
		// Nope
		return false;
	}

	return true;
}

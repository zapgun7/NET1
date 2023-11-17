#pragma once

// This header file includes all of the required header files we need
#include <mysql/jdbc.h>

// You only need 1 of these, so you can make it a singleton
// But you need to make sure you know who is accessing it, and
// they are allowed to access it.

class MySQLUtil
{
public:
	MySQLUtil();
	~MySQLUtil();

	void ConnectToDatabase(
		const char* host, const char* username, const char* password, const char* schema);
	void Disconnect();

	sql::PreparedStatement* PrepareStatement(const char* query);

	sql::ResultSet* Select(const char* query);
	int Update(const char* query);
	int Insert(const char* query);

private:
	sql::mysql::MySQL_Driver* m_Driver;
	sql::Connection* m_Connection;
	sql::ResultSet* m_ResultSet;
	sql::Statement* m_Statement;

	bool m_Connected;
};
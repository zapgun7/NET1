#include "../Shared/authentication.pb.h"
#include <iostream>
#include <fstream>
#include <string.h>





int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;


	auth::AuthenticateWeb testweb;
	testweb.set_email("ihate@effyuu.ca");
	testweb.set_plaintextpassword("dumbasspass");

	std::string serializedString;
	testweb.SerializeToString(&serializedString);

	std::cout << serializedString << std::endl;
	for (int idxString = 0; idxString < serializedString.length(); idxString++) {
		printf("%02X ", serializedString[idxString]);
	}


	auth::AuthenticateWeb dstestweb;
	bool success = dstestweb.ParseFromString(serializedString);
	if (!success)
		std::cout << "Failed to parse the AuthenticateWeb" << std::endl;
	std::cout << "Parsing successful" << std::endl;

	std::cout << "Email: " << dstestweb.email() << std::endl;
	std::cout << "Password: " << dstestweb.plaintextpassword() << std::endl;
}
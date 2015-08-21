#include "stdafx.h"
#include "DatabaseHandler.h"


DatabaseHandler::DatabaseHandler()
{
}


DatabaseHandler::~DatabaseHandler()
{
}


void DatabaseHandler::registerUser(std::string username, std::string password, std::string baseDir) {
	std::string query = "INSERT INTO USERS (username, password, basedir) VALUES ('" + username + "', '" + password + "', '" + baseDir +"')";
	// todo: execute the query!
	// todo : use precompiled queries or check for avoid SQL INJECTION
}

bool DatabaseHandler::logUser(std::string username, std::string password) {
	std::string query = "SELECT password FROM USERS where username = ' " + username + " '";
	// todo: execute the query!

	// todo: calculate md5 (or sha1) on password

	std::string pass = "password";
	return pass == password; // hanno ridefinito operator == 

	// todo : use precompiled queries or check for avoid SQL INJECTION
}

bool DatabaseHandler::existsFile(std::string username, std::string path, std::string fileName) {
	std::string query = "SELECT count(*) FROM FILES where username = ' " + username + " ' AND path = '" + path +"' AND name = ' " + fileName + " '";
	int number = 1;
	return number == 1;
}

void DatabaseHandler::createFileForUser(std::string username, std::string path, std::string fileName) {
	// note : the path is "path/filename.extension"
	if (this->existsFile(username, path, fileName)) throw new std::exception("file already exists");

	std::string query = "INSERT INTO FILES (name, path, username) VALUES (' " + fileName + " ', ' " + path + "', ' " + username + " ')";
	// todo: apply this query

	// now i need to create the blob
	query = "SELECT COUNT(*) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = ' " + username + " ')";
	int count = 1, max = -1;
	if (count == 0) max = 0;
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = ' " + username + " ')";
		int queryResult = 33;
		max = queryResult++;
	}
	time_t t = time(0); // current time. should be an int with the time since 1 Jan 1970. compatible with db??
	
	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES(' " + fileName + " ', ' " + path + "', ' " + username + " ', " +  std::to_string(t) +", " + std::to_string(max) +" )";
}

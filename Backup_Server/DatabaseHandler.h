#pragma once
#include <string>
#include <sql.h>

class DatabaseHandler
{
private:
	SQLHDBC hdbc; // connection handle
	SQLHENV env;  // enviroment handle
public:
	DatabaseHandler();
	~DatabaseHandler();
	
	/*
		function : logUser
		@param : username
		@param : password
	*/
	bool logUser(std::string, std::string);


	/*
	function : registerUser
	@param : username
	@param : password
	@param : baseDirectory
	*/
	void registerUser(std::string, std::string, std::string);


	/*
	function : getUserFolder
	@param : username
	This function returns for each file of the selected user:
		1) path
		2) name
		3) lasted version (blob)
	*/
	std::string getUserFolder(std::string);


	bool existsFile(std::string, std::string, std::string);

	/*
		@param1 : username
		@param2 : path
		@param3 : fileName

		@warning : if file already exists an std::exception is thrown
	*/
	void createFileForUser(std::string, std::string, std::string);
	
	
	/*
	@param1 : username
	@param2 : path
	@param3 : fileName

	@warning : if file already exists an std::exception is thrown
	*/
	void createNewBlobForFile(std::string, std::string, std::string);


	/*
	@param1 : username
	@param2 : path
	@param3 : fileName

	@warning : if file does not exist an std::exception is thrown
	*/
	void deleteFile(std::string, std::string, std::string);

	/*
	@param1 : username
	@param2 : path
	@param3 : fileName

	@warning : if file does not exist an std::exception is thrown
	*/
	void removeFile(std::string, std::string, std::string);

	/*
	@param1 : username
	@param2 : path
	@param3 : fileName

	@warning : if file does not exist an std::exception is thrown

	Returns the string version of a collection of date of last modify for that file
	*/
	std::string getFileVersions(std::string, std::string, std::string);
};


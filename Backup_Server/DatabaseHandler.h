#pragma once
#include <string>
class DatabaseHandler
{
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
	function : getUserFileByName
	@param : username
	@param : filePath ( path/filename )
	*/
	std::string getUserFileByName(std::string, std::string, std::string);


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
};


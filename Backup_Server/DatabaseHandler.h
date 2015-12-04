#pragma once
#include <string>
#include <sql.h>
#include "sqlite3.h"
#include <mutex>

class DatabaseHandler
{
private:
	sqlite3 *database;
	static std::mutex m;

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
	@param : baseDirectory
	This function returns for each file of the selected user:
		1) path
		2) name
		3) lasted version (blob)
	*/
	std::string getUserFolder(std::string, std::string);

	/*
	function : existsFile
	@param : username
	@param : password
	@param : filename
	*/
	bool existsFile(std::string, std::string, std::string);

	/*
		@param1 : username
		@param2 : path
		@param3 : fileName

		@warning : if file already exists an std::exception is thrown
	*/
	int createFileForUser(std::string, std::string, std::string);
	
	
	/*
	@param1 : username
	@param2 : path
	@param3 : fileName

	@warning : if file already exists an std::exception is thrown
	*/
	int createNewBlobForFile(std::string, std::string, std::string);


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


	/*
	@param1 : username
	@param2 : blob
	@param 3: checksum
	Since the blob is unique for the user, then i can just add its checksum
	*/
	void addChecksum(std::string, int, std::string);


	/*
	@param : username
	@param : folderPath

	This function searched for all the files that have been deleted by the users.
	*/
	std::string getDeletedFiles(std::string, std::string);


	/*
	@param1 : username
	@param2 : path
	@param3 : filename
	@param4 : lastupdate

	*/
	int getBlob(std::string, std::string, std::string, std::string);

	
	/*
	@param1 : username
	@param2 : path
	@param3 : filename
	*/
	bool isDeleted(std::string, std::string, std::string);

	/*
		If username and password match, it gives the path. otherwise an exception is thrown
	*/
	std::string getPath(std::string username);

	/*
	This function add a new version for a file. it is used when i want to add a new file but i alrady have the blob where that version is saved
	*/
	void addVersion(std::string username, std::string path, std::string filename, std::string lastModified, int blob);

	int getLastBlob(std::string username, std::string path, std::string filename);

	std::string getLastVersion(std::string username, std::string path, std::string filename);
	
	std::string getBlobVersion(std::string username, int blob);

	/*
		@param : digitN
		@return : a radom string, of the length asked by the parameter
	*/
	std::string getRandomString(int digitN);


	/*
		@param : patameter
		@return : a new string where the character ' has been replaced with '', in order to avoid sql injection
	*/
	std::string secureParameter(std::string parameter);
};



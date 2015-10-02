#include "stdafx.h"

#include "DatabaseHandler.h"
#include "sqlite3.h"
#include <windows.data.json.h>


DatabaseHandler::DatabaseHandler()
{
	sqlite3_open("PDSProject.db", &database);

	if (database == nullptr) {
		std::wcout << L"could not open the db" << std::endl;
		return;
	}
}


DatabaseHandler::~DatabaseHandler()
{
	// here i need to disconnect from the database
	sqlite3_close(database);
}

/*
	Function Name: registerUser
	this function could throw std::exception. read the message to understand what happened
*/
void DatabaseHandler::registerUser(std::string username, std::string password, std::string baseDir) {
	
	std::string query = "INSERT INTO USERS (username, password, folder) VALUES ('" + username + "', '" + password + "', '" + baseDir +"')";
	char* error = NULL;
	// todo : use precompiled queries or check for avoid SQL INJECTION

	std::cout << std::endl << query << std::endl;
	
	sqlite3_exec(database, query.c_str(), NULL, NULL, &error);
	if (error != nullptr) {
		sqlite3_free(error);

		std::string msg("impossible to create the new user: ");
		msg += error;
		std::cout << msg << std::endl;
		throw new std::exception(msg.c_str());
	}
	else
		std::cout << "db handler : utente loggato correttamente" << std::endl;
}

bool DatabaseHandler::logUser(std::string username, std::string password) {

	std::string query = "SELECT password FROM USERS where username = '" + username + "'";
	std::string pass = "password";
	// todo : use precompiled queries or check for avoid SQL INJECTION
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		((std::string*)data)->assign(argv[0]);
		return 0;
		}, &pass, &error);	
	if (error != nullptr) {
		sqlite3_free(error);
		throw new std::exception("impossible to get password");
	}

	// todo: calculate md5 (or sha1) on password
	return pass == password; //  operator == has been redefined
	
}

// this function return something like "folder": [{"name":"filename1", "path", "filepath1"}, {"name":"filename2", "path", "filepath2"}]
std::string DatabaseHandler::getUserFolder(std::string username)
{

	std::string query = "SELECT name, path FROM VERSIONS V WHERE username = '" + username + "' AND Blob is not NULL AND lastUpdate = (\
							SELECT  MAX(lastUpdate) FROM VERSIONS WHERE username = '" + username + "' AND name = V.name AND path = V.path)";

	std::string jsonFolder = "\"Folder\":[";
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string *folder = (std::string*)data;		
		std::string appendString = "{ \"" + std::string(azColName[0]) + "\":\"" + std::string(argv[0]) + "\", \"" + std::string(azColName[1]) + "\":\"" + std::string(argv[1]) + "\" },";
		folder->append(appendString);
		return 0;
	}, &jsonFolder, &error);


	if (error != nullptr) {
		sqlite3_free(error);
		throw new std::exception("error while getting the filesystem for the user ");
	}

	jsonFolder[jsonFolder.size() - 1] = ']';

	return jsonFolder;
}

bool DatabaseHandler::existsFile(std::string username, std::string path, std::string fileName) {
	
	std::string query = "SELECT count(*) FROM FILES where username = '" + username + "' AND path = '" + path +"' AND name = '" + fileName + "'";
	int number;
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &number, &error);

	if (error != nullptr) {		
		sqlite3_free(error);
		throw new std::exception("existsFile has crashed!");
	}
	return number == 1;
}

int DatabaseHandler::createFileForUser(std::string username, std::string path, std::string fileName) {
	
	
	if (this->existsFile(username, path, fileName)) throw new std::exception("file already exists");
	sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
	std::string query = "INSERT INTO FILES (name, path, username) VALUES (' " + fileName + " ', ' " + path + "', ' " + username + " ')";
	char* error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		throw new std::exception("no insertion");

	}

	// now i need to create the blob; how many blobs can i count for that user?
	query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '" + username + "')";
	// todo: (for acid) DELETE FROM FILES WHERE name='filename' and path = 'path' AND username='username'
	int numberOfBlobs,max=-1;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &numberOfBlobs, &error);
	
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: createFileForUser-> no count");
	}

	if (numberOfBlobs == 0) max = 0;
	// TODO: create the file and create the new Blob should be in the same transaction. modify that
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE username = '" + username + "'";
		sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
			*((int*)data) = strtol(argv[0], nullptr, 10) + 1;
			return 0;
		}, &max, &error);
		if (error != nullptr) {
			sqlite3_free(error);
			sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
			throw new std::exception("DbHandler:: createFileForUser-> no select max blob");
		}

	}
	

	std::string t = std::to_string(time(0)); // current time. should be an int with the time since 1 Jan 1970. compatible with db??
	
	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES(' " + fileName + " ', ' " + path + "', ' " + username + " ', '" +  t +"', " + std::to_string(max) +" )";	
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: createFileForUser-> no insert blob");
	}

	sqlite3_exec(database, "COMMIT", nullptr, nullptr, nullptr);
	return 0;
}

int DatabaseHandler::createNewBlobForFile(std::string username, std::string path, std::string fileName) {
	if (! this->existsFile(username, path, fileName)) throw new std::exception("file does not exist");

	int count = 1, max = -1;
	char * error;
	sqlite3_exec(database, "BEGIN EXCLUSIVE TRANSACTION", nullptr, nullptr, nullptr);


	std::string query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '" + username + "')";
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);	
		return 0;
	}, &count, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: createFileForUser-> no select count(blobs)");
	}

	if (count == 0) max = 0;
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE username = '" + username + "'";	
		sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
			*((int*)data) = strtol(argv[0], nullptr, 10) + 1;
			return 0;
		}, &max, &error);
		
		if (error != nullptr) {
			sqlite3_free(error);
			sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
			throw new std::exception("DbHandler:: createFileForUser-> no select max(blob)");
		}
	}
	time_t t = time(0); // current time. should be an int with the time since 1 Jan 1970. compatible with db??

	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES(' " + fileName + " ', ' " + path + "', ' " + username + " ', '" + std::to_string(t) + "', " + std::to_string(max) + " )";
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: createFileForUser-> no insert new blob");
	}

	sqlite3_exec(database, "COMMIT TRANSACTION", nullptr, nullptr, nullptr);

	return max;
}

void DatabaseHandler::deleteFile(std::string username, std::string path, std::string filename)
{
	if (!existsFile(username, path, filename)) throw new std::exception("no file to delete");
	if(isDeleted(username, path, filename)) throw new std::exception("file already deleted");
	
	
	time_t t = time(0); 
	std::string query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES('" + filename + "', '" + path + "', '" + username + "', '" + std::to_string(t) + "', NULL )";

	char* error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: deleteFiles -> error while inserting NULL blob");
	}
}



void DatabaseHandler::removeFile(std::string username, std::string path, std::string filename) {
	if (!existsFile(username, path, filename)) throw new std::exception("no file to delete");
	// the file exist

	char* error;
	sqlite3_exec(database, "BEGIN EXCLUSIVE TRANSACTION", nullptr, nullptr, nullptr);

	std::string query = "DELETE FROM VERSIONS WHERE username = '" + username + "' AND path = '" + path+"' AND name = '"+filename+"'";

	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: removeFile -> error while deleting from FILES");
	}

	query = "DELETE FROM FILE WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "'";
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: removeFile -> error while deleting from VERSIONS");
	}

	sqlite3_exec(database, "BEGIN COMMIT", nullptr, nullptr, nullptr);
}

std::string DatabaseHandler::getFileVersions(std::string username, std::string path, std::string filename)
{
	if (!existsFile(username, path, filename)) throw new std::exception("cannot find the file"); // todo: maybe also if it has been deleted?

	std::string versions = "\"versions\" : [";
	char* error;

	std::string query = "SELECT lastUpdate FROM VERSIONS WHERE Blob is not NULL AND username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "' ORDER BY lastUpdate DESC";

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string* v = (std::string*)data;
		std::string appendString =
			"{ \""+ std::string(azColName[0]) +"\" : \""+ std::string(argv[0]) +"\" },";
		return 0;
	}, &versions, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		throw new std::exception("DbHandler:: getFileVersions -> error while selecting versions");
	}

	versions[versions.size() - 1]  = ']';
	
	return versions;
}

void DatabaseHandler::addChecksum(std::string username, int blob, std::string checksum)
{
	std::string query = "UPDATE VERSIONS SET checksum = '" + checksum + "' WHERE Blob = " + std::to_string(blob) + " AND username = '" + username + "'";
	char* error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler::addChecksum -> error while inserting");
	}
}

std::string DatabaseHandler::getDeletedFiles(std::string username)
{
	std::string query = "SELECT path, name FROM VERSIONS V WHERE username = '" + username + "'AND Blob IS NULL AND lastUpdate = (\
							SELECT MAX(lastUpdate) FROM VERSIONS WHERE username ='" + username + "' AND name = V.name AND path = V.path)";
	
	std::string result = " \"DeletedFiles\": [";
	char* error;

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string* v = (std::string*)data;
		std::string appendString =
			"{ \"" + std::string(azColName[0]) + "\" : \"" + std::string(argv[0]) + "\", \""+std::string(azColName[1])+"\":\""+std::string(argv[1])+"\" },";
		return 0;
	}, &result, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		throw new std::exception("DbHandler::getDeletedFiles -> error while selecting deleted files");
	}

	result[result.size() - 1] = ']';

	return result;
}

int DatabaseHandler::getBlob(std::string username, std::string path, std::string filename, std::string datetime)
{
	if (!existsFile(username, path, filename) || isDeleted(username, path, filename)) return -1;
	
	std::string query = "SELECT Blob from VERSIONS WHERE username = '"+username+"' AND path = '"+path+"' AND name='"+filename+"' AND lastUpdate='"+datetime+"'";	
	int blob = -1;
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &blob, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		throw new std::exception("DbHandler::getBlob-> error while executing select(blob)");
	}

	return blob;
}

bool DatabaseHandler::isDeleted(std::string username, std::string path, std::string filename)
{
	
	int validBlob = -1;
	char* error;
	std::string query = "SELECT COUNT(*) FROM (\
							SELECT Blob FROM VERSIONS WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "' AND lastUpdate = (\
								SELECT MAX(lastUpdate) FROM VERSIONS  WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "')\
						) WHERE Blob is not NULL";

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &validBlob, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		throw new std::exception("DbHandler::isDeleted-> error while checking if last blob is null");
	}
	return validBlob == 0;
}





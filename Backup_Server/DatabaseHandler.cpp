#include "stdafx.h"

#include "DatabaseHandler.h"
#include "sqlite3.h"
#include <windows.data.json.h>


DatabaseHandler::DatabaseHandler()
{
	std::cout << "dbHandler constructor!! " << std::endl;
	database = nullptr;
	if (sqlite3_open("PDSProject.db", &database) != SQLITE_OK) {
		std::wcout << L"could not open/create the db" << std::endl;
		// todo: throw exception
		return;
	}
}


DatabaseHandler::~DatabaseHandler()
{
	// here i need to disconnect from the database
	std::cout << "des. dbhandler " << std::endl;
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
		std::string msg("impossible to create the new user: ");
		msg += error;
		sqlite3_free(error);
		std::cout << msg << std::endl;
		throw std::exception(msg.c_str());
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
		throw std::exception("impossible to get password");
	}

	// todo: calculate md5 (or sha1) on password
	return pass == password; //  operator == has been redefined
	
}

// this function return something like "folder": [{"name":"filename1", "path", "filepath1"}, {"name":"filename2", "path", "filepath2"}]
std::string DatabaseHandler::getUserFolder(std::string username)
{
	std::string query = "SELECT name, path FROM VERSIONS V WHERE username = '" + username + "' AND Blob is not NULL AND lastModified = (\
							SELECT  MAX(lastModified) FROM VERSIONS WHERE username = '" + username + "' AND name = V.name AND path = V.path)";
	
	std::string jsonFolder = "[";
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string *folder = (std::string*)data;		
		std::string appendString = "{ \"" + std::string(azColName[0]) + "\":\"" + std::string(argv[0]) + "\", \"" + std::string(azColName[1]) + "\":\"" + std::string(argv[1]) + "\" },";
		folder->append(appendString);
		return 0;
	}, &jsonFolder, &error);


	if (error != nullptr) {
		std::cout << "error : " << error << std::endl;
		sqlite3_free(error);
		throw std::exception("error while getting the filesystem for the user ");
	}

	if(jsonFolder[jsonFolder.size() - 1] == ',')
		jsonFolder[jsonFolder.size() - 1] = ']';
	else
		jsonFolder += ']';

	std::string returnFolder = "";
	for (int i = 0; i < jsonFolder.size(); i++) {
		if(jsonFolder[i] != '\\')
			returnFolder += jsonFolder[i];
		else
			returnFolder += "\\\\";
	}

	return returnFolder;
}

bool DatabaseHandler::existsFile(std::string username, std::string path, std::string fileName) {
	
	std::string query = "SELECT count(*) FROM FILES where username = '" + username + "' AND path = '" + path +"' AND name = '" + fileName + "'";
	std::cout << "exist query : " << std::endl << query << std::endl;

	int number;
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &number, &error);

	if (error != nullptr) {		
		sqlite3_free(error);
		throw std::exception("existsFile has crashed!");
	}
	return number == 1;
}

int DatabaseHandler::createFileForUser(std::string username, std::string path, std::string fileName) {
	
	
	if (this->existsFile(username, path, fileName)) throw std::exception("file already exists");
	sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
	std::string query = "INSERT INTO FILES (name, path, username) VALUES ('" + fileName + "', '" + path + "', '" + username + "')";
	char* error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		std::cout << "impossibile inserire in files! " << error << std::endl;
		sqlite3_free(error);
		throw std::exception("no insertion");

	}

	// now i need to create the blob; how many blobs can i count for that user?
	query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '" + username + "'";
	// todo: (for acid) DELETE FROM FILES WHERE name='filename' and path = 'path' AND username='username'
	int numberOfBlobs = -1,max=-1;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &numberOfBlobs, &error);
	
	if (error != nullptr) {
		std::cout << error << std::endl;
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler::createFileForUser-> no count");
	}

	std::cout << "# of blob for " << username << " = " << numberOfBlobs << std::endl;

	if (numberOfBlobs == 0) max = 0;
	// TODO: create the file and create the new Blob should be in the same transaction. modify that
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE username = '" + username + "'";
		sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
			*((int*)data) = strtol(argv[0], nullptr, 10) + 1;
			return 0;
		}, &max, &error);
		if (error != nullptr) {
			std::cout << "error in select max(blob) : " << error << std::endl;
			sqlite3_free(error);
			sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
			throw std::exception("DbHandler:: createFileForUser-> no select max blob");
		}

	}
	
	std::cout << "so max is " << max << std::endl;

	std::string timestamp;
	time_t t = time(0);
	struct tm now;
	if (EINVAL == localtime_s(&now, &t))
		std::cout << "could not fill tm struct" << std::endl;
	timestamp += std::to_string((now.tm_year + 1900)) + "-" + std::to_string((now.tm_mon + 1)) + '-' + std::to_string(now.tm_mday) + ' ';
	if (now.tm_hour < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_hour) + ":";
	if (now.tm_min < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_min);


	std::cout << "timestamp = " << timestamp << std::endl;


	query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('" + fileName + "', '" + path + "', '" + username + "', '" + timestamp +"', '" + std::to_string(max) +"' )";
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		std::cout << "error in insert blob : " << error << std::endl;
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: createFileForUser-> no insert blob");
	}

	sqlite3_exec(database, "COMMIT", nullptr, nullptr, nullptr);
	return max;
}

int DatabaseHandler::createNewBlobForFile(std::string username, std::string path, std::string fileName) {
	if (! this->existsFile(username, path, fileName)) throw std::exception("file does not exist");

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
		throw std::exception("DbHandler:: createFileForUser-> no select count(blobs)");
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
			throw std::exception("DbHandler:: createFileForUser-> no select max(blob)");
		}
	}
	std::string timestamp;
	time_t t = time(0);
	struct tm now;
	if (EINVAL == localtime_s(&now, &t))
		std::cout << "could not fill tm struct" << std::endl;
	timestamp += std::to_string((now.tm_year + 1900)) + "-" + std::to_string((now.tm_mon + 1)) + '-' + std::to_string(now.tm_mday) + ' ';
	if (now.tm_hour < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_hour) + ":";
	if (now.tm_min < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_min);

	query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('" + fileName + "', '" + path + "', '" + username + "', '" + timestamp+ "', " + std::to_string(max) + " )";
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: createFileForUser-> no insert new blob");
	}

	sqlite3_exec(database, "COMMIT TRANSACTION", nullptr, nullptr, nullptr);

	return max;
}

void DatabaseHandler::deleteFile(std::string username, std::string path, std::string filename)
{
	if (!existsFile(username, path, filename)) throw std::exception("no file to delete");
	if(isDeleted(username, path, filename)) throw std::exception("file already deleted");
	
	
	time_t t = time(0); 
	std::string query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('" + filename + "', '" + path + "', '" + username + "', '" + std::to_string(t) + "', NULL )";

	char* error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: deleteFiles -> error while inserting NULL blob");
	}
}



void DatabaseHandler::removeFile(std::string username, std::string path, std::string filename) {
	if (!existsFile(username, path, filename)) throw std::exception("no file to delete");
	// the file exist

	char* error;
	sqlite3_exec(database, "BEGIN EXCLUSIVE TRANSACTION", nullptr, nullptr, nullptr);

	std::string query = "DELETE FROM VERSIONS WHERE username = '" + username + "' AND path = '" + path+"' AND name = '"+filename+"'";

	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: removeFile -> error while deleting from FILES");
	}

	query = "DELETE FROM FILE WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "'";
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: removeFile -> error while deleting from VERSIONS");
	}

	sqlite3_exec(database, "BEGIN COMMIT", nullptr, nullptr, nullptr);
}

std::string DatabaseHandler::getFileVersions(std::string username, std::string path, std::string filename)
{
	if (!existsFile(username, path, filename)) {
		std::cout << "cannot find the file" << std::endl;
		throw std::exception("cannot find the file"); // todo: maybe also if it has been deleted?
	}

	std::string versions = "[";
	char* error;

	std::string query = "SELECT lastModified FROM VERSIONS WHERE Blob is not NULL AND username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "' ORDER BY lastModified DESC";

	std::cout << query << std::endl;
	OutputDebugStringA(query.c_str());

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string* v = (std::string*)data;
		std::string appendString =
			"{ \""+ std::string(azColName[0]) +"\" : \""+ std::string(argv[0]) +"\" },";
		v->append(appendString);
		return 0;
	}, &versions, &error);

	if (error != nullptr) {
		std::cout << "error: " << error << std::endl;
		sqlite3_free(error);
		throw std::exception("DbHandler:: getFileVersions -> error while selecting versions");
	}
	std::cout << "the query worked!";
	if (versions[versions.size() - 1] == ',')
		versions[versions.size() - 1] = ']';
	else
		versions += ']';
	
	std::cout << versions << std::endl;

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
		throw std::exception("DbHandler::addChecksum -> error while inserting");
	}
}

std::string DatabaseHandler::getDeletedFiles(std::string username)
{
	std::string query = "SELECT path, name FROM VERSIONS V WHERE username = '" + username + "'AND Blob IS NULL AND lastModified = (\
							SELECT MAX(lastModified) FROM VERSIONS WHERE username ='" + username + "' AND name = V.name AND path = V.path)";
	
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
		throw std::exception("DbHandler::getDeletedFiles -> error while selecting deleted files");
	}

	result[result.size() - 1] = ']';

	return result;
}

int DatabaseHandler::getBlob(std::string username, std::string path, std::string filename, std::string datetime)
{
	if (!existsFile(username, path, filename) || isDeleted(username, path, filename)) return -1;
	
	std::string query = "SELECT Blob from VERSIONS WHERE username = '"+username+"' AND path = '"+path+"' AND name='"+filename+"' AND lastModified='"+datetime+"'";	
	int blob = -1;
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &blob, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		throw std::exception("DbHandler::getBlob-> error while executing select(blob)");
	}

	return blob;
}

bool DatabaseHandler::isDeleted(std::string username, std::string path, std::string filename)
{
	
	int validBlob = -1;
	char* error;
	std::string query = "SELECT COUNT(*) FROM (\
							SELECT Blob FROM VERSIONS WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "' AND lastModified = (\
								SELECT MAX(lastModified) FROM VERSIONS  WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "')\
						) WHERE Blob is not NULL";

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &validBlob, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		throw std::exception("DbHandler::isDeleted-> error while checking if last blob is null");
	}
	return validBlob == 0;
}
std::string DatabaseHandler::getPath(std::string username)
{
	
	char* error;
	std::string path = "";
	std::string query = "SELECT folder FROM USERS WHERE username='"+username+"'";
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((std::string*)data) += argv[0];
		return 0;
	}, &path, &error);

	
	if (error != nullptr) {
		sqlite3_free(error);
		throw std::exception("DbHandler::getPath-> error while searchng for path");
	}
	else if(path == "")
		throw std::exception("username does not exist");
	return path;
}





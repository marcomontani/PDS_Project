#include "stdafx.h"

#include "DatabaseHandler.h"
#include "sqlite3.h"
#include "EncryptionHandler.h"
#include <windows.data.json.h>

#define PRECOMPILED

std::mutex DatabaseHandler::m;


enum stmnt_positions {
	INSERT_USER = 0,
	GET_SALT = 1,
	GET_PASSWORD = 2,
	GET_USER_FOLDER = 3,
	USER_FILE_EXISTS = 4,
	ADD_FILE = 5,
	COUNT_USER_VERSIONS = 6,
	GET_NEXT_BLOB_NAME = 7,
	ADD_VERSION = 8,
	DELETE_VERSIONS = 9,
	DELETE_FILES = 10,
	ADD_CHECKSUM = 11,
	GET_DELETED_FILES = 12,
	GET_BLOB = 13,
	IS_DELETED = 14,
	GET_USER_PATH = 15,
	GET_LAST_BLOB = 16,
	GET_BLOB_LAST_VERSION_DATE = 17,
	GET_BLOB_DATE = 18
};


void DatabaseHandler::prepareStatements() {
	std::string query = "INSERT INTO USERS (username, password, folder, salt) VALUES (?, ?, ?, ?)";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[INSERT_USER], nullptr);

	sqlite3_prepare_v2(database, "SELECT salt FROM USERS WHERE username=? and salt is not NULL", -1, &statements[GET_SALT], nullptr);

	query = "SELECT password FROM USERS where username = '?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_PASSWORD], nullptr);

	query = "SELECT name, path, checksum, lastModified FROM VERSIONS V WHERE username = '?' AND Blob is not NULL AND lastModified = (\
				SELECT  MAX(lastModified) FROM VERSIONS WHERE username = '?' AND name = V.name AND path = V.path)";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_USER_FOLDER], nullptr);

	query = "SELECT count(*) FROM FILES where username = '?' AND path = '?' AND name = '?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[USER_FILE_EXISTS], nullptr);

	query = "INSERT INTO FILES (name, path, username) VALUES ('?', '?', '?')";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[ADD_FILE], nullptr);

	query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[COUNT_USER_VERSIONS], nullptr);

	query = "SELECT MAX(Blob) FROM VERSIONS WHERE username = '?'";

	query = "SELECT salt FROM USERS where username = '?' and salt is not NULL";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_NEXT_BLOB_NAME], nullptr);

	query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('?', '?', '?', '?', '?')";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[ADD_VERSION], nullptr);

	query = "DELETE FROM VERSIONS WHERE username = '?' AND path = '?' AND name = '?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[DELETE_VERSIONS], nullptr);

	query = "DELETE FROM FILES WHERE username = '?' AND path = '?' AND name = '?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[DELETE_FILES], nullptr);

	query = "UPDATE VERSIONS SET checksum = '?' WHERE Blob = ? AND username = '?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[ADD_CHECKSUM], nullptr);

	query = "SELECT path, name FROM VERSIONS V WHERE username = '?'AND Blob IS NULL AND lastModified = (\
							SELECT MAX(lastModified) FROM VERSIONS WHERE username ='?' AND name = V.name AND path = V.path)";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_DELETED_FILES], nullptr);

	query = "SELECT Blob from VERSIONS WHERE username = '?' AND path = '?' AND name='?' AND lastModified='?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_BLOB], nullptr);

	query = "SELECT COUNT(*) FROM (\
							SELECT Blob FROM VERSIONS WHERE username = '?' AND path = '?' AND name = '?' AND lastModified = (\
								SELECT MAX(lastModified) FROM VERSIONS  WHERE username = '?' AND path = '?' AND name = '?')\
						) WHERE Blob is not NULL";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[IS_DELETED], nullptr);

	query = "SELECT folder FROM USERS WHERE username='?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_USER_PATH], nullptr);

	query = "SELECT Blob FROM VERSIONS WHERE username='?' AND path='?' AND name='?' AND Blob IS NOT NULL AND lastModified = (\
		SELECT MAX(lastModified) FROM VERSIONS WHERE username='?' AND path='?' AND name='?' and Blob is not null)";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_LAST_BLOB], nullptr);

	query = "SELECT MAX(lastModified) FROM VERSIONS WHERE username='?' AND path='?' AND name='?'";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_BLOB_LAST_VERSION_DATE], nullptr);

	query = "SELECT lastModified FROM VERSIONS WHERE username='?' AND Blob = ?";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[GET_BLOB_DATE], nullptr);

	query = "SELECT salt FROM USERS where username = '?' and salt is not NULL";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[1], nullptr);

	query = "SELECT salt FROM USERS where username = '?' and salt is not NULL";
	sqlite3_prepare_v2(database, query.c_str(), query.size(), &statements[1], nullptr);
}

sqlite3_stmt* DatabaseHandler::getStatement(int number) {
#ifdef DEBUG
	std::cout << "requested statement " << number << std::endl;
#endif
	if (number < 0 || number > 18) return nullptr;
	//sqlite3_reset(statements[number]);
	return statements[number];
}

DatabaseHandler::DatabaseHandler()
{
	std::cout << "dbHandler constructor!! " << std::endl;
	
	database = nullptr;
	if (sqlite3_open("PDSProject.db", &database) != SQLITE_OK) {
		std::wcout << L"could not open/create the db" << std::endl;
		// todo: throw exception
		return;
	}
	statements = new sqlite3_stmt*[18];
	prepareStatements();
}


DatabaseHandler::~DatabaseHandler()
{
	// here i need to disconnect from the database
	std::cout << "des. dbhandler " << std::endl;
	sqlite3_close(database);
	delete[] statements;
}

/*
	Function Name: registerUser
	this function could throw std::exception. read the message to understand what happened
*/
void DatabaseHandler::registerUser(std::string username, std::string password, std::string baseDir) {
	std::string salt = this->getRandomString(50);
	EncryptionHandler ea;
	ea.update(password + salt);

#ifndef PRECOMPILED
	std::string query = "INSERT INTO USERS (username, password, folder, salt) VALUES ('" + username + "', '" + ea.final() + "', '" + baseDir + "', '" + salt + "')";

	char* error = NULL;
	// note : define precompiled queries or check for avoid SQL INJECTION

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
#else
	// "INSERT INTO USERS (username, password, folder, salt) VALUES ('" + username + "', '" + ea.final() + "', '" + baseDir + "', '" + salt + "')"
	std::string pass = ea.final();

	sqlite3_stmt* query = nullptr; 
	sqlite3_prepare_v2(database, "INSERT INTO USERS (username, password, folder, salt) VALUES (?, ?, ?, ?)", -1, &query, nullptr);
	sqlite3_bind_text(query, 1, username.c_str(), username.size(), SQLITE_STATIC);
	sqlite3_bind_text(query, 2, pass.c_str(), pass.size(), SQLITE_STATIC);
	sqlite3_bind_text(query, 2, pass.c_str(), pass.size(), SQLITE_STATIC);
	sqlite3_bind_text(query, 2, baseDir.c_str(), baseDir.size(), SQLITE_STATIC);

	if (int rc =  sqlite3_step(query) != SQLITE_DONE) { // something went wrong
		std::string msg("impossible to create the new user. errcode = ");
#ifdef DEBUG
		std::cout << msg << rc << std::endl;
#endif // DEBUG
		throw std::exception(msg.c_str());
	}

#ifdef DEBUG
	std::cout << "db handler : utente creato correttamente" << std::endl;
#endif // DEBUG


#endif // !PRECOMPILED

	
		
}

bool DatabaseHandler::logUser(std::string username, std::string password) {

	// i need to read the salt from the DB
	EncryptionHandler ea;
	std::string salt = "";

#ifndef PRECOMPILED
	std::string query = "SELECT salt FROM USERS where username = '" + username + "' and salt is not NULL";
	char* error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		if (argv[0] != nullptr) ((std::string *)data)->append(argv[0]);
		return 0;
	}, &salt, &error);

	if (error != nullptr) throw std::exception("impossible to get password");

	ea.update(password + salt);

	query = "SELECT password FROM USERS where username = '" + username + "'";
	std::string pass = "password";
	// todo : use precompiled queries or check for avoid SQL INJECTION

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		((std::string*)data)->assign(argv[0]);
		return 0;
	}, &pass, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		throw std::exception("impossible to get password");
	}

	return ea.final() == pass; //  operator == has been redefined

#else
	sqlite3_stmt* query = nullptr;
	sqlite3_prepare_v2(database, "SELECT salt FROM USERS WHERE username=? and salt is not NULL", -1, &query, nullptr);
	
	sqlite3_bind_text(query, 1, username.c_str(), username.size(), SQLITE_STATIC);

	int rc = sqlite3_step(query);
	if (rc == SQLITE_ROW) {
		char * text;
		text = (char*)sqlite3_column_text(query, 0);
		salt.append(text);
	}
	else
		std::cout << "could not read salt. returned " << rc << std::endl;

	sqlite3_finalize(query);
	sqlite3_prepare_v2(database, "SELECT password FROM USERS where username =?", -1, &query, nullptr);
	sqlite3_bind_text(query, 1, username.c_str(), username.size(), SQLITE_STATIC);

	if (sqlite3_step(query) != SQLITE_ROW) {
		sqlite3_finalize(query);
		return false;
	}
	
	ea.update(password + salt);

	std::string pass;
	pass += (char*) sqlite3_column_text(query, 0);

	sqlite3_finalize(query);
	return (pass.compare(ea.final()) == 0);
#endif // !PRECOMPILED

	
}

// this function return something like [{"name":"filename1", "path", "filepath1"}, {"name":"filename2", "path", "filepath2"}]
std::string DatabaseHandler::getUserFolder(std::string username, std::string basePath) 
{

	std::string jsonFolder = "[";
#ifndef PRECOMPILED
	std::string query = "SELECT name, path, checksum, lastModified FROM VERSIONS V WHERE username = '" + username + "' AND Blob is not NULL AND lastModified = (\
				SELECT  MAX(lastModified) FROM VERSIONS WHERE username = '" + username + "' AND name = V.name AND path = V.path)";



	std::string* params[2];
	params[0] = &jsonFolder;
	params[1] = &basePath;
	char* error;

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string *folder = ((std::string**)data)[0];
		std::string bPath = *((std::string**)data)[1];
		std::string appendString = "{";
		for (int i = 0; i < argc; i++) {
			if (strcmp(azColName[i], "path") != 0) appendString += ("\"" + std::string(azColName[i]) + "\":\"" + std::string(argv[i]) + "\",");
			else appendString += ("\"" + std::string(azColName[i]) + "\":\"" + bPath + std::string(argv[i]) + "\",");

		}
		if (argc > 0) appendString[appendString.size() - 1] = '}';
		else appendString += "}";
		appendString += ",";
		folder->append(appendString);
		return 0;
	}, params, &error);


	if (error != nullptr) {
		std::cout << "error getUserFolder: " << error << std::endl;
		sqlite3_free(error);
		throw std::exception("error while getting the filesystem for the user ");
	}

	
	
#else

	sqlite3_stmt* query = nullptr;
	sqlite3_prepare_v2(database, "SELECT name, path, checksum, lastModified FROM VERSIONS V WHERE username =? AND Blob is not NULL AND lastModified = (\
				SELECT  MAX(lastModified) FROM VERSIONS WHERE username = ? AND name = V.name AND path = V.path)", -1, &query, nullptr);
	sqlite3_bind_text(query, 1, username.c_str(), username.size(), SQLITE_STATIC);
	sqlite3_bind_text(query, 2, username.c_str(), username.size(), SQLITE_STATIC);
	
	while (sqlite3_step(query) == SQLITE_ROW) {
		jsonFolder += '{';
		for (int i = 0; i < 4; i++) {
			if (std::string(sqlite3_column_name(query, i)).compare("path") != 0) {
				jsonFolder = jsonFolder.append("\"").append(sqlite3_column_name(query, i)).append("\":");
				// ho ottenuto {"name":
				jsonFolder = jsonFolder.append("\"").append((char*)sqlite3_column_text(query, i)).append("\",");
				// ora ho {"name":"value",
			}
			else
			{
				jsonFolder = jsonFolder.append("\"").append(sqlite3_column_name(query, i)).append("\":");
				jsonFolder = jsonFolder.append("\"").append(basePath).append((char*)sqlite3_column_text(query, i)).append("\",");
			}
		}
		if (jsonFolder[jsonFolder.size() - 1] == ',')
			jsonFolder[jsonFolder.size() - 1] = '}';
		else jsonFolder += '}';
		jsonFolder += ',';
	}

	sqlite3_finalize(query);

#endif // !PRECOMPILED

	if (jsonFolder[jsonFolder.size() - 1] == ',')
		jsonFolder[jsonFolder.size() - 1] = ']';
	else
		jsonFolder += ']';

	std::string returnFolder = "";
	for (int i = 0; i < jsonFolder.size(); i++) {
		if (jsonFolder[i] != '\\')
			returnFolder += jsonFolder[i];
		else
			returnFolder += "\\\\";
	}

	return returnFolder;
}


//TODO: CONTINUA A CREARE QUERY PRECOMPILATE DA QUI. 

bool DatabaseHandler::existsFile(std::string username, std::string path, std::string fileName) {
	
	std::string query = "SELECT count(*) FROM FILES where username = '" + username + "' AND path = '" + path +"' AND name = '" + fileName + "'";
	OutputDebugStringA(query.c_str());
	OutputDebugStringA("\n");

	std::cout << "|filename| = " << std::to_string(fileName.length()) << std::endl;

	int number = -1;
	char* error;
	
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &number, &error);

	if (error != nullptr) {
		std::cout << error << std::endl;
		sqlite3_free(error);
		throw std::exception("existsFile has crashed!");
	}
	else
		std::cout << "number = " << std::to_string(number);

	return number == 1;
}

int DatabaseHandler::createFileForUser(std::string username, std::string path, std::string fileName) {
	std::lock_guard<std::mutex> lockguard(m);

	if (this->existsFile(username, path, fileName)) throw std::exception("file already exists");
	sqlite3_exec(database, "BEGIN TRANSACTION", nullptr, nullptr, nullptr);

	std::string query = "INSERT INTO FILES (name, path, username) VALUES ('" + fileName + "', '" + path + "', '" + username + "')";
	char* error;
	if (sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error) == SQLITE_LOCKED)
	{

	}
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
	
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE username = '" + username + "'";
		sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
			*((int*)data) = strtol(argv[0], nullptr, 10) + 1;
			return 0;
		}, &max, &error);
		if (error != nullptr) {
			std::cout << error << std::endl;
			std::cout << "error in select max(blob) : " << error << std::endl;
			sqlite3_free(error);
			sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
			throw std::exception("DbHandler:: createFileForUser-> no select max blob");
		}

	}
	
#ifdef DEBUG
	std::cout << "so max is " << max << std::endl;
#endif // DEBUG

	std::string timestamp;
	time_t t = time(0);
	struct tm now;
	if (EINVAL == localtime_s(&now, &t))
		std::cout << "could not fill tm struct" << std::endl;
	timestamp += std::to_string((now.tm_year + 1900)) + "-" + std::to_string((now.tm_mon + 1)) + '-' + std::to_string(now.tm_mday) + ' ';
	if (now.tm_hour < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_hour) + ":";
	if (now.tm_min < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_min) + ":";
	if (now.tm_sec < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_sec);

#ifdef DEBUG
	std::cout << "timestamp = " << timestamp << std::endl;
#endif


	query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('" + fileName + "', '" + path + "', '" + username + "', '" + timestamp +"', '" + std::to_string(max) +"' )";
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
#ifdef DEBUG
		std::cout << "error in insert blob : " << error << std::endl;
#endif // DEBUG
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: createFileForUser-> no insert blob");
	}

	sqlite3_exec(database, "COMMIT", nullptr, nullptr, nullptr);
	return max;
}

/* 
	This function creates a new blob for a file. since it is only called from createFileForUser, it is wrong to try to get the mutex here
*/
int DatabaseHandler::createNewBlobForFile(std::string username, std::string path, std::string fileName) {
	if (! this->existsFile(username, path, fileName)) throw std::exception("file does not exist");

	int count = 1, max = -1;
	char * error;
	sqlite3_exec(database, "BEGIN EXCLUSIVE TRANSACTION", nullptr, nullptr, nullptr);


	std::string query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '" + username + "'";
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);	
		return 0;
	}, &count, &error);

	if (error != nullptr) {
		std::cout << "error : " << error << std::endl;
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
	timestamp += std::to_string(now.tm_min) + ":";
	if (now.tm_sec < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_sec);

	query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('" + fileName + "', '" + path + "', '" + username + "', '" + timestamp+ "', " + std::to_string(max) + " )";
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);

	if (error != nullptr) {
		std::cout << error << std::endl;
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: createFileForUser-> no insert new blob");
	}

	sqlite3_exec(database, "COMMIT", nullptr, nullptr, nullptr);

	return max;
}

void DatabaseHandler::deleteFile(std::string username, std::string path, std::string filename)
{
	std::lock_guard<std::mutex> lg(m);
	if (!existsFile(username, path, filename)) throw std::exception("no file to delete");
	if(isDeleted(username, path, filename)) throw std::exception("file already deleted");
	
	
	time_t t = time(0); 
	std::string timestamp = "";
	struct tm now;
	if (EINVAL == localtime_s(&now, &t))
		std::cout << "could not fill tm struct" << std::endl;
	timestamp += std::to_string((now.tm_year + 1900)) + "-" + std::to_string((now.tm_mon + 1)) + '-' + std::to_string(now.tm_mday) + ' ';
	if (now.tm_hour < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_hour) + ":";
	if (now.tm_min < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_min) + ":";
	if (now.tm_sec < 10) timestamp += '0';
	timestamp += std::to_string(now.tm_sec);

	std::string query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('" + filename + "', '" + path + "', '" + username + "', '" +timestamp + "', NULL )";

	char* error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: deleteFiles -> error while inserting NULL blob");
	}
}

void DatabaseHandler::addVersion(std::string username, std::string path, std::string filename, std::string lastModified, int blob) 
{
	if (!existsFile(username, path, filename)) throw std::exception("the file does not exist");
	char* error;
	std::string query = "INSERT INTO VERSIONS(name, path, username, lastModified, Blob) VALUES('" + filename + "', '" + path + "', '" + username + "', '" + lastModified + "', " + std::to_string(blob) + " )";
	std::cout << query << std::endl;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);

	if (error != nullptr) {
		std::cout << error << std::endl;
		sqlite3_free(error);
		throw std::exception("DbHandler:: addVersion-> no insert new version");
	}
}

void DatabaseHandler::removeFile(std::string username, std::string path, std::string filename) {
	std::lock_guard<std::mutex> lg(m);
	if (!existsFile(username, path, filename)) { 
#ifdef  DEBUG
		std::cout << "error : file not found" <<std::endl;
#endif //  DEBUG
		throw std::exception("no file to delete"); 
	}
	// the file exist

	char* error;
	sqlite3_exec(database, "BEGIN EXCLUSIVE TRANSACTION", nullptr, nullptr, nullptr);

	std::string query = "DELETE FROM VERSIONS WHERE username = '" + username + "' AND path = '" + path+"' AND name = '"+filename+"'";

#ifdef DEBUG
	OutputDebugStringA(query.c_str());
	OutputDebugStringA("\n");
#endif // DEBUG


	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
#ifdef  DEBUG
		std::cout << "error : " << error << std::endl;
#endif //  DEBUG

		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: removeFile -> error while deleting from FILES");
	}

	query = "DELETE FROM FILES WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "'";

#ifdef DEBUG
	OutputDebugStringA(query.c_str());
	OutputDebugStringA("\n");
#endif // DEBUG


	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
#ifdef  DEBUG
		std::cout << "error : " << error << std::endl;
#endif //  DEBUG
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler:: removeFile -> error while deleting from VERSIONS");
	}

	sqlite3_exec(database, "COMMIT", nullptr, nullptr, nullptr);
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
	std::lock_guard<std::mutex> lg(m);
	std::string query = "UPDATE VERSIONS SET checksum = '" + checksum + "' WHERE Blob = " + std::to_string(blob) + " AND username = '" + username + "'";
	char* error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, &error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK TRANSACTION", nullptr, nullptr, nullptr);
		throw std::exception("DbHandler::addChecksum -> error while inserting");
	}
}

std::string DatabaseHandler::getDeletedFiles(std::string username, std::string basePath)
{
	std::string query = "SELECT path, name FROM VERSIONS V WHERE username = '" + username + "'AND Blob IS NULL AND lastModified = (\
							SELECT MAX(lastModified) FROM VERSIONS WHERE username ='" + username + "' AND name = V.name AND path = V.path)";
	
	std::string result = "[";

	std::string* params[2];
	params[0] = &result;
	params[1] = &basePath;
	char* error;

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string* v = ((std::string**)data)[0];
		std::string base = *((std::string**)data)[1];
		std::string appendString = "{ \"" + std::string(azColName[0]) + "\" : \"" + base + std::string(argv[0]) + "\", \""+std::string(azColName[1])+"\":\""+std::string(argv[1])+"\" },";
		v->append(appendString);
		return 0;
	}, params, &error);

	if (error != nullptr) {
		sqlite3_free(error);
		throw std::exception("DbHandler::getDeletedFiles -> error while selecting deleted files");
	}
	if (result[result.size() - 1] == ',')
		result[result.size() - 1] = ']';
	else
		result += ']';



	std::string ret = "";
	for (int i = 0; i < result.size(); i++) {
		if (result[i] != '\\') ret += result[i];
		else ret += "\\\\";
	}

	return ret;
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

int DatabaseHandler::getLastBlob(std::string username, std::string path, std::string filename) {
	char* error;
	int blob = -1;
	if (!existsFile(username, path, filename)) return -1;
	std::string query = "SELECT Blob FROM VERSIONS WHERE username='" + username + "' AND path='"+path+"' AND name='"+filename+"' AND Blob IS NOT NULL AND lastModified = (\
		SELECT MAX(lastModified) FROM VERSIONS WHERE username='" + username + "' AND path='" + path + "' AND name='" + filename + "' and Blob is not null)";

	OutputDebugStringA(query.c_str()); OutputDebugStringA("\n");

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*(int*)data = strtol(argv[0], nullptr, 10);
		std::cout << azColName[0] << ": " << argv[0];
		return 0;
	}, &blob, &error);

	if (error != nullptr) {
		std::cout << error << std::endl;
		sqlite3_free(error);
		throw std::exception("DbHandler::getPath-> error while searchng for path");
	}
	std::cout << "blob = " << std::to_string(blob) << std::endl;
	return blob;
}

std::string DatabaseHandler::getLastVersion(std::string username, std::string path, std::string filename) {
	char* error;
	std::string version;
	if (!existsFile(username, path, filename)) return "";
	std::string query = "SELECT MAX(lastModified) FROM VERSIONS WHERE username='" + username + "' AND path='" + path + "' AND name='" + filename + "'";

	OutputDebugStringA(query.c_str());
	OutputDebugStringA("\n");
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		((std::string*)data)->append(argv[0]);
		std::cout << azColName[0] << ": " << argv[0];
		return 0;
	}, &version, &error);

	if (error != nullptr) {
		std::cout << error << std::endl;
		sqlite3_free(error);
		throw std::exception("DbHandler::getPath-> error while searchng for path");
	}
	
	return version;
}

std::string DatabaseHandler::getBlobVersion(std::string username, int blob) {
	char* error;
	std::string version = "";
	std::string query = "SELECT lastModified FROM VERSIONS WHERE username='" + username + "' AND Blob = " + std::to_string(blob);
	// blob is unique for each user -> i get just one value of lastModified

	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		((std::string*)data)->append(argv[0]);
		std::cout << azColName[0] << ": " << argv[0];
		return 0;
	}, &version, &error);

	if (error != nullptr) {
		std::cout << error << std::endl;
		sqlite3_free(error);
		throw std::exception("DbHandler::getPath-> error while searchng for path");
	}

	return version;
}

std::string DatabaseHandler::getRandomString(int digitN) {
	char digits[2 * 26 + 10];
	char c = 'a';
	int pos=0;
	while (c <= 'z') digits[pos++] = c++;
	c = 'A';
	while (c <= 'Z') digits[pos++] = c++;
	c = '0';
	while (c <= '9') digits[pos++] = c++;

	// the characters have been inizialized. now i'll build the string

	std::string retStr;
	for (int i = 0; i < digitN; i++) retStr.append(1, digits[rand() % (2 * 26 + 10)]);
	return retStr;
}

std::string DatabaseHandler::secureParameter(std::string parameter) {
	std::string retStr;
	for (char c : parameter) {
		if (c != '\'') retStr += c;
		else retStr += "''";
	}
	return retStr;
}


/*
	NOTE: there is no check about a possible duplicated cookie. 
		  In that case an error will occur and it will be told "impossible to create the cookie, log with username and password also next time"
*/
std::string DatabaseHandler::updateCookie(std::string username)
{
	std::string cookie = getRandomString(100); 
	time_t t = time(0);
	t += 60  // seconds in a minute
		* 60 // minutes in an hour
		* 24; // hours a day

	std::string query = "UPDATE USERS SET cookie = '?', cookieExpiration = ? WHERE username = '?'";
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(database, query.c_str(), query.size(), &stmt, nullptr) != SQLITE_OK) {
		
#ifdef DEBUG	
		std::cout << "error while praparing the query to upate cookie";
#endif
		return nullptr;
	}
	// note: the count of parameters to be set starts from 1
	// with SQLITE STATIC i say that the string does not need to be disposed, so it is oky it does not remove it
	sqlite3_bind_text(stmt, 1, cookie.c_str(), cookie.size(), SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, std::to_string(t).c_str(), std::to_string(t).size(), SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, username.c_str(), username.size(), SQLITE_STATIC);


	std::lock_guard<std::mutex> lg(m); // now it is time to crate the lock since the query is ready to be executed
	
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return nullptr;
	}
	// if done, it means that it has updated out table. yeah!
	return cookie;
}

std::string DatabaseHandler::getUserFromCookie(std::string cookie) {

	std::string query = "SELECT username, cookieExpiration FROM USERS WHERE cookie = '?'";
	sqlite3_stmt* stmt;
	if (sqlite3_prepare_v2(database, query.c_str(), query.size(), &stmt, nullptr) != SQLITE_OK) {
#ifdef DEBUG	
		std::cout << "error while praparing the query to retrive username from cookie";
#endif
		return nullptr;
	}

	sqlite3_bind_text(stmt, 1, cookie.c_str(), cookie.size(), SQLITE_STATIC);

	if (sqlite3_step(stmt) != SQLITE_DONE) { // since cookie is UNIQUE, i can assume it's the last element. if present
		sqlite3_finalize(stmt);
		return nullptr;
	}

	std::string user;
	std::string expires;

	try {
		user += (char*)sqlite3_column_text(stmt, 1);
		expires += (char*)sqlite3_column_text(stmt, 2);
	}
	catch (...) { // i am here if user or expires is null. but dunno the exception
		sqlite3_finalize(stmt);
		return nullptr;
	}

	sqlite3_finalize(stmt);

	if (std::to_string(time(0)).compare(expires) > 0)
		return nullptr;
	else
		return user;	
}
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
	char** error;
	// todo : use precompiled queries or check for avoid SQL INJECTION
	
	sqlite3_exec(database, query.c_str(), [](void* notUsed, int argc, char **argv, char **azColName) ->int {
		// this is the code of the callback
		for (int i = 0; i<argc; i++) {
			printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
		}
		return 0;

	}, NULL, error);
	if (error == nullptr) {
		sqlite3_free(error);
		throw new std::exception("impossible to create the new user");
	}
}

bool DatabaseHandler::logUser(std::string username, std::string password) {

	std::string query = "SELECT password FROM USERS where username = '" + username + "'";
	std::string pass = "password";
	// todo : use precompiled queries or check for avoid SQL INJECTION
	char** error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		((std::string*)data)->assign(argv[0]);
		return 0;
		}, &pass, error);	
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
	char** error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		std::string *folder = (std::string*)data;		
		std::string appendString = "{ \"" + std::string(azColName[0]) + "\":\"" + std::string(argv[0]) + "\", \"" + std::string(azColName[1]) + "\":\"" + std::string(argv[1]) + "\" },";
		folder->append(appendString);
		return 0;
	}, &jsonFolder, error);


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
	char** error;
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		return 0;
	}, &number, error);

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
	char** error;
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, error);
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
	}, &numberOfBlobs, error);
	
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
		}, &max, error);
		if (error != nullptr) {
			sqlite3_free(error);
			sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
			throw new std::exception("DbHandler:: createFileForUser-> no select max blob");
		}

	}
	

	std::string t = std::to_string(time(0)); // current time. should be an int with the time since 1 Jan 1970. compatible with db??
	
	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES(' " + fileName + " ', ' " + path + "', ' " + username + " ', '" +  t +"', " + std::to_string(max) +" )";	
	sqlite3_exec(database, query.c_str(), nullptr, nullptr, error);
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
	char ** error;

	std::string query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '" + username + "')";
	sqlite3_exec(database, query.c_str(), [](void* data, int argc, char **argv, char **azColName)->int {
		*((int*)data) = strtol(argv[0], nullptr, 10);
		
		return 0;
	}, &count, error);
	if (error != nullptr) {
		sqlite3_free(error);
		sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
		throw new std::exception("DbHandler:: createFileForUser-> no select");
	}



	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_INTEGER, // want an int 
		(SQLPOINTER)&count,  // put it here
		0, // is 0, maybe it means "the necessary". // todo: check that
		&d // the length of the int ??
		);
	if (SQLFetch(hStmt) == SQL_NO_DATA) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error: cannot retrive how many files there are for the selected user");
	}

	if (count == 0) max = 0;
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE username = '" + username + "'";

		ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
		if (ret != SQL_SUCCESS) {
			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
			throw new std::exception("could not count blobs for that user");
		}

		SQLBindCol(hStmt, // statement handle
			0, // column number  
			SQL_INTEGER, // want an int 
			(SQLPOINTER)&count,  // put it here
			0, // is 0, maybe it means "the necessary". // todo: check that
			&d // the length of the int ??
			);
		if (SQLFetch(hStmt) == SQL_NO_DATA) {
			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
			throw new std::exception("error: cannot retrive how many files there are for the selected user");
		}

		
		max = count++;
	}
	time_t t = time(0); // current time. should be an int with the time since 1 Jan 1970. compatible with db??

	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES(' " + fileName + " ', ' " + path + "', ' " + username + " ', " + std::to_string(t) + ", " + std::to_string(max) + " )";
	ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("");
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	return max;
}

void DatabaseHandler::deleteFile(std::string username, std::string path, std::string filename)
{
	if (!existsFile(username, path, filename)) throw new std::exception("no file to delete");
	
	SQLHANDLE hStmt;
	int validBlob;
	SQLINTEGER d;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	std::string query = "SELECT COUNT(*) FROM (\
							SELECT Blob FROM VERSION WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "' AND lastUpdate = (\
								SELECT MAX(lastUpdate) FROM VERSION  WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "')\
						) WHERE Blob is not NULL";

	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("could not see if last blob for that file was NULL");
	}

	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_INTEGER, // want an int 
		(SQLPOINTER)&validBlob,  // put it here
		0, // is 0, maybe it means "the necessary". // todo: check that
		&d // the length of the int ??
		);
	if (SQLFetch(hStmt) == SQL_NO_DATA) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error: cannot retrive how many files there are for the selected user");
	}

	if (validBlob == 0) { // means that last updted blob was null
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("trying to delete an already deleted file");
	}
	time_t t = time(0); // current time. should be an int with the time since 1 Jan 1970. compatible with db??
	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES('" + filename + "', '" + path + "', '" + username + "', " + std::to_string(t) + ", NULL )";
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}



void DatabaseHandler::removeFile(std::string username, std::string path, std::string filename) {
	if (!existsFile(username, path, filename)) throw new std::exception("no file to delete");
	// the file exist
	SQLHANDLE hStmt;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");
	
	// 1st : i need to remove all its versions
	std::string query = "DELETE FROM VERSIONS WHERE username = '" + username + "' AND path = '" + path+"' AND name = '"+filename+"'";
	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("Impossible to remove the versions for the file");
	}
	query = "DELETE FROM FILE WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "'";
	ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("Impossible to remove the versions for the file");
	}
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

std::string DatabaseHandler::getFileVersions(std::string username, std::string path, std::string filename)
{
	if (!existsFile(username, path, filename)) throw new std::exception("cannot find the file");
	
	SQLHANDLE hStmt;
	time_t t;
	SQLINTEGER d;
	std::string versions = "{";




	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	std::string query = "SELECT lastUpdate FROM VERSIONS WHERE Blob is not NULL AND username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "' ORDER BY lastUpdate DESC";
	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) return nullptr;

	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_TIME, // want an int 
		(SQLPOINTER)&t,  // put it here
		0, // in the example is 0, maybe it means "the necessary". // todo: check that
		&d // the length of the time ??
		);

	while (SQLFetch(hStmt) != SQL_NO_DATA) 
		versions += ("[" + std::to_string(t) + "]");

	versions += "}";
	
	return versions;
}

void DatabaseHandler::addChecksum(std::string username, int blob, std::string checksum)
{
	SQLHANDLE hStmt;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	std::string query = "UPDATE VERSIONS SET checksum = '" + checksum + "' WHERE Blob = " + std::to_string(blob) + " AND username = '" + username + "'";
	SQLExecDirectA(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
}

std::string DatabaseHandler::getDeletedFiles(std::string username)
{
	SQLHANDLE hStmt;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	std::string query = "SELECT path, name, lastUpdate FROM VERSIONS V WHERE username = '" + username + "'AND Blob IS NULL AND lastUpdate = (\
							SELECT MAX(lastUpdate) FROM VERSIONS WHERE username ='" + username + "' AND name = V.name AND path = V.path)";
	SQLRETURN res = SQLExecDirectA(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
	if (res != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("sql: impossible to retreive deleted files");
	}


	std::string result = "{";
	std::string path, name;
	time_t t;
	SQLINTEGER pathDim, nameDim, timeDim;

	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_VARCHAR, // want an int 
		(SQLPOINTER)path.c_str(),  // put it here
		0, // in the example is 0, maybe it means "the necessary". // todo: check that
		&pathDim // the length of the time ??
		);

	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_VARCHAR, // want an int 
		(SQLPOINTER)name.c_str(),  // put it here
		0, // in the example is 0, maybe it means "the necessary". // todo: check that
		&nameDim // the length of the time ??
		);

	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_TIME, // want an int 
		(SQLPOINTER)&t,  // put it here
		0, // in the example is 0, maybe it means "the necessary". // todo: check that
		&timeDim // the length of the time ??
		);


	while (SQLFetch(hStmt) != SQL_NO_DATA)  result += ("[" + path + name + ",", std::to_string(t) + "]");
	

	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	result += "}";

	return result;
}

int DatabaseHandler::getBlob(std::string username, std::string path, std::string filename, std::string datetime)
{
	if (!existsFile(username, path, filename) || isDeleted(username, path, filename)) return -1;
	SQLHANDLE hStmt;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	std::string query = "SELECT Blob from VERSIONS WHERE username = '"+username+"' AND path = '"+path+"' AND name='"+filename+"' AND lastUpdate="+datetime;
	SQLRETURN res = SQLExecDirectA(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
	if (res != SQL_SUCCESS) throw new std::exception("sql: impossible to retreive deleted files");

	// blob must be only one

	int blob = -1;
	SQLINTEGER d = sizeof(blob);
	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_INTEGER, // want an int 
		(SQLPOINTER)&blob,  // put it here
		0, // in the example is 0, maybe it means "the necessary". // todo: check that
		&d
		);


	SQLFetch(hStmt);
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

	return blob;
}

bool DatabaseHandler::isDeleted(std::string username, std::string path, std::string filename)
{
	SQLHANDLE hStmt;
	int validBlob;
	SQLINTEGER d;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	std::string query = "SELECT COUNT(*) FROM (\
							SELECT Blob FROM VERSION WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "' AND lastUpdate = (\
								SELECT MAX(lastUpdate) FROM VERSION  WHERE username = '" + username + "' AND path = '" + path + "' AND name = '" + filename + "')\
						) WHERE Blob is not NULL";

	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("could not see if last blob for that file was NULL");
	}

	SQLBindCol(hStmt, // statement handle
		0, // column number  
		SQL_INTEGER, // want an int 
		(SQLPOINTER)&validBlob,  // put it here
		0, // is 0, maybe it means "the necessary". // todo: check that
		&d // the length of the int ??
		);
	if (SQLFetch(hStmt) == SQL_NO_DATA) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error: cannot retrive how many files there are for the selected user");
	}

	if (validBlob == 0) { // means that last updted blob was null
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		return true;
	}
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	return false;
}





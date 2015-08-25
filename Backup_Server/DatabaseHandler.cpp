#include "stdafx.h"
#include "DatabaseHandler.h"

#include <windows.security.cryptography.h>
#include <windows.security.cryptography.core.h>
#include <windows.storage.streams.h>

#include <sql.h>
#include <sqlext.h>
#include <sal.h>


DatabaseHandler::DatabaseHandler()
{
	// here i need to connect to the database. We'll use the ODBC driver
	
	// 1) create the enviroment
	if( SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env) == SQL_ERROR )
		throw new std::exception("impossible to create enviroment handle");
	// 2) Register this as an application that expects 3.x behavior
	if(SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0) == SQL_ERROR ) throw new std::exception("registration as ODBC 3 not successfull");
	// 3) allocate the handle for the connection (hdbc)
	SQLAllocHandle(SQL_HANDLE_DBC, env, &hdbc);
	// 4) connect to the driver. The String is the one getted in the properties of the sql server.
	
	if(SQLDriverConnect(hdbc, GetDesktopWindow(), L"Data Source = (localdb)\v11.0; Initial Catalog = master; Integrated Security = True; Connect Timeout = 30; Encrypt = False; TrustServerCertificate = False; ApplicationIntent = ReadWrite; MultiSubnetFailover = False", SQL_NTS, NULL, 0, NULL, SQL_DRIVER_COMPLETE) == SQL_ERROR ) throw new std::exception("impossible to connect to the driver");
	
}


DatabaseHandler::~DatabaseHandler()
{
	// here i need to disconnect from the database

	if (hdbc)
	{
		SQLDisconnect(hdbc);
		SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
	}

	if (env) SQLFreeHandle(SQL_HANDLE_ENV, env);
	
	
}


void DatabaseHandler::registerUser(std::string username, std::string password, std::string baseDir) {
	
	SQLHSTMT hStmt;
	SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt); // Created the handle for a statement. todo: check the result

	/* TODO: HERE I SHOULD CALCULATE AN HASH ON THE PASSWORD
	
	HCRYPTPROV provider;
	LPCTSTR pszContainerName = TEXT("My Sample Key Container");
	if (!CryptAcquireContext(&provider, nullptr, pszContainerName, PROV_RSA_FULL, 0))
		throw new std::exception("Impossible to acquire the cryptographic provider");

	*/
	

	std::string query = "INSERT INTO USERS (username, password, basedir) VALUES ('" + username + "', '" + password + "', '" + baseDir +"')";

	
	SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);

	// todo: todo: check the result
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

void DatabaseHandler::createNewBlobForFile(std::string username, std::string path, std::string fileName) {
	if (! this->existsFile(username, path, fileName)) throw new std::exception("file does not exist");
	std::string query = "SELECT COUNT(*) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = ' " + username + " ')";
	int count = 1, max = -1;
	if (count == 0) max = 0;
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = ' " + username + " ')";
		int queryResult = 33;
		max = queryResult++;
	}
	time_t t = time(0); // current time. should be an int with the time since 1 Jan 1970. compatible with db??

	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES(' " + fileName + " ', ' " + path + "', ' " + username + " ', " + std::to_string(t) + ", " + std::to_string(max) + " )";

}


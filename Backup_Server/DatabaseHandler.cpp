#include "stdafx.h"
#include "DatabaseHandler.h"


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
	// 4) connect to the driver. The String is the one getten from the properties of the sql server.
	
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

/*
	Function Name: registerUser
	this function could throw std::exception. read the message to understand what happened
*/
void DatabaseHandler::registerUser(std::string username, std::string password, std::string baseDir) {
	
	SQLHSTMT hStmt;
	if  (
		 SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	
	

	std::string query = "INSERT INTO USERS (username, password, basedir) VALUES ('" + username + "', '" + password + "', '" + baseDir +"')";
	// todo : use precompiled queries or check for avoid SQL INJECTION

	
	SQLRETURN ret =  SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

	if (ret != SQL_SUCCESS) // we can call SQLGetDiagRec to get a SQLSTATE where there is a code about what kind of error has occurred
		throw new std::exception("error while executing the query");
	
	
	
}

bool DatabaseHandler::logUser(std::string username, std::string password) {

	SQLHSTMT hStmt;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");


	std::string query = "SELECT password FROM USERS where username = ' " + username + " '";
	// todo : use precompiled queries or check for avoid SQL INJECTION

	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error while retriving the password from the user");
	}

	std::string pass = "password";
	int dimension;

	SQLBindCol(hStmt, // statement handle
		1, // column number
		SQL_C_TCHAR, // want a string
		(SQLPOINTER)pass.c_str(),  // put it here
		50, // max dim of the string. in the example is 0, maybe it means "the necessary". // todo: check that
		(SQLINTEGER*)&dimension // the length of the string 
		);

	if ( SQLFetch(hStmt) == SQL_NO_DATA ) { // this puts into pass the password. if the result is empty there is no error but i get SQL_NO_DATA						
			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
			return false;
	}


	
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt); // this also closes the cursor
	// todo: calculate md5 (or sha1) on password
	return pass == password; //  operator == has been redefined
	
}

bool DatabaseHandler::existsFile(std::string username, std::string path, std::string fileName) {
	SQLHANDLE hStmt;
	int number;
	SQLINTEGER d;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");

	std::string query = "SELECT count(*) FROM FILES where username = ' " + username + " ' AND path = '" + path +"' AND name = ' " + fileName + " '";

	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error while checking if the user has the file requested");
	}

	SQLBindCol(hStmt, // statement handle
		1, // column number. // TODO: Modify it (the table has not been created yet)
		SQL_INTEGER, // want an int 
		(SQLPOINTER)&number,  // put it here
		0, // is 0, maybe it means "the necessary". // todo: check that
		&d // the length of the int ??
		);

	

	if (SQLFetch(hStmt) == SQL_NO_DATA)
		number = 0; // so i'll ret false
	
	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
	return number == 1;
}

void DatabaseHandler::createFileForUser(std::string username, std::string path, std::string fileName) {
	
	if (this->existsFile(username, path, fileName)) throw new std::exception("file already exists");

	SQLHANDLE hStmt;
	int count = 1, max = -1;
	SQLINTEGER d;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");



	std::string query = "INSERT INTO FILES (name, path, username) VALUES (' " + fileName + " ', ' " + path + "', ' " + username + " ')";
	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error while inserting the file");
	}

	
	// now i need to create the blob
	// how many blobs can i count for that file?
	query = "SELECT COUNT(*) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = '" + username + "')";
	// todo: maybe this is wrong and i should count the blobs for the user.	query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '" + username + "')";

	ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error while counting the file");
	}


	// TODO: create the file and create the new Blob should be in the same transaction. modify that


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
	SQLCloseCursor(hStmt);
	if (count == 0) max = 0;
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = '" + username + "')"; 
		// todo: myght be wrong. blob should be independent from the file, but depend only from the user!!  
		// query = SELECT MAX(Blob) FROM VERSIONS WHERE username = '" + username + "')";
		// todo: should we add	where Blob not null ??
		ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
		if (ret != SQL_SUCCESS) {
			SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
			throw new std::exception("error while searching for the max blob");
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
	
	query = "INSERT INTO VERSIONS(name, path, username, lastUpdate, Blob) VALUES(' " + fileName + " ', ' " + path + "', ' " + username + " ', " +  std::to_string(t) +", " + std::to_string(max) +" )";
	ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
	if (ret != SQL_SUCCESS) {
		SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
		throw new std::exception("error while searching for the max blob");
	}

	SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
}

void DatabaseHandler::createNewBlobForFile(std::string username, std::string path, std::string fileName) {
	if (! this->existsFile(username, path, fileName)) throw new std::exception("file does not exist");

	SQLHANDLE hStmt;
	int count = 1, max = -1;
	SQLINTEGER d;
	if (
		SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hStmt) // Created the handle for a statement.
		)  throw new std::exception("impossible to create a statement handle");


	std::string query = "SELECT COUNT(*) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = '" + username + "')";
	// query = "SELECT COUNT(*) FROM VERSIONS WHERE username = '" + username + "')";
	SQLRETURN ret = SQLExecDirect(hStmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
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

	if (count == 0) max = 0;
	else {
		query = "SELECT MAX(Blob) FROM VERSIONS WHERE name='" + fileName + " ' AND path =' " + path + "' AND username = '" + username + "')";
		// todo: query = "SELECT MAX(Blob) FROM VERSIONS WHERE username = '" + username + "')";

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
}


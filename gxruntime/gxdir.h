
#ifndef GXDIR_H
#define GXDIR_H

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

class gxDir{
public:
#ifdef _WIN32
	gxDir( HANDLE h,const WIN32_FIND_DATA &f );
#endif
	~gxDir();

private:
#ifdef _WIN32
	HANDLE handle;
	WIN32_FIND_DATA findData;
#endif

	/***** GX INTERFACE *****/
public:
	std::string getNextFile();
};

#endif
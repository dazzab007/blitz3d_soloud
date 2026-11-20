
#ifndef GXDIR_H
#define GXDIR_H

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

class gxDir{
public:
#ifdef _WIN32
	gxDir( HANDLE h,const WIN32_FIND_DATA &f );
#else
	gxDir( const std::vector<std::string> &entries );	//snapshot of a directory (POSIX)
#endif
	~gxDir();

private:
#ifdef _WIN32
	HANDLE handle;
	WIN32_FIND_DATA findData;
#else
	std::vector<std::string> names;
	size_t pos;
#endif

	/***** GX INTERFACE *****/
public:
	std::string getNextFile();
};

#endif
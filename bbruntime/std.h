
#ifndef STD_H
#define STD_H

#ifdef _WIN32
//#ifndef _WINSOCKAPI_
//#define _WINSOCKAPI_   /* Prevent inclusion of winsock.h in windows.h */
//#endif
#include <windows.h>
//#include <winsock2.h>
#endif

#include "../config/config.h"
#include "../stdutil/stdutil.h"

#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <math.h>

using namespace std;

#endif

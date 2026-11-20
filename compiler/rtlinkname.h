//The module symbol a runtime registration is linked under, without declaring
//anything to the front end (so the stand-alone launcher can build its symbol
//table without the compiler). Must match rtlinkDeclare() in rtlink.cpp:
//"_"+sym for internals, "_f"+lowercase command name for BASIC commands.
#ifndef RTLINKNAME_H
#define RTLINKNAME_H

#include <string>
#include <cctype>

inline std::string rtlinkName( const std::string &sym ){
	if( sym[0]=='_' ) return "_"+sym;
	std::string s=sym;
	if( s[0]=='!' ) s=s.substr( 1 );
	size_t start=isalpha( (unsigned char)s[0] ) ? 0 : 1;
	size_t k=start+1;
	while( k<s.size() && (isalnum( (unsigned char)s[k] ) || s[k]=='_') ) ++k;
	std::string n=s.substr( start,k-start );
	for( size_t i=0;i<n.size();++i ) n[i]=(char)tolower( (unsigned char)n[i] );
	return "_f"+n;
}

#endif

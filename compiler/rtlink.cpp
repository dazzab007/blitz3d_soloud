#include "rtlink.h"
#include <fstream>
#include <cstdio>

static Type *rtType( int c ){
	switch( c ){
	case '%':return Type::int_type;
	case '#':return Type::float_type;
	case '$':return Type::string_type;
	}
	return Type::void_type;
}

string rtlinkDeclare( const string &sym,Environ *runtimeEnviron,vector<string> *keywords ){

	string s( sym );

	//internal?
	if( s[0]=='_' ) return "_"+s;

	bool cfunc=false;
	if( s[0]=='!' ){
		cfunc=true;
		s=s.substr( 1 );
	}

	if( keywords ) keywords->push_back( s );

	int start=0,end,k;
	Type *t=Type::void_type;
	if( !isalpha( s[0] ) ){ start=1;t=rtType( s[0] ); }
	for( k=1;k<(int)s.size();++k ){
		if( !isalnum( s[k] ) && s[k]!='_' ) break;
	}
	end=k;
	DeclSeq *params=d_new DeclSeq();
	string n=s.substr( start,end-start );
	while( k<(int)s.size() ){
		Type *pt=rtType( s[k++] );
		int from=k;
		for( ;isalnum( s[k] ) || s[k]=='_';++k ){}
		string str=s.substr( from,k-from );
		ConstType *defType=0;
		if( s[k]=='=' ){
			int from=++k;
			if( s[k]=='\"' ){
				for( ++k;s[k]!='\"';++k ){}
				string d=s.substr( from+1,k-from-1 );
				defType=d_new ConstType( d );++k;
			}else{
				if( s[k]=='-' ) ++k;
				for( ;isdigit( s[k] );++k ){}
				if( pt==Type::int_type ){
					defType=d_new ConstType( atoi( s.substr( from,k-from ).c_str() ) );
				}else{
					defType=d_new ConstType( (float)atof( s.substr( from,k-from ).c_str() ) );
				}
			}
		}
		params->insertDecl( str,pt,DECL_PARAM,defType );
	}

	FuncType *f=d_new FuncType( t,params,false,cfunc );
	n=tolower( n );
	runtimeEnviron->funcDecls->insertDecl( n,f,DECL_FUNC );
	return "_f"+n;
}

/////////////////
// Userlibs    //
/////////////////
#include <dirent.h>
#include <algorithm>

namespace{
struct DeclsLexer{
	istream &in;
	int curr;
	string text;
	DeclsLexer( istream &i ):in(i),curr(0){}

	int next(){
		text="";
		int t=0;
		for(;;){
			while( isspace( in.peek() ) ) in.get();
			if( in.eof() ) return curr=0;
			t=in.get();
			if( t!=';' ) break;
			while( !in.eof() && in.get()!='\n' ){}
		}
		if( isalpha( t ) ){
			text+=(char)t;
			while( isalnum( in.peek() ) || in.peek()=='_' ) text+=(char)in.get();
			return curr=-1;
		}
		if( t=='\"' ){
			while( !in.eof() && in.peek()!='\"' ) text=text+(char)in.get();
			in.get();
			return curr=-2;
		}
		return curr=t;
	}
};
}

const char *rtlinkLoadUserLib( const string &declsFile,Environ *runtimeEnviron,
	vector<string> *keywords,vector<UserFunc> &userFuncs,set<string> &seen ){

	string lib="";
	ifstream in( declsFile.c_str() );
	if( !in ) return "unable to open file";
	DeclsLexer lx( in );

	lx.next();
	while( lx.curr ){

		if( lx.curr=='.' ){

			if( lx.next()!=-1 ) return "expecting identifier after '.'";

			if( lx.text=="lib" ){
				if( lx.next()!=-2 ) return "expecting string after lib directive";
				lib=lx.text;
			}else{
				return "unknown decl directive";
			}
			lx.next();

		}else if( lx.curr==-1 ){

			if( !lib.size() ) return "function decl without lib directive";

			string id=lx.text;
			string lower_id=tolower( id );

			if( seen.count( lower_id ) ) return "duplicate identifier";
			seen.insert( lower_id );

			Type *ty=0;
			switch( lx.next() ){
			case '%':ty=Type::int_type;break;
			case '#':ty=Type::float_type;break;
			case '$':ty=Type::string_type;break;
			}
			if( ty ) lx.next();
			else ty=Type::void_type;

			DeclSeq *params=d_new DeclSeq();

			if( lx.curr!='(' ) return "expecting '(' after function identifier";
			lx.next();
			if( lx.curr!=')' ){
				for(;;){
					if( lx.curr!=-1 ) break;
					string arg=lx.text;

					Type *pt=0;
					switch( lx.next() ){
					case '%':pt=Type::int_type;break;
					case '#':pt=Type::float_type;break;
					case '$':pt=Type::string_type;break;
					case '*':pt=Type::null_type;break;
					}
					if( pt ) lx.next();
					else pt=Type::int_type;

					params->insertDecl( arg,pt,DECL_PARAM,0 );

					if( lx.curr!=',' ) break;
					lx.next();
				}
			}
			if( lx.curr!=')' ) return "expecting ')' after function decl";

			if( keywords ) keywords->push_back( id );

			FuncType *fn=d_new FuncType( ty,params,true,true );
			runtimeEnviron->funcDecls->insertDecl( lower_id,fn,DECL_FUNC );

			if( lx.next()==':' ){	//real name?
				lx.next();
				if( lx.curr!=-1 && lx.curr!=-2 ) return "expecting identifier or string after alias";
				id=lx.text;
				lx.next();
			}

			userFuncs.push_back( UserFunc( lower_id,id,lib ) );
		}else{
			return "syntax error";
		}
	}
	return 0;
}

const char *rtlinkLoadUserLibs( const string &dir,Environ *runtimeEnviron,
	vector<string> *keywords,vector<UserFunc> &userFuncs ){

	DIR *d=opendir( dir.c_str() );
	if( !d ) return 0;
	vector<string> files;
	while( struct dirent *e=readdir( d ) ){
		string n=e->d_name;
		if( n.size()>6 && n.substr( n.size()-6 )==".decls" ) files.push_back( n );
	}
	closedir( d );
	sort( files.begin(),files.end() );	//deterministic order

	set<string> seen;
	static char buf[512];
	for( size_t k=0;k<files.size();++k ){
		if( const char *err=rtlinkLoadUserLib( dir+"/"+files[k],runtimeEnviron,keywords,userFuncs,seen ) ){
			snprintf( buf,sizeof(buf),"Error in userlib '%s' - %s",files[k].c_str(),err );
			return buf;
		}
	}
	return 0;
}

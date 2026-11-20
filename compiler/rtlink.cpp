#include "rtlink.h"

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

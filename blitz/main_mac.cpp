//blitzcc for macOS (x86-64): compile a .bb file and run it in-process.
//
//Windows blitzcc loads linker.dll/runtime.dll and assembles x86-32 text through
//assem_x86. This one drives the same front end into Codegen_x64, loads the code
//with the x86-64 module loader (linker/x64module.cpp) and runs it under the
//console host (bbruntime/bbhost.cpp). Creating a stand-alone executable (-o) is
//a later step.

#include "../compiler/std.h"	//must come first: front-end headers use bare string/istream
#include "../compiler/environ.h"
#include "../compiler/parser.h"
#include "../compiler/prognode.h"
#include "../compiler/rtlink.h"
#include "../compiler/codegen_x64/codegen_x64.h"
#include "../linker/x64module.h"
#include "../linker/x64export.h"
#include "../linker/x64exe.h"
#include "../bbruntime/bbhost.h"

#include <unistd.h>
#include <iomanip>

static const char *VERSION_TEXT="BlitzCC (macOS x86-64 port)";

static Environ *runtimeEnviron;
static std::map<std::string,void*> runtimeSyms;
static std::vector<std::string> keyWords;

static void collect( const char *sym,void *pc ){
	string link=rtlinkDeclare( sym,runtimeEnviron,&keyWords );
	runtimeSyms[link]=pc;
}

static void showUsage(){
	cout<<"Usage: blitzcc [-h|-q|+q|-c|-d|-k|+k|-v] sourcefile.bb [args]"<<endl;
}

static void showHelp(){
	showUsage();
	cout<<"-h         : show this help"<<endl;
	cout<<"-q         : quiet mode"<<endl;
	cout<<"+q         : very quiet mode"<<endl;
	cout<<"-c         : compile only"<<endl;
	cout<<"-d         : debug compile"<<endl;
	cout<<"-k         : dump keywords"<<endl;
	cout<<"+k         : dump keywords and syntax"<<endl;
	cout<<"-v         : version info"<<endl;
	cout<<"-o exefile : generate a stand-alone executable"<<endl;
}

static void err( const string &t ){
	cout<<t<<endl;
	exit( -1 );
}

static string quickHelp( const string &kw ){
	Decl *d=runtimeEnviron->funcDecls->findDecl( tolower( kw ) );
	if( !d || d->type->funcType()==0 ) return "No quick help available for "+kw;
	string t=kw;
	FuncType *f=d->type->funcType();
	if( f->returnType==Type::float_type ) t+='#';
	else if( f->returnType==Type::string_type ) t+='$';
	t+=" ";
	if( f->returnType!=Type::void_type ) t+="( ";
	for( int k=0;k<f->params->size();++k ){
		string s;
		if( k ) s+=',';
		Decl *p=f->params->decls[k];s+=p->name;
		if( p->type==Type::float_type ) s+='#';
		else if( p->type==Type::string_type ) s+='$';
		else if( p->type==Type::void_type ) s+='*';
		if( p->defType ) s='['+s+']';
		t+=s;
	}
	if( f->returnType!=Type::void_type ) t+=f->params->size() ? " )" : ")";
	return t;
}

static void dumpKeys( bool help ){
	map<string,int> &keywords=Toker::getKeywords();
	for( map<string,int>::iterator it=keywords.begin();it!=keywords.end();++it ){
		if( it->first.find( ' ' )!=string::npos ) continue;
		cout<<it->first<<endl;
	}
	for( size_t k=0;k<keyWords.size();++k ){
		string t=keyWords[k];
		if( t[0]=='_' ) continue;
		if( !isalpha( t[0] ) ) t=t.substr( 1 );
		for( size_t n=0;n<t.size();++n ){
			if( !isalnum( t[n] ) && t[n]!='_' ){ t=t.substr( 0,n );break; }
		}
		if( help ) t=quickHelp( t );
		cout<<t<<endl;
	}
}

int main( int argc,char *argv[] ){

	string in_file,out_file,args;
	bool debug=false,quiet=false,veryquiet=false,compileonly=false;
	bool dumpkeys=false,dumphelp=false,showhelp=false,versinfo=false;

	for( int k=1;k<argc;++k ){
		string t=tolower( string( argv[k] ) );
		if( t=="-h" ) showhelp=true;
		else if( t=="-q" ) quiet=true;
		else if( t=="+q" ) quiet=veryquiet=true;
		else if( t=="-c" ) compileonly=true;
		else if( t=="-d" ) debug=true;
		else if( t=="-k" ) dumpkeys=true;
		else if( t=="+k" ) dumpkeys=dumphelp=true;
		else if( t=="-v" ) versinfo=true;
		else if( t=="-o" ){
			if( out_file.size() || k==argc-1 ) err( "Usage error" );
			out_file=argv[++k];
		}else{
			if( in_file.size() || t[0]=='-' || t[0]=='+' ) err( "Usage error" );
			in_file=argv[k];
			for( ++k;k<argc;++k ){
				string a=argv[k];
				if( a.find( ' ' )!=string::npos ) a='\"'+a+'\"';
				if( args.size() ) args+=' ';
				args+=a;
			}
		}
	}

	if( out_file.size() && !in_file.size() ) err( "Usage error" );

	//declare the runtime to the front end and collect the symbol addresses
	runtimeEnviron=d_new Environ( "",Type::int_type,0,0 );
	bbhost_link( collect );

	if( showhelp ) showHelp();
	if( dumpkeys ) dumpKeys( dumphelp );
	if( versinfo ) cout<<VERSION_TEXT<<endl;
	if( !in_file.size() ) return 0;

	ifstream in( in_file.c_str() );
	if( !in ) err( "Unable to open input file" );
	if( !quiet ){
		cout<<VERSION_TEXT<<endl;
		cout<<"Compiling \""<<in_file<<"\""<<endl;
	}

	//run from the source's directory, as the Windows blitzcc does
	size_t n=in_file.rfind( '/' );
	if( n!=string::npos ) chdir( in_file.substr( 0,n ? n : 1 ).c_str() );

	ProgNode *prog=0;
	Environ *environ=0;
	asmjit::CodeHolder holder;
	holder.init( asmjit::Environment::host() );
	Codegen_x64 codegen( &holder,debug );

	try{
		if( !veryquiet ) cout<<"Parsing..."<<endl;
		Toker toker( in );
		Parser parser( toker );
		prog=parser.parse( in_file );

		if( !veryquiet ) cout<<"Generating..."<<endl;
		environ=prog->semant( runtimeEnviron );

		if( !veryquiet ) cout<<"Translating..."<<endl;
		vector<UserFunc> userFuncs;
		prog->translate( &codegen,userFuncs );
		codegen.flush();
	}catch( Ex &x ){
		string file='\"'+x.file+'\"';
		int row=((x.pos>>16)&65535)+1,col=(x.pos&65535)+1;
		cout<<file<<":"<<row<<":"<<col<<":"<<row<<":"<<col<<":"<<x.ex<<endl;
		exit( -1 );
	}
	delete prog;

	if( !veryquiet ) cout<<"Linking..."<<endl;
	X64Module module;
	if( !x64Export( holder,codegen,module ) ) err( "Code generation failed" );
	if( out_file.size() ){
		//the launcher (blitzrun) lives next to blitzcc; the program is appended to a copy of it
		string self=x64SelfPath();
		size_t slash=self.rfind( '/' );
		string launcher=(slash==string::npos ? string( "." ) : self.substr( 0,slash ))+"/blitzrun";
		if( !veryquiet ) cout<<"Creating executable \""<<out_file<<"\"..."<<endl;
		string why;
		if( !x64WriteExe( launcher,out_file,module,debug ? X64EXE_DEBUG : 0,why ) ){
			err( "Error creating executable: "+why );
		}
		return 0;
	}
	if( compileonly ) return 0;

	X64Loader loader;
	if( !loader.load( module,runtimeSyms ) ) err( "Linking failed" );
	void (*entry)()=(void(*)())loader.symbol( module,"__MAIN" );
	if( !entry ) err( "Program has no entry point" );

	const char *e=0;
	if( !bbhost_create( debug,&e ) ) err( string( "Startup Error: " )+e );
	if( !veryquiet ) cout<<"Executing..."<<endl;
	cout.flush();

	const char *rtErr=bbhost_run( entry );

	bbhost_destroy();
	loader.free();
	delete environ;

	if( rtErr ){
		cerr<<"Runtime Error: "<<rtErr<<endl;
		return 1;
	}
	return 0;
}

//Test: emit mov eax,42; ret through the harness, run it, get 42.
//(commit 4, x64-plan.md section 5)

#include <cstdio>
#include <cstdint>
#include "harness.h"

static int tests_run=0;
static int tests_failed=0;

#define CHECK(cond) do{ \
	++tests_run; \
	if( !(cond) ){ \
		++tests_failed; \
		printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#cond); \
	} \
}while(0)

int main(){
	//Build a module with mov eax,42; ret
	X64Module mod;

	//mov eax,42  = B8 2A 00 00 00
	mod.emit( 0xB8 );	//mov eax, imm32
	mod.emit32( 42 );	//42 = 0x2A

	//ret         = C3
	mod.emit( 0xC3 );

	CHECK( mod.size()==6 );

	//Load and run.
	X64Loader loader;
	std::map<std::string,void*> stubs;	//empty: no relocs needed
	void *fn=loader.load( mod,stubs );
	CHECK( fn!=nullptr );

	if( fn ){
		int (*func)()=(int(*)())fn;
		int result=func();
		printf("result=%d\n",result);
		CHECK( result==42 );
	}

	loader.free();

	printf("test_codegen_x64: %d checks, %d failed\n",tests_run,tests_failed);
	return tests_failed?1:0;
}

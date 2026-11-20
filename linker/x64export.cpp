#include "x64export.h"
#include <algorithm>

bool x64Export( asmjit::CodeHolder &holder,Codegen_x64 &cg,X64Module &mod ){
	if( !cg.ok() ) return false;
	asmjit::CodeBuffer &buf=holder.text_section()->buffer();
	std::vector<std::pair<string,int> > syms=cg.symbols();
	std::vector<std::pair<int,string> > byOff;
	for( size_t i=0;i<syms.size();++i ) byOff.push_back( std::make_pair( syms[i].second,syms[i].first ) );
	std::sort( byOff.begin(),byOff.end() );
	int at=0;
	for( size_t i=0;i<byOff.size();++i ){
		mod.emitBytes( buf.data()+at,byOff[i].first-at );
		at=byOff[i].first;
		mod.addSymbol( byOff[i].second.c_str() );
	}
	mod.emitBytes( buf.data()+at,(int)buf.size()-at );

	//data section: its own pages so it stays writable
	const std::vector<unsigned char> &d=cg.data();
	int dataBase=0;
	if( !d.empty() ){
		mod.beginData();
		dataBase=mod.getPC();
		mod.emitBytes( &d[0],(int)d.size() );
		const std::vector<std::pair<string,int> > &ds=cg.dataSymbols();
		for( size_t i=0;i<ds.size();++i ) mod.addDataSymbol( ds[i].first.c_str(),dataBase+ds[i].second );
		const std::vector<Codegen_x64::Fixup> &df=cg.dataFixups();
		for( size_t i=0;i<df.size();++i ) mod.addReloc( df[i].sym.c_str(),dataBase+df[i].offset,ABS64 );
	}

	const std::vector<Codegen_x64::Fixup> &fx=cg.fixups();
	for( size_t i=0;i<fx.size();++i ){
		RelocKind k=fx[i].kind==Codegen_x64::FIX_ABS64 ? ABS64 :
			(fx[i].kind==Codegen_x64::FIX_REL32 ? REL32 : ABS32);
		mod.addReloc( fx[i].sym.c_str(),fx[i].offset,k );
	}
	return true;
}

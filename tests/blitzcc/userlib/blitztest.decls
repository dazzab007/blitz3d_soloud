; test userlib for tests/blitzcc (copied to $blitzpath/userlibs by run.sh)
.lib "libblitztestlib.dylib"

Add%( a%, b% ) : "tl_add"
Scale#( x#, k# ) : "tl_scale"
MixU#( a%, b#, c%, d# ) : "tl_mix"
Sum9%( a%,b%,c%,d%,e%,f%,g%,h%,i% ) : "tl_sum9"
Greet$( name$ ) : "tl_greet"
SumBytes%( data*, n% ) : "tl_sumbytes"
FillBytes( data*, n%, v% ) : "tl_fill"
ObjSum%( obj* ) : "tl_objsum"
Missing%( a% ) : "tl_does_not_exist"

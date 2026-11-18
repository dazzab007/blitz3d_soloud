#ifndef BB_ABI_H
#define BB_ABI_H

//BASIC ABI constants shared by the compiler front end and the runtime.
//On Windows (x86-32) every slot is 4 bytes; on x86-64 every BASIC value
//(int, float, pointer) occupies one 8-byte slot so pointers fit.
#ifdef _WIN32
#define BB_SLOT 4
#else
#define BB_SLOT 8
#endif

//BBArray header layout (basic.h): data ptr, elementType, dims, scales[1].
#define BB_ARRAY_DATA_OFF	0
#define BB_ARRAY_DIMS_OFF	(2*BB_SLOT)
#define BB_ARRAY_SCALE_OFF(k)	(BB_ARRAY_DIMS_OFF + (k+1)*BB_SLOT)

//BBObj field stride: fields are a 1-ptr-aligned array of BBField.
#define BB_FIELD_STRIDE		BB_SLOT

#endif

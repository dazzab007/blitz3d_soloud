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

//BBArray header layout follows the real struct in basic.h:
//{void *data; int elementType,dims,scales[1];}
//The data pointer is BB_PTR wide; elementType/dims/scales are 32-bit ints.
#ifdef _WIN32
#define BB_PTR 4
#else
#define BB_PTR 8
#endif

#define BB_ARRAY_DATA_OFF	0
#define BB_ARRAY_DIMS_OFF	(BB_PTR+4)
#define BB_ARRAY_SCALE_OFF(k)	(BB_PTR+8+(k)*4)

//BBObj field stride: fields are a 1-ptr-aligned array of BBField.
#define BB_FIELD_STRIDE		BB_SLOT

#endif

// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __LONGARITH_H__
#define __LONGARITH_H__

// multiply two 32-bit numbers, yielding a 64-bit temporary result,
// then divide by another 32-bit number
DWORD UMulDiv(DWORD a,DWORD b,DWORD c);

// multiply a 64-bit number by a 32-bit number, yielding a 96-bit
// temporary result, then divide by a 32-bit number
ULONGLONG ULongMulDiv(ULONGLONG a,DWORD b,DWORD c);

#endif
// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __UTIL_H__
#define __UTIL_H__

// ---- logging functions
void initLog();
void closeLog();
void printLog(const char *format,...);

// ---- vtable patching
PBYTE DetourCOM(IUnknown *obj,int vtableOffs,PBYTE newFunction);

// ---- long integer arithmetic
// multiply two 32-bit numbers, yielding a 64-bit temporary result,
// then divide by another 32-bit number
DWORD UMulDiv(DWORD a,DWORD b,DWORD c);

// multiply a 64-bit number by a 32-bit number, yielding a 96-bit
// temporary result, then divide by a 32-bit number
ULONGLONG ULongMulDiv(ULONGLONG a,DWORD b,DWORD c);

#endif
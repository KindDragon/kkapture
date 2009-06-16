// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __VTBL_PATCH_H__
#define __VTBL_PATCH_H__

PBYTE DetourCOM(IUnknown *obj,int vtableOffs,PBYTE newFunction);

#endif
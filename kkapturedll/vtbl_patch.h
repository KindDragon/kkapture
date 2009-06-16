// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __VTBL_PATCH_H__
#define __VTBL_PATCH_H__

void unprotectVTable(IUnknown *obj,int nVirtualFuncs);
PBYTE patchVTable(IUnknown *obj,int vtableOffs,PBYTE newValue);

#endif
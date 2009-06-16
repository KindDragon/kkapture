// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"

void unprotectVTable(IUnknown *obj,int nVirtualFuncs)
{
  PBYTE **vtblPtr = (PBYTE **) obj;
  DWORD oldProtect;
  BOOL res = VirtualProtect(*vtblPtr,nVirtualFuncs * sizeof(PBYTE),PAGE_EXECUTE_READWRITE,&oldProtect);

  if(!res)
    printLog("vtbl: VirtualProtect failed\n");
}

PBYTE patchVTable(IUnknown *obj,int vtableOffs,PBYTE newValue)
{
  PBYTE **vtblPtr = (PBYTE **) obj;
  PBYTE *vtbl = *vtblPtr;
  PBYTE old = vtbl[vtableOffs];

  vtbl[vtableOffs] = newValue;
  return old;
}
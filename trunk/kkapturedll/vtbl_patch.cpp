// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"

PBYTE DetourCOM(IUnknown *obj,int vtableOffs,PBYTE newFunction)
{
  PBYTE **vtblPtr = (PBYTE **) obj;
  PBYTE *vtbl = *vtblPtr;
  PBYTE target = vtbl[vtableOffs];
  
  return DetourFunction(target,newFunction);
}
// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "longarith.h"

DWORD UMulDiv(DWORD a,DWORD b,DWORD c)
{
  __asm
  {
    mov   eax, [a];
    mul   [b];
    div   [c];
    mov   [a], eax;
  }

  return a;
}

ULONGLONG ULongMulDiv(ULONGLONG a,DWORD b,DWORD c)
{
  __asm
  {
    mov   eax, dword ptr [a+4];
    mul   [b];
    mov   ecx, edx;             // ecx=top 32 bits
    mov   ebx, eax;             // ebx=middle 32 bits
    mov   eax, dword ptr [a];
    mul   [b];
    add   ebx, edx;

    // now eax=lower 32 bits, ebx=middle 32 bits, ecx=top 32 bits
    mov   edx, ecx;
    xchg  eax, ebx;
    div   [c];                  // high divide
    mov   dword ptr [a+4], eax;
    mov   eax, ebx;
    div   [c];                  // low divide
    mov   dword ptr [a], eax;
  }

  return a;
}
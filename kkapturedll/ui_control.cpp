// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"

// trampolines
/*DETOUR_TRAMPOLINE(INT_PTR __stdcall Real_DialogBox(HINSTANCE hInstance,LPCTSTR lpTemplate,HWND hWndParent,DLGPROC lpDialogFunc), DialogBox);

INT_PTR __stdcall Mine_DialogBox(HINSTANCE hInstance,LPCTSTR lpTemplate,HWND hWndParent,DLGPROC lpDialogFunc)
{
  return Real_DialogBox(hInstace,lpTemplate,hWndParent,lpDialogFunc);
}
*/

void initUIControl()
{
}

void doneUIControl()
{
}
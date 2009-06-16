// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "video.h"
#include "videoencoder.h"

#include <gl/gl.h>
#pragma comment(lib,"opengl32.lib")

static void captureGLFrame()
{
  if(captureData && encoder && params.CaptureVideo)
  {
    // blit data into capture buffer
    glReadPixels(0,0,captureWidth,captureHeight,GL_BGR_EXT,GL_UNSIGNED_BYTE,captureData);

    // encode
    encoder->WriteFrame(captureData);
  }
}

static void initGLFromDC(HDC hdc)
{
  // get client rect of window
  HWND wnd = WindowFromDC(hdc);
  RECT rc;

  if(wnd)
  {
    GetClientRect(wnd,&rc);
    setCaptureResolution(rc.right-rc.left,rc.bottom-rc.top);
  }
}

// trampolines

// newer wingdi.h versions don't declare this anymore
extern "C" BOOL __stdcall wglSwapBuffers(HDC hdc);

DETOUR_TRAMPOLINE(LONG __stdcall Real_ChangeDisplaySettingsEx(LPCTSTR lpszDeviceName,LPDEVMODE lpDevMode,HWND hwnd,DWORD dwflags,LPVOID lParam), ChangeDisplaySettingsEx);
DETOUR_TRAMPOLINE(HGLRC __stdcall Real_wglCreateContext(HDC hdc), wglCreateContext);
DETOUR_TRAMPOLINE(HGLRC __stdcall Real_wglCreateLayerContext(HDC hdc,int iLayerPlane), wglCreateLayerContext);
DETOUR_TRAMPOLINE(BOOL __stdcall Real_wglSwapBuffers(HDC hdc), wglSwapBuffers);
DETOUR_TRAMPOLINE(BOOL __stdcall Real_wglSwapLayerBuffers(HDC hdc,UINT fuPlanes), wglSwapLayerBuffers);

static LONG __stdcall Mine_ChangeDisplaySettingsEx(LPCTSTR lpszDeviceName,LPDEVMODE lpDevMode,HWND hwnd,DWORD dwflags,LPVOID lParam)
{
  LONG result = Real_ChangeDisplaySettingsEx(lpszDeviceName,lpDevMode,hwnd,dwflags,lParam);

  if(result == DISP_CHANGE_SUCCESSFUL && lpDevMode &&
    (lpDevMode->dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT)) == (DM_PELSWIDTH | DM_PELSHEIGHT))
    setCaptureResolution(lpDevMode->dmPelsWidth,lpDevMode->dmPelsHeight);

  return result;
}

static HGLRC __stdcall Mine_wglCreateContext(HDC hdc)
{
  HGLRC result = Real_wglCreateContext(hdc);
  if(result)
    initGLFromDC(hdc);

  return result;
}

HGLRC __stdcall Mine_wglCreateLayerContext(HDC hdc,int iLayerPlane)
{
  HGLRC result = Real_wglCreateLayerContext(hdc,iLayerPlane);
  if(result)
    initGLFromDC(hdc);

  return result;
}

static BOOL __stdcall Mine_wglSwapBuffers(HDC hdc)
{
  captureGLFrame();
  nextFrame();
  return Real_wglSwapBuffers(hdc);
}

static BOOL __stdcall Mine_wglSwapLayerBuffers(HDC hdc,UINT fuPlanes)
{
  if(fuPlanes & WGL_SWAP_MAIN_PLANE)
  {
    captureGLFrame();
    nextFrame();
  }

  return Real_wglSwapLayerBuffers(hdc,fuPlanes);
}

void initVideo_OpenGL()
{
  DetourFunctionWithTrampoline((PBYTE) Real_ChangeDisplaySettingsEx,(PBYTE) Mine_ChangeDisplaySettingsEx);
  DetourFunctionWithTrampoline((PBYTE) Real_wglCreateContext,(PBYTE) Mine_wglCreateContext);
  DetourFunctionWithTrampoline((PBYTE) Real_wglCreateLayerContext, (PBYTE) Mine_wglCreateLayerContext);
  DetourFunctionWithTrampoline((PBYTE) Real_wglSwapBuffers,(PBYTE) Mine_wglSwapBuffers);
  DetourFunctionWithTrampoline((PBYTE) Real_wglSwapLayerBuffers, (PBYTE) Mine_wglSwapLayerBuffers);
}
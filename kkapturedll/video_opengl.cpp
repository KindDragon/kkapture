/* kkapture: intrusive demo video capturing.
 * Copyright (c) 2005-2006 Fabian "ryg/farbrausch" Giesen.
 *
 * This program is free software; you can redistribute and/or modify it under
 * the terms of the Artistic License, Version 2.0beta5, or (at your opinion)
 * any later version; all distributions of this program should contain this
 * license in a file named "LICENSE.txt".
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT UNLESS REQUIRED BY
 * LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT HOLDER OR CONTRIBUTOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"
#include "video.h"
#include "videoencoder.h"
#include "videocapturetimer.h"

#include <gl/gl.h>
#pragma comment(lib,"opengl32.lib")

typedef BOOL (__stdcall *PWGLSWAPINTERVALEXT)(int interval);
typedef int (_stdcall 
             *PWGLGETSWAPINTERVALEXT)();

PWGLSWAPINTERVALEXT wglSwapIntervalEXT = 0;
PWGLGETSWAPINTERVALEXT wglGetSwapIntervalEXT = 0;

static void captureGLFrame()
{
  HDC hdc = wglGetCurrentDC();
  HWND wnd = WindowFromDC(hdc);

  if(wnd)
  {
    RECT rc;
    GetClientRect(wnd,&rc);
    setCaptureResolution(rc.right-rc.left,rc.bottom-rc.top);
  }

  if(captureData && encoder && params.CaptureVideo)
  {
    // use immediate blits if possible
    if(wglGetSwapIntervalEXT && wglSwapIntervalEXT && wglGetSwapIntervalEXT() > 0)
      wglSwapIntervalEXT(0);

    // blit data into capture buffer
    glReadPixels(0,0,captureWidth,captureHeight,GL_BGR_EXT,GL_UNSIGNED_BYTE,captureData);

    // encode
    encoder->WriteFrame(captureData);
  }
}

/*static void initGLFromDC(HDC hdc)
{
  // get client rect of window
  HWND wnd = WindowFromDC(hdc);
  RECT rc;

  if(wnd)
  {
    GetClientRect(wnd,&rc);
    setCaptureResolution(rc.right-rc.left,rc.bottom-rc.top);
  }
}*/

// trampolines

// newer wingdi.h versions don't declare this anymore
extern "C" BOOL __stdcall wglSwapBuffers(HDC hdc);

DETOUR_TRAMPOLINE(LONG __stdcall Real_ChangeDisplaySettingsEx(LPCTSTR lpszDeviceName,LPDEVMODE lpDevMode,HWND hwnd,DWORD dwflags,LPVOID lParam), ChangeDisplaySettingsEx);
DETOUR_TRAMPOLINE(HGLRC __stdcall Real_wglCreateContext(HDC hdc), wglCreateContext);
DETOUR_TRAMPOLINE(HGLRC __stdcall Real_wglCreateLayerContext(HDC hdc,int iLayerPlane), wglCreateLayerContext);
DETOUR_TRAMPOLINE(BOOL __stdcall Real_wglMakeCurrent(HDC hdc,HGLRC hglrc), wglMakeCurrent);
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
  /*if(result)
    initGLFromDC(hdc);*/

  return result;
}

HGLRC __stdcall Mine_wglCreateLayerContext(HDC hdc,int iLayerPlane)
{
  HGLRC result = Real_wglCreateLayerContext(hdc,iLayerPlane);
  /*if(result)
    initGLFromDC(hdc);*/

  return result;
}

BOOL __stdcall Mine_wglMakeCurrent(HDC hdc,HGLRC hglrc)
{
  BOOL result = Real_wglMakeCurrent(hdc,hglrc);

  if(result)
  {
    if(!wglSwapIntervalEXT || !wglGetSwapIntervalEXT)
    {
      wglSwapIntervalEXT = (PWGLSWAPINTERVALEXT) wglGetProcAddress("wglSwapIntervalEXT");
      wglGetSwapIntervalEXT = (PWGLGETSWAPINTERVALEXT) wglGetProcAddress("wglGetSwapIntervalEXT");
      if(wglSwapIntervalEXT)
        printLog("video/opengl: wglSwapIntervalEXT supported\n");
    }
  }

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
  DetourFunctionWithTrampoline((PBYTE) Real_wglMakeCurrent, (PBYTE) Mine_wglMakeCurrent);
  DetourFunctionWithTrampoline((PBYTE) Real_wglSwapBuffers,(PBYTE) Mine_wglSwapBuffers);
  DetourFunctionWithTrampoline((PBYTE) Real_wglSwapLayerBuffers, (PBYTE) Mine_wglSwapLayerBuffers);
}
// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "videoencoder.h"

// capture buffer
static int captureWidth = 0, captureHeight = 0;
static unsigned char *captureData = 0;

static void destroyCaptureBuffer()
{
  if(captureData)
  {
    delete[] captureData;
    captureData = 0;
  }
}

static void createCaptureBuffer(HWND hWnd,int width,int height)
{
  destroyCaptureBuffer();

  captureWidth = width;
  captureHeight = height;
  captureData = new unsigned char[width*height*3];
}

// video encoder
static void nextFrame()
{
  nextFrameTiming();
  nextFrameSound();
}

static void captureGLFrame()
{
  if(captureData && encoder)
  {
    // blit data into capture buffer
    glReadPixels(0,0,captureWidth,captureHeight,GL_BGR_EXT,GL_UNSIGNED_BYTE,captureData);

    // encode
    encoder->WriteFrame(captureData);
  }

  nextFrame();
}

// trampolines

// newer wingdi.h versions don't declare this anymore
extern "C" BOOL __stdcall wglSwapBuffers(HDC hdc);

DETOUR_TRAMPOLINE(LONG __stdcall Real_ChangeDisplaySettingsEx(LPCTSTR lpszDeviceName,LPDEVMODE lpDevMode,HWND hwnd,DWORD dwFlags,LPVOID lParam), ChangeDisplaySettingsEx);
DETOUR_TRAMPOLINE(BOOL __stdcall Real_SwapBuffers(HDC hdc), SwapBuffers);
DETOUR_TRAMPOLINE(BOOL __stdcall Real_wglSwapBuffers(HDC hdc), wglSwapBuffers);

static LONG __stdcall Mine_ChangeDisplaySettingsEx(LPCTSTR lpszDeviceName,LPDEVMODE lpDevMode,HWND hwnd,DWORD dwFlags,LPVOID lParam)
{
  printLog("video: ChangeDisplaySettingsEx\n");
  if(lpDevMode && (lpDevMode->dmFields & (DM_PELSWIDTH | DM_PELSHEIGHT)) == (DM_PELSWIDTH | DM_PELSHEIGHT))
  {
    HWND hWndFore = GetForegroundWindow();

    printLog("video:   change resolution to %dx%d\n",lpDevMode->dmPelsWidth,lpDevMode->dmPelsHeight);
    createCaptureBuffer(hwnd,lpDevMode->dmPelsWidth,lpDevMode->dmPelsHeight);

    if(encoder)
      encoder->SetSize(lpDevMode->dmPelsWidth,lpDevMode->dmPelsHeight);

    SetForegroundWindow(hWndFore);
  }

  LONG result = Real_ChangeDisplaySettingsEx(lpszDeviceName,lpDevMode,hwnd,dwFlags,lParam);

  return result;
}

static BOOL __stdcall Mine_SwapBuffers(HDC hdc)
{
  captureGLFrame();
  return Real_SwapBuffers(hdc);
}

static BOOL __stdcall Mine_wglSwapBuffers(HDC hdc)
{
  captureGLFrame();
  return Real_wglSwapBuffers(hdc);
}

// d3d wrappers

DETOUR_TRAMPOLINE(IDirect3D9 * __stdcall Real_Direct3DCreate9(UINT SDKVersion), Direct3DCreate9);

typedef HRESULT (__stdcall *PD3D9_CreateDevice)(IDirect3D9 *d3d,UINT a0,UINT a1,DWORD a2,DWORD a3,D3DPRESENT_PARAMETERS *a4,IDirect3DDevice9 **a5);
typedef HRESULT (__stdcall *PD3DDevice9_Present)(IDirect3DDevice9 *dev,DWORD a0,DWORD a1,DWORD a2,DWORD a3);

static PD3D9_CreateDevice Real_D3D9_CreateDevice;
static PD3DDevice9_Present Real_D3DDevice9_Present;

static HDC hCaptureDC = 0;
static HBITMAP hCaptureSection = 0;
static BYTE *captureDataD3D;

static HRESULT __stdcall Mine_D3DDevice9_Present(IDirect3DDevice9 *dev,DWORD a0,DWORD a1,DWORD a2,DWORD a3)
{
  HRESULT hr = Real_D3DDevice9_Present(dev,a0,a1,a2,a3);

  // grab via DC bitblit (faster than GetFrontBufferData, but kinda hackish)
  HDC hDC = GetDC(0);
  if(!hCaptureSection)
  {
    BITMAPINFOHEADER bih;
    ZeroMemory(&bih,sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = captureWidth;
    bih.biHeight = captureHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    hCaptureDC = CreateCompatibleDC(hDC);
    hCaptureSection = CreateDIBSection(hCaptureDC,(BITMAPINFO *) &bih,DIB_RGB_COLORS,
      (VOID **)&captureDataD3D,0,0);

    if(!hCaptureDC || !hCaptureSection)
    {
      if(hCaptureDC)
        DeleteDC(hCaptureDC);

      if(hCaptureSection)
        DeleteObject(hCaptureSection);

      hCaptureDC = 0;
      hCaptureSection = 0;
    }
    else
      SelectObject(hCaptureDC,hCaptureSection);
  }

  if(hCaptureSection && hCaptureDC)
  {
    BitBlt(hCaptureDC,0,0,captureWidth,captureHeight,hDC,0,0,SRCCOPY);
    encoder->WriteFrame(captureDataD3D);
  }

  ReleaseDC(0,hDC);
  nextFrame();

  return hr;
}

static HRESULT __stdcall Mine_D3D9_CreateDevice(IDirect3D9 *d3d,UINT a0,UINT a1,
  DWORD a2,DWORD a3,D3DPRESENT_PARAMETERS *a4,IDirect3DDevice9 **a5)
{
  HRESULT hr = Real_D3D9_CreateDevice(d3d,a0,a1,a2,a3,a4,a5);

  if(SUCCEEDED(hr) && *a5)
  {
    unprotectVTable(*a5,119);
    Real_D3DDevice9_Present = (PD3DDevice9_Present) patchVTable(*a5,17,(PBYTE) Mine_D3DDevice9_Present);
    printLog("video: IDirect3D9::CreateDevice successful, vtable patched.\n");
  }

  return hr;
}

static IDirect3D9 * __stdcall Mine_Direct3DCreate9(UINT SDKVersion)
{
  IDirect3D9 *d3d9 = Real_Direct3DCreate9(SDKVersion);

  unprotectVTable(d3d9,17);
  Real_D3D9_CreateDevice = (PD3D9_CreateDevice) patchVTable(d3d9,16,(PBYTE) Mine_D3D9_CreateDevice);
  printLog("video: IDirect3D9 object created, vtable patched.\n");

  return d3d9;
}

// public interface

void initVideo()
{
  DetourFunctionWithTrampoline((PBYTE) Real_ChangeDisplaySettingsEx,(PBYTE) Mine_ChangeDisplaySettingsEx);
  DetourFunctionWithTrampoline((PBYTE) Real_SwapBuffers,(PBYTE) Mine_SwapBuffers);
  DetourFunctionWithTrampoline((PBYTE) Real_wglSwapBuffers,(PBYTE) Mine_wglSwapBuffers);

  DetourFunctionWithTrampoline((PBYTE) Real_Direct3DCreate9,(PBYTE) Mine_Direct3DCreate9);
}

void doneVideo()
{
  if(hCaptureDC)
  {
    DeleteDC(hCaptureDC);
    hCaptureDC = 0;
  }

  if(hCaptureSection)
  {
    DeleteObject(hCaptureSection);
    hCaptureSection = 0;
  }

  destroyCaptureBuffer();
}
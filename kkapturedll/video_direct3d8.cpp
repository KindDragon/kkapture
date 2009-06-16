#include "stdafx.h"
#include "video.h"
#include "videoencoder.h"

#include "d3d8.h"
#pragma comment(lib,"d3d8.lib")

DETOUR_TRAMPOLINE(IDirect3D8 * __stdcall Real_Direct3DCreate8(UINT SDKVersion), Direct3DCreate8);

typedef HRESULT (__stdcall *PD3D8_CreateDevice)(IDirect3D8 *d3d,UINT a0,UINT a1,DWORD a2,DWORD a3,D3DPRESENT_PARAMETERS *a4,IDirect3DDevice8 **a5);
typedef ULONG (__stdcall *PD3DDevice8_Release)(IDirect3DDevice8 *dev);
typedef HRESULT (__stdcall *PD3DDevice8_Present)(IDirect3DDevice8 *dev,DWORD a0,DWORD a1,DWORD a2,DWORD a3);

static PD3D8_CreateDevice Real_D3D8_CreateDevice = 0;
static PD3DDevice8_Release Real_D3DDevice8_Release = 0;
static PD3DDevice8_Present Real_D3DDevice8_Present = 0;

static IDirect3DSurface8 *captureSurf = 0;
static ULONG startRefCount = 0;

static bool captureD3DFrame8(IDirect3DDevice8 *dev)
{
  if(!captureSurf)
    return false;

  IDirect3DSurface8 *back = 0;
  bool error = true;

  dev->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back);
  if(back)
  {
    if(SUCCEEDED(dev->CopyRects(back,0,0,captureSurf,0)))
    {
      D3DLOCKED_RECT lr;

      if(SUCCEEDED(captureSurf->LockRect(&lr,0,D3DLOCK_READONLY)))
      {
        blitAndFlipBGRAToCaptureData((unsigned char *) lr.pBits,lr.Pitch);
        captureSurf->UnlockRect();
        error = false;
      }
    }

    back->Release();
  }

  if(!error)
    encoder->WriteFrame(captureData);

  return !error;
}

static ULONG __stdcall Mine_D3DDevice8_Release(IDirect3DDevice8 *dev)
{
  ULONG ref = Real_D3DDevice8_Release(dev);

  if(ref <= startRefCount)
  {
    dev->AddRef();

    if(captureSurf)
    {
      captureSurf->Release();
      captureSurf = 0;
    }

    return Real_D3DDevice8_Release(dev);
  }
  else
    return ref;
}

static HRESULT __stdcall Mine_D3DDevice8_Present(IDirect3DDevice8 *dev,DWORD a0,DWORD a1,DWORD a2,DWORD a3)
{
  HRESULT hr = Real_D3DDevice8_Present(dev,a0,a1,a2,a3);

  if(params.CaptureVideo)
  {
    if(!captureD3DFrame8(dev))
      captureGDIFullScreen();
  }

  nextFrame();
  return hr;
}

static HRESULT __stdcall Mine_D3D8_CreateDevice(IDirect3D8 *d3d,UINT a0,UINT a1,
  DWORD a2,DWORD a3,D3DPRESENT_PARAMETERS *a4,IDirect3DDevice8 **a5)
{
  if(a4)
  {
    a4->BackBufferCount = 1;
    a4->SwapEffect = D3DSWAPEFFECT_COPY;

    // force back buffer format to something we can read
    D3DFORMAT fmt = a4->BackBufferFormat;
    if(fmt == D3DFMT_A1R5G5B5 || fmt == D3DFMT_A8R8G8B8)
      a4->BackBufferFormat = D3DFMT_A8R8G8B8;
    else
      a4->BackBufferFormat = D3DFMT_X8R8G8B8;
  }

  HRESULT hr = Real_D3D8_CreateDevice(d3d,a0,a1,a2,a3,a4,a5);

  if(SUCCEEDED(hr) && *a5)
  {
    IDirect3DDevice8 *dev = *a5;

    if(!Real_D3DDevice8_Release)
      Real_D3DDevice8_Release = (PD3DDevice8_Release) DetourCOM(dev,2,(PBYTE) Mine_D3DDevice8_Release);

    if(!Real_D3DDevice8_Present)
      Real_D3DDevice8_Present = (PD3DDevice8_Present) DetourCOM(dev,15,(PBYTE) Mine_D3DDevice8_Present);

    printLog("video: IDirect3D8::CreateDevice successful.\n");

    dev->AddRef();
    startRefCount = Real_D3DDevice8_Release(dev);

    setCaptureResolution(a4->BackBufferWidth,a4->BackBufferHeight);
    if(FAILED(dev->CreateImageSurface(captureWidth,captureHeight,a4->BackBufferFormat,&captureSurf)))
      printLog("video: couldn't create capture surface.\n");
  }

  return hr;
}

static IDirect3D8 * __stdcall Mine_Direct3DCreate8(UINT SDKVersion)
{
  IDirect3D8 *d3d8 = Real_Direct3DCreate8(SDKVersion);

  if(d3d8)
  {
    if(!Real_D3D8_CreateDevice)
      Real_D3D8_CreateDevice = (PD3D8_CreateDevice) DetourCOM(d3d8,15,(PBYTE) Mine_D3D8_CreateDevice);

    printLog("video: IDirect3D8 object created.\n");
  }

  return d3d8;
}

void initVideo_Direct3D8()
{
  DetourFunctionWithTrampoline((PBYTE) Real_Direct3DCreate8,(PBYTE) Mine_Direct3DCreate8);
}
// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "video.h"
#include "videoencoder.h"

#include "d3d9.h"
#pragma comment (lib,"d3d9.lib")

DETOUR_TRAMPOLINE(IDirect3D9 * __stdcall Real_Direct3DCreate9(UINT SDKVersion), Direct3DCreate9);

typedef HRESULT (__stdcall *PD3D9_CreateDevice)(IDirect3D9 *d3d,UINT a0,UINT a1,DWORD a2,DWORD a3,D3DPRESENT_PARAMETERS *a4,IDirect3DDevice9 **a5);
typedef ULONG (__stdcall *PD3DDevice9_Release)(IDirect3DDevice9 *dev);
typedef HRESULT (__stdcall *PD3DDevice9_Present)(IDirect3DDevice9 *dev,DWORD a0,DWORD a1,DWORD a2,DWORD a3);

static PD3D9_CreateDevice Real_D3D9_CreateDevice;
static PD3DDevice9_Release Real_D3DDevice9_Release;
static PD3DDevice9_Present Real_D3DDevice9_Present;

static IDirect3DTexture9 *captureTex = 0;
static IDirect3DSurface9 *captureSurf = 0;
static IDirect3DSurface9 *captureInbetween = 0;
static ULONG startRefCount = 0;
static bool multiSampleMode = false;

static bool captureD3DFrame9(IDirect3DDevice9 *dev)
{
  if(!captureSurf)
    return false;

  IDirect3DSurface9 *back = 0, *captureSrc;
  bool error = true;

  dev->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back);
  if(back)
  {
    // if multisampling is used, we need another inbetween blit
    if(multiSampleMode)
    {
      if(FAILED(dev->StretchRect(back,0,captureInbetween,0,D3DTEXF_LINEAR)))
        return false;

      captureSrc = captureInbetween;
    }
    else
      captureSrc = back;

    if(SUCCEEDED(dev->GetRenderTargetData(captureSrc,captureSurf)))
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

static ULONG __stdcall Mine_D3DDevice9_Release(IDirect3DDevice9 *dev)
{
  ULONG ref = Real_D3DDevice9_Release(dev);

  if(ref <= startRefCount)
  {
    dev->AddRef();

    if(captureSurf)
    {
      captureSurf->Release();
      captureSurf = 0;
    }

    if(captureTex)
    {
      captureTex->Release();
      captureTex = 0;
    }

    if(captureInbetween)
    {
      captureInbetween->Release();
      captureInbetween = 0;
    }

    return Real_D3DDevice9_Release(dev);
  }
  else
    return ref;
}

static HRESULT __stdcall Mine_D3DDevice9_Present(IDirect3DDevice9 *dev,DWORD a0,DWORD a1,DWORD a2,DWORD a3)
{
  HRESULT hr = Real_D3DDevice9_Present(dev,a0,a1,a2,a3);

  if(!captureD3DFrame9(dev))
    captureGDIFullScreen();

  nextFrame();

  return hr;
}

static HRESULT __stdcall Mine_D3D9_CreateDevice(IDirect3D9 *d3d,UINT a0,UINT a1,
  DWORD a2,DWORD a3,D3DPRESENT_PARAMETERS *a4,IDirect3DDevice9 **a5)
{
  if(a4)
  {
		a4->BackBufferCount = 1;

    if(a4->MultiSampleType == D3DMULTISAMPLE_NONE)
    {
  		a4->SwapEffect = D3DSWAPEFFECT_COPY;
      multiSampleMode = false;
    }
    else
      multiSampleMode = true;

		// force back buffer format to something we can read
		D3DFORMAT fmt = a4->BackBufferFormat;
		if(fmt == D3DFMT_A2R10G10B10 || fmt == D3DFMT_A1R5G5B5 || fmt == D3DFMT_A8R8G8B8)
			a4->BackBufferFormat = D3DFMT_A8R8G8B8;
		else
			a4->BackBufferFormat = D3DFMT_X8R8G8B8;
  }

  HRESULT hr = Real_D3D9_CreateDevice(d3d,a0,a1,a2,a3,a4,a5);

  if(SUCCEEDED(hr) && *a5)
  {
    static bool firstCreate = true;
    IDirect3DDevice9 *dev = *a5;

    if(firstCreate)
    {
      unprotectVTable(dev,119);
      Real_D3DDevice9_Release = (PD3DDevice9_Release) patchVTable(dev,2,(PBYTE) Mine_D3DDevice9_Release);
      Real_D3DDevice9_Present = (PD3DDevice9_Present) patchVTable(dev,17,(PBYTE) Mine_D3DDevice9_Present);
      printLog("video: IDirect3D9::CreateDevice successful, vtable patched.\n");
      firstCreate = false;
    }
    else
      printLog("video: IDirect3D9::CreateDevice successful.\n");

    dev->AddRef();
    startRefCount = Real_D3DDevice9_Release(dev);

    setCaptureResolution(a4->BackBufferWidth,a4->BackBufferHeight);
    if(SUCCEEDED(dev->CreateTexture(captureWidth,captureHeight,1,0,
      a4->BackBufferFormat,D3DPOOL_SYSTEMMEM,&captureTex,0)))
    {
      if(FAILED(captureTex->GetSurfaceLevel(0,&captureSurf)))
        printLog("video: couldn't get capture surface.\n");

      if(FAILED(dev->CreateRenderTarget(captureWidth,captureHeight,a4->BackBufferFormat,
        D3DMULTISAMPLE_NONE,0,FALSE,&captureInbetween,0)))
        printLog("video: couldn't create multisampling blit buffer\n");
    }
    else
      printLog("video: couldn't create capture texture.\n");
  }

  return hr;
}

static IDirect3D9 * __stdcall Mine_Direct3DCreate9(UINT SDKVersion)
{
  IDirect3D9 *d3d9 = Real_Direct3DCreate9(SDKVersion);

  if(d3d9)
  {
    static bool firstCreate = true;

    if(firstCreate)
    {
      unprotectVTable(d3d9,17);
      Real_D3D9_CreateDevice = (PD3D9_CreateDevice) patchVTable(d3d9,16,(PBYTE) Mine_D3D9_CreateDevice);
      printLog("video: IDirect3D9 object created, vtable patched.\n");
      firstCreate = false;
    }
    else
      printLog("video: IDirect3D9 object created.\n");
  }

  return d3d9;
}

void initVideo_Direct3D9()
{
  DetourFunctionWithTrampoline((PBYTE) Real_Direct3DCreate9,(PBYTE) Mine_Direct3DCreate9);
}
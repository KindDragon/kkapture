/* kkapture: intrusive demo video capturing.
 * Copyright (c) 2005-2010 Fabian "ryg/farbrausch" Giesen.
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
#include "videoencoder.h"
#include "video.h"
#include "sound_directsound.h"

#pragma comment(lib,"dsound.lib")
#pragma comment(lib,"dxguid.lib")
// if waveOutGetPosition is called frequently in a single frame, assume the app is waiting for the
// current playback position to change and advance the time. this is the threshold for "frequent" calls.
static const int MAX_GETPOSITION_PER_FRAME = 1024;

// my own directsound fake!
class MyDirectSound8;
class MyDirectSoundBuffer8;

MyDirectSoundBuffer8 *playBuffer = 0;

class LockOwner
{
  CRITICAL_SECTION &section;

public:
  LockOwner(CRITICAL_SECTION &cs)
    : section(cs)
  {
    EnterCriticalSection(&section);
  }

  ~LockOwner()
  {
    LeaveCriticalSection(&section);
  }
};

class MyDirectSound3DListener8 : public IDirectSound3DListener8
{
  int RefCount;

public:
  MyDirectSound3DListener8()
    : RefCount(1)
  {
  }

  // IUnknown methods
  virtual HRESULT __stdcall QueryInterface(REFIID iid,LPVOID *ptr)
  {
    if(iid == IID_IDirectSound3DListener || iid == IID_IDirectSound3DListener8)
    {
      *ptr = this;
      AddRef();
      return S_OK;
    }
    else
      return E_NOINTERFACE;
  }

  virtual ULONG __stdcall AddRef()
  {
    return ++RefCount;
  }

  virtual ULONG __stdcall Release()
  {
    ULONG ret = --RefCount;
    if(!RefCount)
      delete this;

    return ret;
  }

  // IDirectSound3DListener methods
  virtual HRESULT __stdcall GetAllParameters(LPDS3DLISTENER pListener)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetDistanceFactor(D3DVALUE* pflDistanceFactor)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetDopplerFactor(D3DVALUE* pflDistanceFactor)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetOrientation(D3DVECTOR* pvOrientFront, D3DVECTOR* pvOrientTop)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetPosition(D3DVECTOR* pvPosition)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetRolloffFactor(D3DVALUE* pflRolloffFactor)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetVelocity(D3DVECTOR* pvVelocity)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetAllParameters(LPCDS3DLISTENER pcListener,DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetDistanceFactor(D3DVALUE flDistanceFactor,DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetDopplerFactor(D3DVALUE flDopplerFactor,DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetOrientation(D3DVALUE xFront,D3DVALUE yFront,D3DVALUE zFront,D3DVALUE xTop,D3DVALUE yTop,D3DVALUE zTop,DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetPosition(D3DVALUE x,D3DVALUE y,D3DVALUE z,DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetRolloffFactor(D3DVALUE flRolloffFactor,DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetVelocity(D3DVALUE x,D3DVALUE y,D3DVALUE z,DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall CommitDeferredSettings()
  {
    return S_OK;
  }
};

class MyDirectSound3DBuffer8 : public IDirectSound3DBuffer8
{
  int RefCount;

public:
  MyDirectSound3DBuffer8()
    : RefCount(1)
  {
  }

  // IUnknown methods
  virtual HRESULT __stdcall MyDirectSound3DBuffer8::QueryInterface(REFIID iid,LPVOID *ptr)
  {
    if(iid == IID_IDirectSound3DBuffer || iid == IID_IDirectSound3DListener8)
    {
      *ptr = this;
      AddRef();
      return S_OK;
    }
    else
      return E_NOINTERFACE;
  }

  virtual ULONG __stdcall AddRef()
  {
    return ++RefCount;
  }

  virtual ULONG __stdcall Release()
  {
    ULONG ret = --RefCount;
    if(!RefCount)
      delete this;

    return ret;
  }

  // IDirectSound3DBuffer methods
  virtual HRESULT __stdcall GetAllParameters(LPDS3DBUFFER pDs3dBuffer)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetConeAngles(LPDWORD pdwInsideConeAngle, LPDWORD pdwOutsideConeAngle)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetConeOrientation(D3DVECTOR *pvOrientation)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetConeOutsideVolume(LPLONG plConeOutsideVolume)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetMaxDistance(D3DVALUE *pflMaxDistance)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetMinDistance(D3DVALUE *pflMaxDistance)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetMode(LPDWORD pdwMode)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetPosition(D3DVECTOR *pvPosition)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetVelocity(D3DVECTOR *pvVelocity)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetAllParameters(LPCDS3DBUFFER pDs3dBuffer, DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetConeAngles(DWORD dwInsideConeAngle, DWORD dwOutsideConeAngle, DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetConeOrientation(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetConeOutsideVolume(LONG lConeOutsideVolume, DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetMaxDistance(D3DVALUE flMaxDistance, DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetMinDistance(D3DVALUE flMaxDistance, DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetMode(DWORD dwMode, DWORD dwApply)
  {
    return E_NOTIMPL;
  }
  
  virtual HRESULT __stdcall SetPosition(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD dwApply)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetVelocity(D3DVALUE x, D3DVALUE y, D3DVALUE z, DWORD dwApply)
  {
    return E_NOTIMPL;
  }
};

  MyDirectSoundBuffer8::MyDirectSoundBuffer8(DWORD flags,DWORD bufBytes,LPWAVEFORMATEX fmt)
    : RefCount(1)
  {
    Flags = flags;
    Buffer = new BYTE[bufBytes];
    Bytes = bufBytes;
    memset(Buffer,0,bufBytes);

    if(fmt)
      Format = CopyFormat(fmt);
    else
    {
      Format = new WAVEFORMATEX;
      Format->wFormatTag = WAVE_FORMAT_PCM;
      Format->nChannels = 2;
      Format->nSamplesPerSec = 44100;
      Format->nAvgBytesPerSec = 176400;
      Format->nBlockAlign = 4;
      Format->wBitsPerSample = 16;
      Format->cbSize = 0;
    }

    Frequency = Format->nSamplesPerSec;

    Playing = FALSE;
    Looping = FALSE;
    Volume = 0;
    Panning = 0;
    PlayCursor = 0;
    SkipAllowed = FALSE;
    SamplesPlayed = 0;
    GetPosThisFrame = 0;

    InitializeCriticalSection(&BufferLock);
  }

  MyDirectSoundBuffer8::~MyDirectSoundBuffer8()
  {
    // synchronize access to bufferlock before deleting the section
    {
      LockOwner sync(BufferLock);
    }

    DeleteCriticalSection(&BufferLock);
    delete Format;
    delete[] Buffer;
  }

  // IUnknown methods
  HRESULT __stdcall MyDirectSoundBuffer8::QueryInterface(REFIID iid,LPVOID *ptr)
  {
    if(iid == IID_IDirectSoundBuffer || iid == IID_IDirectSoundBuffer8)
    {
      *ptr = this;
      AddRef();
      return S_OK;
    }
    else if(iid == IID_IDirectSound3DListener || iid == IID_IDirectSound3DListener8)
    {
      *ptr = new MyDirectSound3DListener8;
      return S_OK;
    }
    else if(iid == IID_IDirectSound3DBuffer || iid == IID_IDirectSound3DBuffer8)
    {
      *ptr = new MyDirectSound3DBuffer8;
      return S_OK;
    }
    else
      return E_NOINTERFACE;
  }

  // IDirectSoundBuffer methods
  HRESULT __stdcall MyDirectSoundBuffer8::GetCaps(LPDSBCAPS caps)
  {
    if(caps && caps->dwSize == sizeof(DSBCAPS))
    {
      caps->dwFlags = Flags;
      caps->dwBufferBytes = Bytes;
      caps->dwUnlockTransferRate = 400 * 1024;
      caps->dwPlayCpuOverhead = 0;

      return S_OK;
    }
    else
    {
      printLog("dsound: DirectSoundBuffer::GetCaps - invalid size\n");
      return DSERR_INVALIDPARAM;
    }
  }

  HRESULT __stdcall MyDirectSoundBuffer8::GetCurrentPosition(LPDWORD pdwCurrentPlayCursor,LPDWORD pdwCurrentWriteCursor)
  {
    LockOwner lock(BufferLock);

    if(++GetPosThisFrame >= MAX_GETPOSITION_PER_FRAME) // assume that the app is waiting for the playback position to change.
    {
      printLog("sound: app is hammering dsound GetCurrentPosition, advancing time (frame=%d)\n",getFrameTiming());
      GetPosThisFrame = 0;
      LeaveCriticalSection(&BufferLock);
      skipFrame();
      EnterCriticalSection(&BufferLock);
    }

    // skip some milliseconds of silence at start
    if(SkipAllowed)
    {
      DWORD maxskip = UMulDiv(Format->nSamplesPerSec,Format->nBlockAlign*params.SoundMaxSkip,1000);
      DWORD pp = PlayCursor;
      DWORD i;

      // find out whether the next maxskip bytes are zero
      for(i=0;i<maxskip;i++)
      {
        if(Buffer[pp])
          break;

        if(++pp == Bytes)
          pp = 0;
      }

      // yes they are, skip them
      if(i && i == maxskip)
        PlayCursor = pp;
      else
        SkipAllowed = FALSE;
    }

    if(!Playing) // not playing, report zeroes
    {
      if(pdwCurrentPlayCursor)
        *pdwCurrentPlayCursor = 0;

      if(pdwCurrentWriteCursor)
        *pdwCurrentWriteCursor = 0;
    }
    else // playing, so report current positions
    {
      //// the "track one" hack!
      //if(params.FairlightHack && !Looping && ++GetPosThisFrame > 128)
      //{
      //  Stop();
      //  PlayCursor = Bytes;
      //}

      if(pdwCurrentPlayCursor)
        *pdwCurrentPlayCursor = PlayCursor;

      if(pdwCurrentWriteCursor)
        *pdwCurrentWriteCursor = WriteCursor();
    }

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten)
  {
    int size = min(dwSizeAllocated,Format ? sizeof(WAVEFORMATEX) + Format->cbSize : 0);

    if(pdwSizeWritten)
      *pdwSizeWritten = size;

    if(pwfxFormat)
      memcpy(pwfxFormat,Format,size);

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::GetVolume(LPLONG plVolume)
  {
    if(plVolume)
      *plVolume = Volume;

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::GetPan(LPLONG plPan)
  {
    if(plPan)
      *plPan = Panning;

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::GetFrequency(LPDWORD pdwFrequency)
  {
    if(pdwFrequency)
      *pdwFrequency = Frequency;

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::GetStatus(LPDWORD pdwStatus)
  {
    if(pdwStatus)
    {
      *pdwStatus = 0;
      *pdwStatus |= Playing ? DSBSTATUS_PLAYING : 0;
      *pdwStatus |= Looping ? DSBSTATUS_LOOPING : 0;
    }

    return DS_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::Initialize(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc)
  {
    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::Lock(DWORD dwOffset,DWORD dwBytes,LPVOID *ppvAudioPtr1,LPDWORD pdwAudioBytes1,LPVOID *ppvAudioPtr2,LPDWORD pdwAudioBytes2,DWORD dwFlags)
  {
    LockOwner lock(BufferLock);

    if(dwFlags & DSBLOCK_FROMWRITECURSOR)
      dwOffset = WriteCursor();

    if(dwFlags & DSBLOCK_ENTIREBUFFER)
      dwBytes = Bytes;

    if(dwOffset >= Bytes || dwBytes > Bytes)
    {
      LeaveCriticalSection(&BufferLock);
      return DSERR_INVALIDPARAM;
    }

    if(dwOffset + dwBytes <= Bytes) // no wrap
    {
      *ppvAudioPtr1 = (LPVOID) (Buffer + dwOffset);
      *pdwAudioBytes1 = dwBytes;
      if(ppvAudioPtr2)
      {
        *ppvAudioPtr2 = 0;
        *pdwAudioBytes2 = 0;
      }
    }
    else // wrap
    {
      *ppvAudioPtr1 = (LPVOID) (Buffer + dwOffset);
      *pdwAudioBytes1 = Bytes - dwOffset;
      if(ppvAudioPtr2)
      {
        *ppvAudioPtr2 = (LPVOID) Buffer;
        *pdwAudioBytes2 = dwBytes - *pdwAudioBytes1;
      }
    }

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::Play(DWORD dwReserved1,DWORD dwPriority,DWORD dwFlags)
  {
    if(!Playing)
      SkipAllowed = TRUE;

    printLog("sound: play\n");

    Playing = TRUE;
    Looping = (dwFlags & DSBPLAY_LOOPING) ? TRUE : FALSE;

    if(!(Flags & DSBCAPS_PRIMARYBUFFER)/* && (!params.FairlightHack || Looping)*/)
    {
      encoder->SetAudioFormat(Format);
      playBuffer = this;
      FirstFrame = getFrameTiming();
    }

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::SetCurrentPosition(DWORD dwNewPosition)
  {
    PlayCursor = dwNewPosition;
    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::SetFormat(LPCWAVEFORMATEX pcfxFormat)
  {
    delete Format;
    Format = CopyFormat(pcfxFormat);
    if(playBuffer==this)
      encoder->SetAudioFormat(Format);

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::SetVolume(LONG lVolume)
  {
    Volume = lVolume;
    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::SetPan(LONG lPan)
  {
    Panning = lPan;
    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::SetFrequency(DWORD dwFrequency)
  {
    Frequency = dwFrequency;
    Format->nSamplesPerSec = dwFrequency;
    Format->nAvgBytesPerSec = Format->nBlockAlign * dwFrequency;
    if(playBuffer==this)
      encoder->SetAudioFormat(Format);

    return S_OK;
  }

  HRESULT __stdcall MyDirectSoundBuffer8::Stop()
  {
    Playing = FALSE;
    if(playBuffer == this)
      playBuffer = 0;

    return S_OK;
  }

  // ----
  void MyDirectSoundBuffer8::encodeLastFrameAudio()
  {
    LockOwner lock(BufferLock);

    // calculate number of samples processed since last frame, then encode
    DWORD frameSize = NextFrameSize();
    DWORD end = PlayCursor + frameSize;
    DWORD align = Format->nBlockAlign;

    if(end - PlayCursor > Bytes)
    {
      printLog("sound: more samples required per frame than buffer allows, increase frame rate\n");
      end = PlayCursor + Bytes;
    }

    if(end > Bytes) // wrap
    {
      encoder->WriteAudioFrame(Buffer + PlayCursor,(Bytes - PlayCursor) / align);
      encoder->WriteAudioFrame(Buffer,(end - Bytes) / align);
    }
    else // no wrap
      encoder->WriteAudioFrame(Buffer + PlayCursor,(end - PlayCursor) / align);

    PlayCursor = end % Bytes;
    SamplesPlayed += frameSize;
    GetPosThisFrame = 0;
  }

class MyDirectSound8 : public IDirectSound8
{
  int RefCount;

public:
  MyDirectSound8()
    : RefCount(1)
  {
    videoNeedEncoder();
  }

  // IUnknown methods
  virtual HRESULT __stdcall QueryInterface(REFIID iid,LPVOID *ptr)
  {
    printLog("sound: dsound queryinterface\n");
    return E_NOINTERFACE;
  }

  virtual ULONG __stdcall AddRef()
  {
    return ++RefCount;
  }

  virtual ULONG __stdcall Release()
  {
    ULONG ret = --RefCount;
    if(!RefCount)
      delete this;

    return ret;
  }

  // IDirectSound methods
  virtual HRESULT __stdcall CreateSoundBuffer(LPCDSBUFFERDESC desc,LPDIRECTSOUNDBUFFER *ppDSBuffer,LPUNKNOWN pUnkOuter)
  {
    if(desc && (desc->dwSize == sizeof(DSBUFFERDESC) || desc->dwSize == sizeof(DSBUFFERDESC1)))
    {
      if(desc->dwFlags & DSBCAPS_LOCHARDWARE) // we certainly don't do hw mixing.
        return DSERR_CONTROLUNAVAIL;

      *ppDSBuffer = new MyDirectSoundBuffer8(desc->dwFlags,desc->dwBufferBytes,desc->lpwfxFormat);
      printLog("sound: buffer created\n");
      return S_OK;
    }
    else
      return DSERR_INVALIDPARAM;
  }

  virtual HRESULT __stdcall GetCaps(LPDSCAPS pDSCaps)
  {
    if(pDSCaps && pDSCaps->dwSize == sizeof(DSCAPS))
    {
      ZeroMemory(pDSCaps,sizeof(DSCAPS));

      pDSCaps->dwSize = sizeof(DSCAPS);
      pDSCaps->dwFlags = DSCAPS_CONTINUOUSRATE
        | DSCAPS_PRIMARY16BIT | DSCAPS_PRIMARY8BIT
        | DSCAPS_PRIMARYMONO | DSCAPS_PRIMARYSTEREO
        | DSCAPS_SECONDARY16BIT | DSCAPS_SECONDARY8BIT
        | DSCAPS_SECONDARYMONO | DSCAPS_SECONDARYSTEREO;
      pDSCaps->dwMinSecondarySampleRate = 4000;
      pDSCaps->dwMaxSecondarySampleRate = 96000;
      pDSCaps->dwPrimaryBuffers = 1;

      return S_OK;
    }
    else
    {
      printLog("sound: DirectSound::GetCaps - invalid size\n");
      return DSERR_INVALIDPARAM;
    }
  }

  virtual HRESULT __stdcall DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER pDSBufferOriginal,LPDIRECTSOUNDBUFFER *ppDSBufferDuplicate)
  {
    printLog("sound: attempting DuplicateSoundBuffer hack...\n");

    if(!ppDSBufferDuplicate)
      return E_INVALIDARG;

    // we don't mix, so simply return a handle to the same buffer and hope it works...
    pDSBufferOriginal->AddRef();
    *ppDSBufferDuplicate = pDSBufferOriginal;
    return S_OK;

    //printLog("sound: DuplicateSoundBuffer attempted (not implemented yet)\n");
    //return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetCooperativeLevel(HWND hwnd,DWORD dwLevel)
  {
    return S_OK;
  }

  virtual HRESULT __stdcall Compact()
  {
    return S_OK;
  }

  virtual HRESULT __stdcall GetSpeakerConfig(LPDWORD pdwSpeakerConfig)
  {
    *pdwSpeakerConfig = DSSPEAKER_STEREO;
    return S_OK;
  }

  virtual HRESULT __stdcall SetSpeakerConfig(DWORD dwSpeakerConfig)
  {
    printLog("sound: dsound setspeakerconfig\n");
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall Initialize(LPCGUID pcGuidDevice)
  {
    return S_OK;
  }

  // IDirectSound8 methods
  virtual HRESULT __stdcall VerifyCertification(LPDWORD pdwCertified)
  {
    printLog("sound: dsound verifycertification\n");
    return E_NOTIMPL;
  }
};

// trampolines
DETOUR_TRAMPOLINE(HRESULT __stdcall Real_DirectSoundCreate(LPCGUID lpcGuidDevice,LPDIRECTSOUND *ppDS,LPUNKNOWN pUnkOuter), DirectSoundCreate);
DETOUR_TRAMPOLINE(HRESULT __stdcall Real_DirectSoundCreate8(LPCGUID lpcGuidDevice,LPDIRECTSOUND8 *ppDS8,LPUNKNOWN pUnkOuter), DirectSoundCreate8);

DETOUR_TRAMPOLINE(HRESULT __stdcall Real_CoCreateInstance(REFCLSID rclsid,LPUNKNOWN pUnkOuter,DWORD dwClsContext,REFIID riid,LPVOID *ppv), CoCreateInstance);

HRESULT __stdcall Mine_DirectSoundCreate(LPCGUID lpcGuidDevice,LPDIRECTSOUND8 *ppDS8,LPUNKNOWN pUnkOuter)
{
  printLog("sound: emulating DirectSound\n");
  *ppDS8 = new MyDirectSound8;
  return S_OK;
}

HRESULT __stdcall Mine_DirectSoundCreate8(LPCGUID lpcGuidDevice,LPDIRECTSOUND8 *ppDS8,LPUNKNOWN pUnkOuter)
{
  printLog("sound: emulating DirectSound 8\n");
  *ppDS8 = new MyDirectSound8;
  return S_OK;
}

HRESULT __stdcall Mine_CoCreateInstance(REFCLSID rclsid,LPUNKNOWN pUnkOuter,DWORD dwClsContext,REFIID riid,LPVOID *ppv)
{
  IUnknown **ptr = (IUnknown **) ppv;

  if(rclsid == CLSID_DirectSound && riid == IID_IDirectSound)
  {
    printLog("sound: emulating DirectSound (created via CoCreateInstance)\n");
    *ptr = new MyDirectSound8;
    return S_OK;
  }
  else if(rclsid == CLSID_DirectSound8 && riid == IID_IDirectSound8)
  {
    printLog("sound: emulating DirectSound 8 (created via CoCreateInstance)\n");
    *ptr = new MyDirectSound8;
    return S_OK;
  }
  else
    return Real_CoCreateInstance(rclsid,pUnkOuter,dwClsContext,riid,ppv);
}

void initSoundsysDirectSound()
{
  DetourFunctionWithTrampoline((PBYTE) Real_DirectSoundCreate,(PBYTE) Mine_DirectSoundCreate);
  DetourFunctionWithTrampoline((PBYTE) Real_DirectSoundCreate8,(PBYTE) Mine_DirectSoundCreate8);

  DetourFunctionWithTrampoline((PBYTE) Real_CoCreateInstance,(PBYTE) Mine_CoCreateInstance);
}

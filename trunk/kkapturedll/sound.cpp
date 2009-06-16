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
#include <malloc.h>
#include "videoencoder.h"
#include "util.h"

#include <dsound.h>
#pragma comment (lib,"dsound.lib")
#pragma comment (lib,"dxguid.lib")
#pragma comment(lib, "winmm.lib")

// my own directsound fake!
class MyDirectSound8;
class MyDirectSoundBuffer8;

static MyDirectSoundBuffer8 *playBuffer = 0;

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
  virtual HRESULT __stdcall QueryInterface(REFIID iid,LPVOID *ptr)
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

class MyDirectSoundBuffer8 : public IDirectSoundBuffer8
{
  int RefCount;
  PBYTE Buffer;
  DWORD Flags;
  DWORD Bytes;
  WAVEFORMATEX Format;
  DWORD Frequency;
  BOOL Playing,Looping;
  LONG Volume;
  CRITICAL_SECTION BufferLock;
  DWORD PlayCursor;
  BOOL SkipAllowed;
  DWORD SamplesPlayed;
  DWORD GetPosThisFrame;
  int FirstFrame;

  DWORD NextFrameSize()
  {
    DWORD frame = getFrameTiming() - FirstFrame;
    DWORD samplePos = UMulDiv(100 * frame,Frequency,frameRateScaled);
    DWORD bufferPos = samplePos * Format.nBlockAlign;
    DWORD nextSize = bufferPos - SamplesPlayed;

    return nextSize;
  }

  DWORD WriteCursor()
  {
    if(!Playing)
      return 0;

    return (PlayCursor + 128 * Format.nBlockAlign) % Bytes;
  }

public:
  MyDirectSoundBuffer8(DWORD flags,DWORD bufBytes,LPWAVEFORMATEX fmt)
    : RefCount(1)
  {
    Flags = flags;
    Buffer = new BYTE[bufBytes];
    Bytes = bufBytes;
    memset(Buffer,0,bufBytes);

    if(fmt)
      Format = *fmt;
    else
    {
      Format.wFormatTag = WAVE_FORMAT_PCM;
      Format.nChannels = 2;
      Format.nSamplesPerSec = 44100;
      Format.nAvgBytesPerSec = 176400;
      Format.nBlockAlign = 4;
      Format.wBitsPerSample = 16;
      Format.cbSize = 0;
    }

    Frequency = Format.nSamplesPerSec;

    Playing = FALSE;
    Looping = FALSE;
    Volume = 0;
    PlayCursor = 0;
    SkipAllowed = FALSE;
    SamplesPlayed = 0;
    GetPosThisFrame = 0;

    InitializeCriticalSection(&BufferLock);
  }

  ~MyDirectSoundBuffer8()
  {
    DeleteCriticalSection(&BufferLock);
    delete[] Buffer;
  }

  // IUnknown methods
  virtual HRESULT __stdcall QueryInterface(REFIID iid,LPVOID *ptr)
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

  // IDirectSoundBuffer methods
  virtual HRESULT __stdcall GetCaps(LPDSBCAPS caps)
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

  virtual HRESULT __stdcall GetCurrentPosition(LPDWORD pdwCurrentPlayCursor,LPDWORD pdwCurrentWriteCursor)
  {
    EnterCriticalSection(&BufferLock);

    // skip some milliseconds of silence at start
    if(SkipAllowed)
    {
      DWORD maxskip = UMulDiv(Format.nSamplesPerSec,Format.nBlockAlign*params.SoundMaxSkip,1000);
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
      if(pdwCurrentPlayCursor)
        *pdwCurrentPlayCursor = PlayCursor;

      if(pdwCurrentWriteCursor)
        *pdwCurrentWriteCursor = WriteCursor();
    }

    LeaveCriticalSection(&BufferLock);

    return S_OK;
  }

  virtual HRESULT __stdcall GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten)
  {
    int size = min(dwSizeAllocated,sizeof(WAVEFORMATEX));

    if(pdwSizeWritten)
      *pdwSizeWritten = size;

    if(pwfxFormat)
      memcpy(pwfxFormat,&Format,size);

    return S_OK;
  }

  virtual HRESULT __stdcall GetVolume(LPLONG plVolume)
  {
    if(plVolume)
      *plVolume = Volume;

    return S_OK;
  }

  virtual HRESULT __stdcall GetPan(LPLONG plPan)
  {
    printLog("sound: dsound getpan\n");
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetFrequency(LPDWORD pdwFrequency)
  {
    if(pdwFrequency)
      *pdwFrequency = Frequency;

    return S_OK;
  }

  virtual HRESULT __stdcall GetStatus(LPDWORD pdwStatus)
  {
    if(pdwStatus)
    {
      *pdwStatus = 0;
      *pdwStatus |= Playing ? DSBSTATUS_PLAYING : 0;
      *pdwStatus |= Looping ? DSBSTATUS_LOOPING : 0;
    }

    return DS_OK;
  }

  virtual HRESULT __stdcall Initialize(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc)
  {
    return S_OK;
  }

  virtual HRESULT __stdcall Lock(DWORD dwOffset,DWORD dwBytes,LPVOID *ppvAudioPtr1,LPDWORD pdwAudioBytes1,LPVOID *ppvAudioPtr2,LPDWORD pdwAudioBytes2,DWORD dwFlags)
  {
    EnterCriticalSection(&BufferLock);

    if(dwFlags & DSBLOCK_FROMWRITECURSOR)
      dwOffset = WriteCursor();

    if(dwFlags & DSBLOCK_ENTIREBUFFER)
      dwBytes = Bytes;

    if(dwOffset >= Bytes || !dwBytes || dwBytes > Bytes)
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

  virtual HRESULT __stdcall Play(DWORD dwReserved1,DWORD dwPriority,DWORD dwFlags)
  {
    if(!Playing)
      SkipAllowed = TRUE;

    Playing = TRUE;
    Looping = (dwFlags & DSBPLAY_LOOPING) ? TRUE : FALSE;

    if(!(Flags & DSBCAPS_PRIMARYBUFFER))
    {
      encoder->SetAudioFormat(&Format);
      playBuffer = this;
      FirstFrame = getFrameTiming();
    }

    return S_OK;
  }

  virtual HRESULT __stdcall SetCurrentPosition(DWORD dwNewPosition)
  {
    PlayCursor = dwNewPosition;
    return S_OK;
  }

  virtual HRESULT __stdcall SetFormat(LPCWAVEFORMATEX pcfxFormat)
  {
    Format = *pcfxFormat;
    return S_OK;
  }

  virtual HRESULT __stdcall SetVolume(LONG lVolume)
  {
    Volume = lVolume;
    return S_OK;
  }

  virtual HRESULT __stdcall SetPan(LONG lPan)
  {
    printLog("sound: dsound setpan\n");
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall SetFrequency(DWORD dwFrequency)
  {
    Frequency = dwFrequency;
    return S_OK;
  }

  virtual HRESULT __stdcall Stop()
  {
    Playing = FALSE;
    if(playBuffer == this)
      playBuffer = 0;

    return S_OK;
  }

  virtual HRESULT __stdcall Unlock(LPVOID pvAudioPtr1,DWORD dwAudioBytes1,LPVOID pvAudioPtr2,DWORD dwAudioBytes2)
  {
    LeaveCriticalSection(&BufferLock);

    return S_OK;
  }

  virtual HRESULT __stdcall Restore()
  {
    return S_OK;
  }

  // IDirectSoundBuffer8 methods
  virtual HRESULT __stdcall SetFX(DWORD dwEffectsCount,LPDSEFFECTDESC pDSFXDesc,LPDWORD pdwResultCodes)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall AcquireResources(DWORD dwFlags,DWORD dwEffectsCount,LPDWORD pdwResultCodes)
  {
    return E_NOTIMPL;
  }

  virtual HRESULT __stdcall GetObjectInPath(REFGUID rguidObject,DWORD dwIndex,REFGUID rguidInterface,LPVOID *ppObject)
  {
    return E_NOTIMPL;
  }

  // ----
  void encodeLastFrameAudio()
  {
    EnterCriticalSection(&BufferLock);

    // calculate number of samples processed since last frame, then encode
    DWORD frameSize = NextFrameSize();
    DWORD end = PlayCursor + frameSize;
    DWORD align = Format.nBlockAlign;

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

    LeaveCriticalSection(&BufferLock);
  }
};

class MyDirectSound8 : public IDirectSound8
{
  int RefCount;

public:
  MyDirectSound8()
    : RefCount(1)
  {
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
      pDSCaps->dwMaxSecondarySampleRate = 48000;
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
    printLog("sound: DuplicateSoundBuffer attempted (not implemented yet)\n");
    return E_NOTIMPL;
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

// --- now, waveout

class WaveOutImpl;
static WaveOutImpl *currentWaveOut = 0;

class WaveOutImpl
{
  char MagicCookie[16];
  WAVEFORMATEX Format;
  DWORD_PTR Callback;
  DWORD_PTR CallbackInstance;
  DWORD OpenFlags;
  WAVEHDR *Head,*Current,*Tail;
  bool Paused,InLoop;
  int FirstFrame;
  int FirstWriteFrame;
  DWORD CurrentBufferPos;
  DWORD CurrentSamplePos;

  void callbackMessage(UINT uMsg,DWORD dwParam1,DWORD dwParam2)
  {
    switch(OpenFlags & CALLBACK_TYPEMASK)
    {
    case CALLBACK_EVENT:
      SetEvent((HANDLE) Callback);
      break;

    case CALLBACK_FUNCTION:
      ((PDRVCALLBACK) Callback)((HDRVR) this,uMsg,CallbackInstance,dwParam1,dwParam2);
      break;

    case CALLBACK_THREAD:
      PostThreadMessage((DWORD) Callback,uMsg,(WPARAM) this,(LPARAM) dwParam1);
      break;

    case CALLBACK_WINDOW:
      PostMessage((HWND) Callback,uMsg,(WPARAM) this,(LPARAM) dwParam1);
      break;
    }
  }

  void doneBuffer()
  {
    if(Head && Head == Current)
    {
      // mark current buffer as done and advance
      Current->dwFlags = (Current->dwFlags & ~WHDR_INQUEUE) | WHDR_DONE;
      callbackMessage(WOM_DONE,(DWORD) Current,0);

      Current = Current->lpNext;
      Head = Current;
      if(!Head)
        Tail = 0;
    }
    else
      printLog("sound: inconsistent state in waveOut (this is a kkapture bug)\n");
  }

  void advanceBuffer()
  {
    // loops need seperate processing
    if(InLoop && (Current->dwFlags & WHDR_ENDLOOP))
    {
      Current = Head;
      if(!--Current->dwLoops)
        InLoop = false;
      return;
    }

    // current buffer is done, mark it as done and advance
    if(!InLoop)
      doneBuffer();
    else
      Current = Current->lpNext;

    processCurrentBuffer();
  }

  void processCurrentBuffer()
  {
    // process beginloop flag because it may cause a state change
    if(Current && (Current->dwFlags & WHDR_BEGINLOOP) && Current->dwLoops)
      InLoop = true;
  }

public:
  WaveOutImpl(const WAVEFORMATEX *fmt,DWORD_PTR cb,DWORD_PTR cbInstance,DWORD fdwOpen)
  {
    Format = *fmt;
    Callback = cb;
    CallbackInstance = cbInstance;
    OpenFlags = fdwOpen;

    Head = Current = Tail = 0;
    CurrentBufferPos = 0;

    Paused = false;
    InLoop = false;
    FirstFrame = -1;
    FirstWriteFrame = -1;

    CurrentSamplePos = 0;
    memcpy(MagicCookie,"kkapture.waveout",16);

    callbackMessage(WOM_OPEN,0,0);
  }

  ~WaveOutImpl()
  {
    callbackMessage(WOM_CLOSE,0,0);
  }

  bool amIReal() const
  {
    bool result = false;

    __try
    {
      result = memcmp(MagicCookie,"kkapture.waveout",16) == 0;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return result;
  }

  MMRESULT prepareHeader(WAVEHDR *hdr,UINT size)
  {
    if(!hdr || size != sizeof(WAVEHDR))
      return MMSYSERR_INVALPARAM;

    hdr->dwFlags |= WHDR_PREPARED;
    return MMSYSERR_NOERROR;
  }

  MMRESULT unprepareHeader(WAVEHDR *hdr,UINT size)
  {
    if(!hdr || size != sizeof(WAVEHDR))
      return MMSYSERR_INVALPARAM;

    if(hdr->dwFlags & WHDR_INQUEUE)
      return WAVERR_STILLPLAYING;

    hdr->dwFlags &= ~WHDR_PREPARED;
    return MMSYSERR_NOERROR;
  }

  MMRESULT write(WAVEHDR *hdr,UINT size)
  {
    if(!hdr || size != sizeof(WAVEHDR))
      return MMSYSERR_INVALPARAM;

    if(!(hdr->dwFlags & WHDR_PREPARED))
      return WAVERR_UNPREPARED;

    if(hdr->dwFlags & WHDR_INQUEUE)
      return MMSYSERR_NOERROR;

    // enqueue
    if(FirstFrame == -1) // officially start playback!
    {
      FirstFrame = getFrameTiming();
      FirstWriteFrame = FirstFrame;
      encoder->SetAudioFormat(&Format);
      currentWaveOut = this;
    }

    hdr->lpNext = 0;
    hdr->dwFlags = (hdr->dwFlags | WHDR_INQUEUE) & ~WHDR_DONE;

    if(Tail)
    {
      Tail->lpNext = hdr;
      Tail = hdr;
    }
    else
    {
      Head = Current = Tail = hdr;
      processCurrentBuffer();
    }

    return MMSYSERR_NOERROR;
  }

  MMRESULT pause()
  {
    Paused = true;
    return MMSYSERR_NOERROR;
  }

  MMRESULT restart()
  {
    Paused = FALSE;
    return MMSYSERR_NOERROR;
  }

  MMRESULT message(UINT uMsg,DWORD dwParam1,DWORD dwParam2)
  {
    return 0;
  }

  MMRESULT getPosition(MMTIME *mmt,UINT size)
  {
    if(!mmt || size != sizeof(MMTIME))
      return MMSYSERR_INVALPARAM;

    if(mmt->wType != TIME_BYTES && mmt->wType != TIME_SAMPLES && mmt->wType != TIME_MS)
    {
      printLog("sound: unsupported timecode format, defaulting to bytes\n");
      mmt->wType = TIME_BYTES;
      return MMSYSERR_INVALPARAM;
    }

    // current frame (offset corrected)
    DWORD now;
    int relFrame = getFrameTiming() - FirstFrame;

    // calc time in requested format
    switch(mmt->wType)
    {
    case TIME_BYTES:
    case TIME_SAMPLES:
      now = UMulDiv(relFrame*100,Format.nSamplesPerSec,frameRateScaled);
      if(mmt->wType == TIME_BYTES)
        mmt->u.cb = now * Format.nBlockAlign;
      else if(mmt->wType == TIME_SAMPLES)
        mmt->u.sample = now;
      break;

    case TIME_MS:
      mmt->u.ms = UMulDiv(relFrame,100000,frameRateScaled);
      break;
    }

    return MMSYSERR_NOERROR;
  }

  // ----
  void encodeNoAudio(DWORD sampleCount)
  {
    // no new/delete, we do not know from where this might be called
    void *buffer = _alloca(256 * Format.nBlockAlign);
    memset(buffer,0,256 * Format.nBlockAlign);

    while(sampleCount)
    {
      int sampleBlock = min(sampleCount,256);
      encoder->WriteAudioFrame(buffer,sampleBlock);
      sampleCount -= sampleBlock;
    }
  }

  void processFrame()
  {
    // calculate number of samples to write
    int frame = getFrameTiming() - FirstWriteFrame;
    int align = Format.nBlockAlign;

    DWORD sampleNew = UMulDiv(frame * 100,Format.nSamplesPerSec,frameRateScaled);
    DWORD sampleCount = sampleNew - CurrentSamplePos;

    if(!Current || Paused) // write one frame of no audio
    {
      encodeNoAudio(sampleCount);

      // also fix write frame timing
      FirstFrame++;
    }
    else // we have audio playing, so consume buffers as long as possible
    {
      while(sampleCount && Current)
      {
        int smps = min(sampleCount,(Current->dwBufferLength - CurrentBufferPos) / align);
        if(smps)
          encoder->WriteAudioFrame((PBYTE) Current->lpData + CurrentBufferPos,smps);

        sampleCount -= smps;
        CurrentBufferPos += smps * align;
        if(CurrentBufferPos >= Current->dwBufferLength) // buffer done
        {
          advanceBuffer();
          CurrentBufferPos = 0;
        }
      }

      if(sampleCount && !Current) // ran out of audio data
        encodeNoAudio(sampleCount);
    }

    CurrentSamplePos = sampleNew;
  }
};

DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutOpen(LPHWAVEOUT phwo,UINT uDeviceID,LPCWAVEFORMATEX pwfx,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen), waveOutOpen);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutClose(HWAVEOUT hwo), waveOutClose);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutPrepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh), waveOutPrepareHeader);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutUnprepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh), waveOutUnprepareHeader);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutWrite(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh), waveOutWrite);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutPause(HWAVEOUT hwo), waveOutPause);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutRestart(HWAVEOUT hwo), waveOutRestart);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutMessage(HWAVEOUT hwo,UINT uMsg,DWORD_PTR dw1,DWORD_PTR dw2), waveOutMessage);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutGetPosition(HWAVEOUT hwo,LPMMTIME pmmt,UINT cbmmt), waveOutGetPosition);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutGetDevCaps(UINT_PTR uDeviceId,LPWAVEOUTCAPS pwo,UINT cbwoc), waveOutGetDevCaps);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutGetNumDevs(), waveOutGetNumDevs);

static WaveOutImpl *waveOutLast = 0;

static WaveOutImpl *GetWaveOutImpl(HWAVEOUT hwo)
{
  if(hwo)
    return (WaveOutImpl *) hwo;

  return waveOutLast;
}

MMRESULT __stdcall Mine_waveOutOpen(LPHWAVEOUT phwo,UINT uDeviceID,LPCWAVEFORMATEX pwfx,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen)
{
  if(phwo)
  {
    printLog("sound: waveOutOpen %08x (%d hz, %d bits, %d channels)\n",
      uDeviceID,pwfx->nSamplesPerSec,pwfx->wBitsPerSample,pwfx->nChannels);

    WaveOutImpl *impl = new WaveOutImpl(pwfx,dwCallback,dwInstance,fdwOpen);
    waveOutLast = impl;
    *phwo = (HWAVEOUT) impl;

    return MMSYSERR_NOERROR;
  }
  else
  {
    if(fdwOpen & WAVE_FORMAT_QUERY)
      return MMSYSERR_NOERROR;
    else
      return MMSYSERR_INVALPARAM;
  }
}

MMRESULT __stdcall Mine_waveOutClose(HWAVEOUT hwo)
{
  delete GetWaveOutImpl(hwo);

  return MMSYSERR_NOERROR;
}

MMRESULT __stdcall Mine_waveOutPrepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->prepareHeader(pwh,cbwh) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutUnprepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->unprepareHeader(pwh,cbwh) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutWrite(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->write(pwh,cbwh) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutPause(HWAVEOUT hwo)
{
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->pause() : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutRestart(HWAVEOUT hwo)
{
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->restart() : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutMessage(HWAVEOUT hwo,UINT uMsg,DWORD_PTR dw1,DWORD_PTR dw2)
{
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->message(uMsg,(DWORD) dw1,(DWORD) dw2) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutGetPosition(HWAVEOUT hwo,LPMMTIME pmmt,UINT cbmmt)
{
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->getPosition(pmmt,cbmmt) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutGetDevCaps(UINT_PTR uDeviceID,LPWAVEOUTCAPS pwoc,UINT cbwoc)
{
  WaveOutImpl *impl;

  if(uDeviceID == WAVE_MAPPER || uDeviceID == 0)
    impl = waveOutLast;
  else if(uDeviceID < 0x10000)
    return MMSYSERR_BADDEVICEID;
  else
  {
    impl = (WaveOutImpl *) uDeviceID;
    if(!impl->amIReal())
      return MMSYSERR_NODRIVER;
  }

  if(cbwoc < sizeof(WAVEOUTCAPS))
    return MMSYSERR_INVALPARAM;

  pwoc->wMid = MM_MICROSOFT;
  pwoc->wPid = (uDeviceID == WAVE_MAPPER) ? MM_WAVE_MAPPER : MM_MSFT_GENERIC_WAVEOUT;
  pwoc->vDriverVersion = 0x100;
  strcpy(pwoc->szPname,".kkapture Audio");
  pwoc->dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1M16 | WAVE_FORMAT_1S08 | WAVE_FORMAT_1S16
    | WAVE_FORMAT_2M08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2S16
    | WAVE_FORMAT_4M08 | WAVE_FORMAT_4M16 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4S16;
  pwoc->wChannels = 2;
  pwoc->wReserved1 = 0;
  pwoc->dwSupport = WAVECAPS_PLAYBACKRATE | WAVECAPS_SAMPLEACCURATE;

  return MMSYSERR_NOERROR;
}

UINT __stdcall Mine_waveOutGetNumDevs()
{
  return 1;
}

// ----

void initSound()
{
  DetourFunctionWithTrampoline((PBYTE) Real_DirectSoundCreate,(PBYTE) Mine_DirectSoundCreate);
  DetourFunctionWithTrampoline((PBYTE) Real_DirectSoundCreate8,(PBYTE) Mine_DirectSoundCreate8);

  DetourFunctionWithTrampoline((PBYTE) Real_CoCreateInstance,(PBYTE) Mine_CoCreateInstance);

  DetourFunctionWithTrampoline((PBYTE) Real_waveOutOpen,(PBYTE) Mine_waveOutOpen);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutClose,(PBYTE) Mine_waveOutClose);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutPrepareHeader,(PBYTE) Mine_waveOutPrepareHeader);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutUnprepareHeader,(PBYTE) Mine_waveOutUnprepareHeader);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutWrite,(PBYTE) Mine_waveOutWrite);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutPause,(PBYTE) Mine_waveOutPause);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutRestart,(PBYTE) Mine_waveOutRestart);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutMessage,(PBYTE) Mine_waveOutMessage);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutGetPosition,(PBYTE) Mine_waveOutGetPosition);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutGetDevCaps, (PBYTE) Mine_waveOutGetDevCaps);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutGetNumDevs, (PBYTE) Mine_waveOutGetNumDevs);
}

void doneSound()
{
}

void nextFrameSound()
{
  if(playBuffer)
    playBuffer->encodeLastFrameAudio();
  else if(currentWaveOut)
    currentWaveOut->processFrame();
}
// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include <malloc.h>
#include "videoencoder.h"

// my own directsound fake!
class MyDirectSound8;
class MyDirectSoundBuffer8;

static MyDirectSoundBuffer8 *playBuffer = 0;

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
  int PlayBaseFrame;
  int LastFrameRead;

  DWORD FrameToPosition(int frame)
  {
    DWORD relFrame = frame - PlayBaseFrame;
    DWORD samplePos = DWORD(1.0 * relFrame * Frequency / frameRate);
    DWORD bufferPos = samplePos * Format.nBlockAlign;

    if(bufferPos > Bytes)
    {
      if(!Looping)
        bufferPos = 0;
      else
        bufferPos %= Bytes;
    }

    return bufferPos;
  }

  DWORD PlayCursor()
  {
    if(!Playing)
      return 0;

    return FrameToPosition(getFrameTiming());
  }

  DWORD WriteCursor()
  {
    if(!Playing)
      return 0;

    return (PlayCursor() + 128 * Format.nBlockAlign) % Bytes;
  }

public:
  MyDirectSoundBuffer8(DWORD flags,DWORD bufBytes,LPWAVEFORMATEX fmt)
    : RefCount(1)
  {
    Flags = flags;
    Buffer = new BYTE[bufBytes];
    Bytes = bufBytes;

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
    LastFrameRead = 0;

    Playing = FALSE;
    Looping = FALSE;
    Volume = 0;

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

    if(pdwCurrentPlayCursor)
      *pdwCurrentPlayCursor = PlayCursor();

    if(pdwCurrentWriteCursor)
      *pdwCurrentWriteCursor = WriteCursor();

    LeaveCriticalSection(&BufferLock);
    return S_OK;
  }

  virtual HRESULT __stdcall GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten)
  {
    int size = min(dwSizeAllocated,sizeof(WAVEFORMATEX));

    if(pdwSizeWritten)
      *pdwSizeWritten = size;

    if(pwfxFormat)
      memcpy(pwfxFormat,&Format,*pdwSizeWritten);

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
    {
      dwOffset = 0;
      dwBytes = Bytes;
    }

    if(dwBytes > Bytes)
      dwBytes = Bytes;

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
    Playing = TRUE;
    Looping = (dwFlags & DSBPLAY_LOOPING) ? TRUE : FALSE;
    PlayBaseFrame = getFrameTiming();

    if(!(Flags & DSBCAPS_PRIMARYBUFFER))
    {
      encoder->SetAudioFormat(&Format);
      playBuffer = this;
    }

    return S_OK;
  }

  virtual HRESULT __stdcall SetCurrentPosition(DWORD dwNewPosition)
  {
    LastFrameRead = dwNewPosition;
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
    int cursor = PlayCursor();
    int diff = cursor - LastFrameRead;
    int rdiff = diff > 0 ? diff : diff + Bytes;
    int align = Format.nBlockAlign;

    if(diff <= 0) // wrap
    {
      encoder->WriteAudioFrame(Buffer + LastFrameRead,(Bytes - LastFrameRead) / align);
      encoder->WriteAudioFrame(Buffer,cursor / align);
    }
    else // no wrap
      encoder->WriteAudioFrame(Buffer + LastFrameRead,diff / align);

    LastFrameRead = cursor;
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
    if(pDSCaps->dwSize == sizeof(DSCAPS))
    {
      ZeroMemory(pDSCaps,sizeof(DSCAPS));

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
    printLog("sound: dsound initialize\n");
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
static WaveOutImpl *currentWaveOut;

class WaveOutImpl
{
  WAVEFORMATEX Format;
  DWORD_PTR Callback;
  DWORD_PTR CallbackInstance;
  DWORD OpenFlags;
  WAVEHDR *Head,*Tail,*Current;
  BOOL Paused,InLoop;
  int FirstFrame;
  int FirstWriteFrame;
  DWORD CurrentBufferPos;

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

  void doneNextBuffer(WAVEHDR *start)
  {
    WAVEHDR *buffer = start->lpNext;

    // dequeue, unlink
    buffer->dwFlags = (buffer->dwFlags & ~WHDR_INQUEUE) | WHDR_DONE;
    start->lpNext = buffer->lpNext;

    if(buffer == Tail)
      Tail = start;

    // give callback message
    callbackMessage(WOM_DONE,(DWORD) buffer,0);
  }

  void advanceBuffer()
  {
    WAVEHDR *buffer = Current->lpNext;

    // loops need seperate processing
    if(InLoop && (buffer->dwFlags & WHDR_ENDLOOP))
    {
      // find beginning of loop
      while(!(Current->lpNext->dwFlags & WHDR_BEGINLOOP))
        Current = Current->lpNext;

      // decrement loop count. if it's not zero yet, we're done
      if(--Current->lpNext->dwLoops)
        return;
      else
      {
        InLoop = FALSE;
        return;
      }
    }

    // current buffer is done, untag it and advance to next buffer
    if(!InLoop)
      doneNextBuffer(Current);
    else
      Current = Current->lpNext;

    // process flags to know which state we're in now
    if((Current->lpNext->dwFlags & WHDR_BEGINLOOP) && Current->lpNext->dwLoops)
      InLoop = TRUE;

    if(!InLoop && (Current->lpNext->dwFlags & WHDR_ENDLOOP))
      Current->lpNext->dwFlags &= ~WHDR_ENDLOOP;
  }

public:
  WaveOutImpl(const WAVEFORMATEX *fmt,DWORD_PTR cb,DWORD_PTR cbInstance,DWORD fdwOpen)
  {
    Format = *fmt;
    Callback = cb;
    CallbackInstance = cbInstance;
    OpenFlags = fdwOpen;

    Head = new WAVEHDR;
    ZeroMemory(Head,sizeof(*Head));
    Head->lpData = 0;
    Head->dwBufferLength = 0;
    Head->lpNext = Head;
    Tail = Head;
    Current = Head;
    CurrentBufferPos = 0;

    Paused = FALSE;
    InLoop = FALSE;
    FirstFrame = -1;
    FirstWriteFrame = -1;

    callbackMessage(WOM_OPEN,0,0);
  }

  ~WaveOutImpl()
  {
    callbackMessage(WOM_CLOSE,0,0);
  }

  MMRESULT prepareHeader(WAVEHDR *hdr,UINT size)
  {
    printLog("sound: waveOutPrepareHeader\n");

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
    printLog("sound: waveOutWrite\n");

    if(!hdr || size != sizeof(WAVEHDR))
      return MMSYSERR_INVALPARAM;

    if(!(hdr->dwFlags & WHDR_PREPARED))
      return WAVERR_UNPREPARED;

    if(hdr->dwFlags & WHDR_INQUEUE)
      return MMSYSERR_NOERROR;

    // enqueue
    if(FirstWriteFrame == -1) // officially start playback!
    {
      FirstWriteFrame = getFrameTiming();
      FirstFrame = FirstWriteFrame;

      currentWaveOut = this;
    }

    Tail->lpNext = hdr;
    Tail = hdr;

    hdr->lpNext = Head;
    hdr->dwFlags |= WHDR_INQUEUE;

    callbackMessage(WOM_DONE,(DWORD) hdr,0);

    return MMSYSERR_NOERROR;
  }

  MMRESULT pause()
  {
    Paused = TRUE;
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
    printLog("sound: waveOutGetPosition\n");

    if(!mmt || size != sizeof(MMTIME))
      return MMSYSERR_INVALPARAM;

    if(mmt->wType != TIME_BYTES && mmt->wType != TIME_SAMPLES && mmt->wType != TIME_MS)
    {
      mmt->wType = TIME_SAMPLES;
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
      now = DWORD(1.0 * relFrame * Format.nSamplesPerSec / frameRate);
      if(mmt->wType == TIME_BYTES)
        mmt->u.cb = now * Format.nBlockAlign;
      else if(mmt->wType == TIME_SAMPLES)
        mmt->u.sample = now;
      break;

    case TIME_MS:
      mmt->u.ms = DWORD(relFrame * 1000.0 / frameRate);
      break;
    }

    return MMSYSERR_NOERROR;
  }

  // ----
  void encodeNoAudio(DWORD sampleCount)
  {
    // no new/delete, we do not know from where this might be called
    void *buffer = _alloca(sampleCount * Format.nBlockAlign);
    memset(buffer,0,sampleCount * Format.nBlockAlign);
    encoder->WriteAudioFrame(buffer,sampleCount);
  }

  void processFrame()
  {
    // calculate number of samples to write
    int frame = getFrameTiming() - FirstWriteFrame;
    int align = Format.nBlockAlign;
    DWORD sampleOld = DWORD(1.0 * (frame - 1) * Format.nSamplesPerSec / frameRate);
    DWORD sampleNew = DWORD(1.0 * frame * Format.nSamplesPerSec / frameRate);
    DWORD sampleCount = sampleNew - sampleOld;

    if(Head->lpNext == Head || Paused) // write one frame of no audio
    {
      encodeNoAudio(sampleCount);

      // also fix write frame timing
      FirstFrame++;
    }
    else // we have audio playing, so consume buffers as long as possible
    {
      while(sampleCount && Current != Head)
      {
        WAVEHDR *buffer = Current->lpNext;
        int useBytes = min(sampleCount*align,buffer->dwBufferLength - CurrentBufferPos);

        if(useBytes)
          encoder->WriteAudioFrame((PBYTE) buffer->lpData + CurrentBufferPos,useBytes/align);

        CurrentBufferPos += useBytes;
        if(CurrentBufferPos >= buffer->dwBufferLength) // buffer done
        {
          advanceBuffer();
          CurrentBufferPos = 0;
        }
      }

      if(sampleCount && Current == Head) // ran out of audio data
        encodeNoAudio(sampleCount);
    }

    printLog("sound: frame finished\n");
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

MMRESULT __stdcall Mine_waveOutOpen(LPHWAVEOUT phwo,UINT uDeviceID,LPCWAVEFORMATEX pwfx,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen)
{
  if(phwo)
  {
    printLog("sound: waveOutOpen %08x (%d hz, %d bits, %d channels)\n",
      uDeviceID,pwfx->nSamplesPerSec,pwfx->wBitsPerSample,pwfx->nChannels);

    WaveOutImpl *impl = new WaveOutImpl(pwfx,dwCallback,dwInstance,fdwOpen);
    *phwo = (HWAVEOUT) impl;
    return MMSYSERR_NOERROR;
  }
  else
  {
    if(fdwOpen & WAVE_FORMAT_QUERY)
    {
      printLog("sound: query %d hz, %d bits, %d channels\n",
        pwfx->nSamplesPerSec,pwfx->wBitsPerSample,pwfx->nChannels);
      return MMSYSERR_NOERROR;
    }
    else
      return MMSYSERR_INVALPARAM;
  }
}

MMRESULT __stdcall Mine_waveOutClose(HWAVEOUT hwo)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  delete impl;

  printLog("sound: waveOutClose\n");

  return MMSYSERR_NOERROR;
}

MMRESULT __stdcall Mine_waveOutPrepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  return impl->prepareHeader(pwh,cbwh);
}

MMRESULT __stdcall Mine_waveOutUnprepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  return impl->unprepareHeader(pwh,cbwh);
}

MMRESULT __stdcall Mine_waveOutWrite(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  return impl->write(pwh,cbwh);
}

MMRESULT __stdcall Mine_waveOutPause(HWAVEOUT hwo)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  return impl->pause();
}

MMRESULT __stdcall Mine_waveOutRestart(HWAVEOUT hwo)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  return impl->restart();
}

MMRESULT __stdcall Mine_waveOutMessage(HWAVEOUT hwo,UINT uMsg,DWORD_PTR dw1,DWORD_PTR dw2)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  return impl->message(uMsg,(DWORD) dw1,(DWORD) dw2);
}

MMRESULT __stdcall Mine_waveOutGetPosition(HWAVEOUT hwo,LPMMTIME pmmt,UINT cbmmt)
{
  WaveOutImpl *impl = (WaveOutImpl *) hwo;
  return impl->getPosition(pmmt,cbmmt);
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
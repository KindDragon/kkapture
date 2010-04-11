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

#ifndef __SOUND_DIRECTSOUND_H__
#define __SOUND_DIRECTSOUND_H__

#define DIRECTSOUND_VERSION 0x0800
#include <dsound.h>


class MyDirectSoundBuffer8 : public IDirectSoundBuffer8
{
  int RefCount;
  PBYTE Buffer;
  DWORD Flags;
  DWORD Bytes;
  WAVEFORMATEX *Format;
  DWORD Frequency;
  BOOL Playing,Looping;
  LONG Volume;
  LONG Panning;
  CRITICAL_SECTION BufferLock;
  DWORD PlayCursor;
  BOOL SkipAllowed;
  DWORD SamplesPlayed;
  DWORD GetPosThisFrame;
  int FirstFrame;

  DWORD NextFrameSize()
  {
    DWORD frame = getFrameTiming() - FirstFrame;
    DWORD samplePos = UMulDiv(frame,Frequency * frameRateDenom,frameRateScaled);
    DWORD bufferPos = samplePos * Format->nBlockAlign;
    DWORD nextSize = bufferPos - SamplesPlayed;

    return nextSize;
  }

  DWORD WriteCursor()
  {
    if(!Playing)
      return 0;

    return (PlayCursor + 128 * Format->nBlockAlign) % Bytes;
  }

public:
  MyDirectSoundBuffer8(DWORD flags,DWORD bufBytes,LPWAVEFORMATEX fmt);

  ~MyDirectSoundBuffer8();

  // IUnknown methods
  virtual HRESULT __stdcall QueryInterface(REFIID iid,LPVOID *ptr);

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
  virtual HRESULT __stdcall GetCaps(LPDSBCAPS caps);
  virtual HRESULT __stdcall GetCurrentPosition(LPDWORD pdwCurrentPlayCursor,LPDWORD pdwCurrentWriteCursor);
  virtual HRESULT __stdcall GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten);
  virtual HRESULT __stdcall GetVolume(LPLONG plVolume);
  virtual HRESULT __stdcall GetPan(LPLONG plPan);
  virtual HRESULT __stdcall GetFrequency(LPDWORD pdwFrequency);
  virtual HRESULT __stdcall GetStatus(LPDWORD pdwStatus);
  virtual HRESULT __stdcall Initialize(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc);
  virtual HRESULT __stdcall Lock(DWORD dwOffset,DWORD dwBytes,LPVOID *ppvAudioPtr1,LPDWORD pdwAudioBytes1,LPVOID *ppvAudioPtr2,LPDWORD pdwAudioBytes2,DWORD dwFlags);
  virtual HRESULT __stdcall Play(DWORD dwReserved1,DWORD dwPriority,DWORD dwFlags);
  virtual HRESULT __stdcall SetCurrentPosition(DWORD dwNewPosition);
  virtual HRESULT __stdcall SetFormat(LPCWAVEFORMATEX pcfxFormat);
  virtual HRESULT __stdcall SetVolume(LONG lVolume);
  virtual HRESULT __stdcall SetPan(LONG lPan);
  virtual HRESULT __stdcall SetFrequency(DWORD dwFrequency);
  virtual HRESULT __stdcall Stop();

  virtual HRESULT __stdcall Unlock(LPVOID pvAudioPtr1,DWORD dwAudioBytes1,LPVOID pvAudioPtr2,DWORD dwAudioBytes2)
  {
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
  void encodeLastFrameAudio();
};

extern MyDirectSoundBuffer8 *playBuffer;

void initSoundsysDirectSound();

#endif
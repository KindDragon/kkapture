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
#include "sound_waveout.h"

#pragma comment(lib,"winmm.lib")

// if waveOutGetPosition is called frequently in a single frame, assume the app is waiting for the
// current playback position to change and advance the time. this is the threshold for "frequent" calls.
static const int MAX_GETPOSITION_PER_FRAME = 1024;

// --- now, waveout

WaveOutImpl *currentWaveOut = 0;

  void WaveOutImpl::callbackMessage(UINT uMsg,DWORD dwParam1,DWORD dwParam2)
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

  void WaveOutImpl::doneBuffer()
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

  void WaveOutImpl::advanceBuffer()
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

  void WaveOutImpl::processCurrentBuffer()
  {
    // process beginloop flag because it may cause a state change
    if(Current && (Current->dwFlags & WHDR_BEGINLOOP) && Current->dwLoops)
      InLoop = true;
  }

  WaveOutImpl::WaveOutImpl(const WAVEFORMATEX *fmt,DWORD_PTR cb,DWORD_PTR cbInstance,DWORD fdwOpen)
  {
    videoNeedEncoder();

    Format = CopyFormat(fmt);
    Callback = cb;
    CallbackInstance = cbInstance;
    OpenFlags = fdwOpen;

    Head = Current = Tail = 0;
    CurrentBufferPos = 0;

    Paused = false;
    InLoop = false;
    FirstFrame = 0;
    FirstWriteFrame = 0;
    FrameInitialized = false;
    GetPositionCounter = 0;

    CurrentSamplePos = 0;
    memcpy(MagicCookie,"kkapture.waveout",16);

    callbackMessage(WOM_OPEN,0,0);
  }

  WaveOutImpl::~WaveOutImpl()
  {
    delete Format;
    callbackMessage(WOM_CLOSE,0,0);
  }

  bool WaveOutImpl::amIReal() const
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

  MMRESULT WaveOutImpl::prepareHeader(WAVEHDR *hdr,UINT size)
  {
    if(!hdr || size != sizeof(WAVEHDR))
      return MMSYSERR_INVALPARAM;

    hdr->dwFlags |= WHDR_PREPARED;
    return MMSYSERR_NOERROR;
  }

  MMRESULT WaveOutImpl::unprepareHeader(WAVEHDR *hdr,UINT size)
  {
    if(!hdr || size != sizeof(WAVEHDR))
      return MMSYSERR_INVALPARAM;

    if(hdr->dwFlags & WHDR_INQUEUE)
      return WAVERR_STILLPLAYING;

    hdr->dwFlags &= ~WHDR_PREPARED;
    return MMSYSERR_NOERROR;
  }

  MMRESULT WaveOutImpl::write(WAVEHDR *hdr,UINT size)
  {
    if(!hdr || size != sizeof(WAVEHDR))
      return MMSYSERR_INVALPARAM;

    if(!(hdr->dwFlags & WHDR_PREPARED))
      return WAVERR_UNPREPARED;

    if(hdr->dwFlags & WHDR_INQUEUE)
      return MMSYSERR_NOERROR;

    // enqueue
    if(!FrameInitialized) // officially start playback!
    {
      FirstFrame = getFrameTiming();
      FirstWriteFrame = FirstFrame;
      FrameInitialized = true;
      encoder->SetAudioFormat(Format);
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

  MMRESULT WaveOutImpl::pause()
  {
    Paused = true;
    return MMSYSERR_NOERROR;
  }

  MMRESULT WaveOutImpl::restart()
  {
    Paused = FALSE;
    return MMSYSERR_NOERROR;
  }

  MMRESULT WaveOutImpl::reset()
  {
    while(Head)
      doneBuffer();

    CurrentBufferPos = 0;
    CurrentSamplePos = 0;

    Paused = false;
    InLoop = false;
    FirstFrame = 0;
    FirstWriteFrame = 0;
    FrameInitialized = false;

    return MMSYSERR_NOERROR;
  }

  MMRESULT WaveOutImpl::message(UINT uMsg,DWORD dwParam1,DWORD dwParam2)
  {
    return 0;
  }

  MMRESULT WaveOutImpl::getPosition(MMTIME *mmt,UINT size)
  {
    if(++GetPositionCounter >= MAX_GETPOSITION_PER_FRAME) // assume that the app is waiting for the waveout position to change.
    {
      printLog("sound: app is hammering waveOutGetPosition, advancing time (frame=%d)\n",getFrameTiming());
      GetPositionCounter = 0;
      skipFrame();
    }

    if(!mmt || size < sizeof(MMTIME))
    {
      printLog("sound: invalid param to waveOutGetPosition");
      return MMSYSERR_INVALPARAM;
    }

    if(size > sizeof(MMTIME))
    {
      static bool warnedAboutMMTime = false;
      if(!warnedAboutMMTime)
      {
        printLog("sound: MMTIME structure passed to waveOutGetPosition is too large, ignoring extra fields (will only report this once).\n");
        warnedAboutMMTime = true;
      }
    }

    if(mmt->wType != TIME_BYTES && mmt->wType != TIME_SAMPLES && mmt->wType != TIME_MS)
    {
      printLog("sound: unsupported timecode format, defaulting to bytes.\n");
      mmt->wType = TIME_BYTES;
      return MMSYSERR_INVALPARAM;
    }

    // current frame (offset corrected)
    int relFrame = getFrameTiming() - FirstFrame;
    DWORD now = UMulDiv(relFrame,Format->nSamplesPerSec * frameRateDenom,frameRateScaled);

    // calc time in requested format
    switch(mmt->wType)
    {
    case TIME_BYTES:
    case TIME_MS:
      // yes, TIME_MS seems to return *bytes*. WHATEVER.
      mmt->u.cb = now * Format->nBlockAlign;
      break;

    case TIME_SAMPLES:
      mmt->u.sample = now;
      break;
    }

    return MMSYSERR_NOERROR;
  }

  // ----
  void WaveOutImpl::encodeNoAudio(DWORD sampleCount)
  {
    // no new/delete, we do not know from where this might be called
    void *buffer = _alloca(256 * Format->nBlockAlign);
    memset(buffer,0,256 * Format->nBlockAlign);

    while(sampleCount)
    {
      int sampleBlock = min(sampleCount,256);
      encoder->WriteAudioFrame(buffer,sampleBlock);
      sampleCount -= sampleBlock;
    }
  }

  void WaveOutImpl::processFrame()
  {
    GetPositionCounter = 0;

    // calculate number of samples to write
    int frame = getFrameTiming() - FirstWriteFrame;
    int align = Format->nBlockAlign;

    DWORD sampleNew = UMulDiv(frame,Format->nSamplesPerSec * frameRateDenom,frameRateScaled);
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

DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutOpen(LPHWAVEOUT phwo,UINT uDeviceID,LPCWAVEFORMATEX pwfx,DWORD_PTR dwCallback,DWORD_PTR dwInstance,DWORD fdwOpen), waveOutOpen);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutClose(HWAVEOUT hwo), waveOutClose);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutPrepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh), waveOutPrepareHeader);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutUnprepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh), waveOutUnprepareHeader);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutWrite(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh), waveOutWrite);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutPause(HWAVEOUT hwo), waveOutPause);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutRestart(HWAVEOUT hwo), waveOutRestart);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_waveOutReset(HWAVEOUT hwo), waveOutReset);
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
  TRACE(("waveOutOpen(%p,%u,%p,%p,%p,%u)\n",phwo,uDeviceID,pwfx,dwCallback,dwInstance,fdwOpen));

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
  TRACE(("waveOutClose(%p)\n",hwo));

  printLog("sound: waveOutClose %08x\n",hwo);

  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  if(impl)
  {
    delete impl;
    if(impl == waveOutLast)
      waveOutLast = 0;

    return MMSYSERR_NOERROR;
  }
  else
    return MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutPrepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  TRACE(("waveOutPrepareHeader(%p,%p,%u)\n",hwo,pwh,cbwh));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->prepareHeader(pwh,cbwh) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutUnprepareHeader(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  TRACE(("waveOutUnprepareHeader(%p,%p,%u)\n",hwo,pwh,cbwh));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->unprepareHeader(pwh,cbwh) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutWrite(HWAVEOUT hwo,LPWAVEHDR pwh,UINT cbwh)
{
  TRACE(("waveOutWrite(%p,%p,%u)\n",hwo,pwh,cbwh));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->write(pwh,cbwh) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutPause(HWAVEOUT hwo)
{
  TRACE(("waveOutPause(%p)\n",hwo));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->pause() : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutRestart(HWAVEOUT hwo)
{
  TRACE(("waveOutRestart(%p)\n",hwo));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->restart() : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutReset(HWAVEOUT hwo)
{
  TRACE(("waveOutReset(%p)\n",hwo));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->reset() : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutMessage(HWAVEOUT hwo,UINT uMsg,DWORD_PTR dw1,DWORD_PTR dw2)
{
  TRACE(("waveOutMessage(%p,%u,%p,%p)\n",hwo,uMsg,dw1,dw2));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->message(uMsg,(DWORD) dw1,(DWORD) dw2) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutGetPosition(HWAVEOUT hwo,LPMMTIME pmmt,UINT cbmmt)
{
  TRACE(("waveOutGetPosition(%p,%p,%u)\n",hwo,pmmt,cbmmt));
  WaveOutImpl *impl = GetWaveOutImpl(hwo);
  return impl ? impl->getPosition(pmmt,cbmmt) : MMSYSERR_INVALHANDLE;
}

MMRESULT __stdcall Mine_waveOutGetDevCaps(UINT_PTR uDeviceID,LPWAVEOUTCAPS pwoc,UINT cbwoc)
{
  static const char deviceName[] = ".kkapture Audio";
  WaveOutImpl *impl;

  TRACE(("waveOutGetDevCaps(%p,%p,%u)\n",uDeviceID,pwoc,cbwoc));

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
  memcpy(pwoc->szPname,deviceName,sizeof(deviceName));
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
  TRACE(("waveOutGetNumDevs()\n"));
  return 1;
}

void initSoundsysWaveOut()
{
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutOpen,(PBYTE) Mine_waveOutOpen);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutClose,(PBYTE) Mine_waveOutClose);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutPrepareHeader,(PBYTE) Mine_waveOutPrepareHeader);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutUnprepareHeader,(PBYTE) Mine_waveOutUnprepareHeader);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutWrite,(PBYTE) Mine_waveOutWrite);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutPause,(PBYTE) Mine_waveOutPause);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutReset,(PBYTE) Mine_waveOutReset);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutRestart,(PBYTE) Mine_waveOutRestart);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutMessage,(PBYTE) Mine_waveOutMessage);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutGetPosition,(PBYTE) Mine_waveOutGetPosition);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutGetDevCaps, (PBYTE) Mine_waveOutGetDevCaps);
  DetourFunctionWithTrampoline((PBYTE) Real_waveOutGetNumDevs, (PBYTE) Mine_waveOutGetNumDevs);
}

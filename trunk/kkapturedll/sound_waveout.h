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

#ifndef __SOUND_WAVEOUT_H__
#define __SOUND_WAVEOUT_H__

class WaveOutImpl
{
  char MagicCookie[16];
  WAVEFORMATEX *Format;
  DWORD_PTR Callback;
  DWORD_PTR CallbackInstance;
  DWORD OpenFlags;
  WAVEHDR *Head,*Current,*Tail;
  bool Paused,InLoop;
  int FirstFrame;
  int FirstWriteFrame;
  bool FrameInitialized;
  DWORD CurrentBufferPos;
  DWORD CurrentSamplePos;
  int GetPositionCounter;

  void callbackMessage(UINT uMsg,DWORD dwParam1,DWORD dwParam2);
  void doneBuffer();
  void advanceBuffer();
  void processCurrentBuffer();

public:
  WaveOutImpl(const WAVEFORMATEX *fmt,DWORD_PTR cb,DWORD_PTR cbInstance,DWORD fdwOpen);
  ~WaveOutImpl();
  bool amIReal() const;

  MMRESULT prepareHeader(WAVEHDR *hdr,UINT size);
  MMRESULT unprepareHeader(WAVEHDR *hdr,UINT size);
  MMRESULT write(WAVEHDR *hdr,UINT size);
  MMRESULT pause();
  MMRESULT restart();
  MMRESULT reset();
  MMRESULT message(UINT uMsg,DWORD dwParam1,DWORD dwParam2);
  MMRESULT getPosition(MMTIME *mmt,UINT size);

  // ----
  void encodeNoAudio(DWORD sampleCount);
  void processFrame();
};

extern WaveOutImpl *currentWaveOut;

void initSoundsysWaveOut();

#endif
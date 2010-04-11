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
#include <psapi.h>
#include "sound_fmod.h"

#pragma comment(lib,"psapi.lib")

// ---- FMOD 3.xx

typedef int (__stdcall *PFSOUND_STREAM_PLAY)(int channel,void *stream);
typedef int (__stdcall *PFSOUND_STREAM_PLAYEX)(int channel,void *stream,void *dspunit,signed char paused);
typedef signed char (__stdcall *PFSOUND_STREAM_STOP)(void *stream);
typedef int (__stdcall *PFSOUND_STREAM_GETTIME)(void *stream);
typedef int (__stdcall *PFSOUND_PLAYSOUND)(int channel,void *sample);
typedef int (__stdcall *PFSOUND_GETFREQUENCY)(int channel);
typedef unsigned int (__stdcall *PFSOUND_GETCURRENTPOSITION)(int channel);

static PFSOUND_STREAM_PLAY Real_FSOUND_Stream_Play = 0;
static PFSOUND_STREAM_PLAYEX Real_FSOUND_Stream_PlayEx = 0;
static PFSOUND_STREAM_STOP Real_FSOUND_Stream_Stop = 0;
static PFSOUND_STREAM_GETTIME Real_FSOUND_Stream_GetTime = 0;
static PFSOUND_PLAYSOUND Real_FSOUND_PlaySound = 0;
static PFSOUND_GETFREQUENCY Real_FSOUND_GetFrequency = 0;
static PFSOUND_GETCURRENTPOSITION Real_FSOUND_GetCurrentPosition = 0;

static void *FMODStart = 0, *FMODEnd = 0;

#define CalledFromFMOD() (_ReturnAddress() >= FMODStart && _ReturnAddress() < FMODEnd) // needs to be a macro

struct FMODStreamDesc
{
  void *stream;
  int channel;
  int firstFrame;
  int frequency;
};

static const int FMODNumStreams = 16; // max # of active (playing) streams supported
static FMODStreamDesc FMODStreams[FMODNumStreams];

static void RegisterFMODStream(void *stream,int channel)
{
  if(channel == -1) // error from fmod
    return;

  // find a free stream desc
  int index = 0;
  while(index<FMODNumStreams && FMODStreams[index].stream)
    index++;

  if(index==FMODNumStreams)
  {
    printLog("sound/fmod: more than %d streams playing, ran out of handles.\n",FMODNumStreams);
    return;
  }

  FMODStreams[index].stream = stream;
  FMODStreams[index].channel = channel;
  FMODStreams[index].firstFrame = getFrameTiming();
  FMODStreams[index].frequency = Real_FSOUND_GetFrequency(channel);
  printLog("sound/fmod: stream playing on channel %08x with frequency %d Hz.\n",channel,FMODStreams[index].frequency);
}

static FMODStreamDesc *FMODStreamFromChannel(int chan)
{
  for(int i=0;i<FMODNumStreams;i++)
    if(FMODStreams[i].stream && FMODStreams[i].channel == chan)
      return &FMODStreams[i];

  return 0;
}

static FMODStreamDesc *FMODStreamFromPtr(void *ptr)
{
  for(int i=0;i<FMODNumStreams;i++)
    if(FMODStreams[i].stream == ptr)
      return &FMODStreams[i];

  return 0;
}

static int __stdcall Mine_FSOUND_Stream_Play(int channel,void *stream)
{
  printLog("sound/fmod: StreamPlay\n");
  int realChan = Real_FSOUND_Stream_Play(channel,stream);
  RegisterFMODStream(stream,realChan);

  return realChan;
}

static int __stdcall Mine_FSOUND_Stream_PlayEx(int channel,void *stream,void *dspunit,signed char paused)
{
  printLog("sound/fmod: StreamPlayEx(%08x,%p,%p,%d)\n",channel,stream,dspunit,paused);
  int realChan = Real_FSOUND_Stream_PlayEx(channel,stream,dspunit,paused);
  RegisterFMODStream(stream,realChan);

  return realChan;
}

static int __stdcall Mine_FSOUND_PlaySound(int channel,void *sample)
{
  printLog("sound/fmod: PlaySound(%08x,%p)\n",channel,sample);
  int realChan = Real_FSOUND_PlaySound(channel,sample);
  RegisterFMODStream(sample,realChan);

  return realChan;
}

static signed char __stdcall Mine_FSOUND_Stream_Stop(void *stream)
{
  printLog("sound/fmod: StreamStop(%p)\n",stream);
  FMODStreamDesc *desc = FMODStreamFromPtr(stream);
  if(desc)
    desc->stream = 0;

  return Real_FSOUND_Stream_Stop(stream);
}

static int __stdcall Mine_FSOUND_Stream_GetTime(void *stream)
{
  FMODStreamDesc *desc = FMODStreamFromPtr(stream);
  if(desc)
  {
    unsigned int time = UMulDiv(getFrameTiming() - desc->firstFrame,1000*frameRateDenom,frameRateScaled);
    return time;
  }

  return Real_FSOUND_Stream_GetTime(stream);
}

static unsigned int __stdcall Mine_FSOUND_GetCurrentPosition(int channel)
{
  FMODStreamDesc *desc = FMODStreamFromChannel(channel);
  if(!CalledFromFMOD() && desc)
  {
    // this is a known stream playing; return time based on frame timing.
    // (to work around FMODs stupidly coarse timer resolution)
    unsigned int time = UMulDiv(getFrameTiming() - desc->firstFrame,desc->frequency*frameRateDenom,frameRateScaled);
    return time;
  }

  return Real_FSOUND_GetCurrentPosition(channel);
}

void initSoundsysFMOD3()
{
  HMODULE fmodDll = LoadLibraryA("fmod.dll");
  if(fmodDll)
  {
    if(GetProcAddress(fmodDll,"_FSOUND_Stream_Play@8"))
    {
      MODULEINFO moduleInfo;
      GetModuleInformation(GetCurrentProcess(),fmodDll,&moduleInfo,sizeof(moduleInfo));

      FMODStart = moduleInfo.lpBaseOfDll;
      FMODEnd = (void*) ((BYTE*) moduleInfo.lpBaseOfDll + moduleInfo.SizeOfImage);

      printLog("sound/FMOD3: fmod.dll found, FMOD support enabled.\n");
      Real_FSOUND_Stream_Play = (PFSOUND_STREAM_PLAY) DetourFunction((PBYTE) GetProcAddress(fmodDll,"_FSOUND_Stream_Play@8"),(PBYTE) Mine_FSOUND_Stream_Play);
      Real_FSOUND_Stream_PlayEx = (PFSOUND_STREAM_PLAYEX) DetourFunction((PBYTE) GetProcAddress(fmodDll,"_FSOUND_Stream_PlayEx@16"),(PBYTE) Mine_FSOUND_Stream_PlayEx);
      Real_FSOUND_Stream_Stop = (PFSOUND_STREAM_STOP) DetourFunction((PBYTE) GetProcAddress(fmodDll,"_FSOUND_Stream_Stop@4"),(PBYTE) Mine_FSOUND_Stream_Stop);
      Real_FSOUND_Stream_GetTime = (PFSOUND_STREAM_GETTIME) DetourFunction((PBYTE) GetProcAddress(fmodDll,"_FSOUND_Stream_GetTime@4"),(PBYTE) Mine_FSOUND_Stream_GetTime);
      Real_FSOUND_PlaySound = (PFSOUND_PLAYSOUND) DetourFunction((PBYTE) GetProcAddress(fmodDll,"_FSOUND_PlaySound@8"),(PBYTE) Mine_FSOUND_PlaySound);
      Real_FSOUND_GetFrequency = (PFSOUND_GETFREQUENCY) GetProcAddress(fmodDll,"_FSOUND_GetFrequency@4");
      Real_FSOUND_GetCurrentPosition = (PFSOUND_GETCURRENTPOSITION) DetourFunction((PBYTE) GetProcAddress(fmodDll,"_FSOUND_GetCurrentPosition@4"),(PBYTE) Mine_FSOUND_GetCurrentPosition);
    }
  }
}

// ---- FMODEx 4.xx

typedef int (__stdcall *PSYSTEM_INIT)(void *sys,int maxchan,int flags,void *extradriverdata);
typedef int (__stdcall *PSYSTEM_SETOUTPUT)(void *sys,int output);
typedef int (__stdcall *PSYSTEM_PLAYSOUND)(void *sys,int index,void *sound,bool paused,void **channel);
typedef int (__stdcall *PCHANNEL_GETFREQUENCY)(void *chan,float *freq);
typedef int (__stdcall *PCHANNEL_GETPOSITION)(void *chan,unsigned *position,int posType);

static PSYSTEM_INIT Real_System_init;
static PSYSTEM_SETOUTPUT Real_System_setOutput;
static PSYSTEM_PLAYSOUND Real_System_playSound;
static PCHANNEL_GETFREQUENCY Real_Channel_getFrequency;
static PCHANNEL_GETPOSITION Real_Channel_getPosition;

static void *FMODExStart = 0, *FMODExEnd = 0;

#define CalledFromFMODEx() (_ReturnAddress() >= FMODExStart && _ReturnAddress() < FMODExEnd) // needs to be a macro

struct FMODExSoundDesc
{
  void *sound;
  void *channel;
  int firstFrame;
  float frequency;
};

static const int FMODExNumSounds = 16; // max # of active (playing) sounds supported
static FMODExSoundDesc FMODExSounds[FMODExNumSounds];

static int __stdcall Mine_System_init(void *sys,int maxchan,int flags,void *extradriverdata)
{
  // force output type to be dsound (mainly so i don't have to write a windows audio session implementation for vista)
  Real_System_setOutput(sys,6); // 6=DirectSound
  return Real_System_init(sys,maxchan,flags,extradriverdata);
}

static int __stdcall Mine_System_playSound(void *sys,int index,void *sound,bool paused,void **channel)
{
  void *chan;
  printLog("sound/fmodex: playSound\n");
  int result = Real_System_playSound(sys,index,sound,paused,&chan);
  if(result == 0) // FMOD_OK
  {
    // find a free sound desc
    int index = 0;
    while(index<FMODExNumSounds && FMODExSounds[index].sound)
      index++;

    if(index==FMODExNumSounds)
      printLog("sound/fmodex: more than %d sounds playing, ran out of handles.\n",FMODExNumSounds);
    else
    {
      FMODExSounds[index].sound = sound;
      FMODExSounds[index].channel = chan;
      FMODExSounds[index].firstFrame = getFrameTiming();
      Real_Channel_getFrequency(chan,&FMODExSounds[index].frequency);
    }
  }

  if(channel)
    *channel = chan;

  return result;
}

static FMODExSoundDesc *FMODExSoundFromChannel(void *chan)
{
  for(int i=0;i<FMODExNumSounds;i++)
    if(FMODExSounds[i].sound && FMODExSounds[i].channel == chan)
      return &FMODExSounds[i];

  return 0;
}

static int __stdcall Mine_Channel_getPosition(void *channel,unsigned *position,int posType)
{
  FMODExSoundDesc *desc = FMODExSoundFromChannel(channel);
  if(desc && !CalledFromFMODEx())
  {
    int timeBase;

    switch(posType)
    {
    case 1:   timeBase = 1000; break;                   // FMOD_TIMEUNIT_MS
    case 2:   timeBase = (int) desc->frequency; break;  // FMOD_TIMEUNIT_PCM
    default:  timeBase = -1; break;
    }

    if(timeBase != -1)
    {
      *position = UMulDiv(getFrameTiming() - desc->firstFrame,timeBase*frameRateDenom,frameRateScaled);
      return 0; // FMOD_OK
    }
    else
      printLog("sound/fmodex: unsupported timebase used in Channel::getPosition, forwarding to fmod timer.\n");
  }

  return Real_Channel_getPosition(channel,position,posType);
}

void initSoundsysFMODEx()
{
  HMODULE fmodDll = LoadLibraryA("fmodex.dll");
  if(fmodDll)
  {
    if(GetProcAddress(fmodDll,"FMOD_System_Init"))
    {
      MODULEINFO moduleInfo;
      GetModuleInformation(GetCurrentProcess(),fmodDll,&moduleInfo,sizeof(moduleInfo));

      FMODExStart = moduleInfo.lpBaseOfDll;
      FMODExEnd = (void*) ((BYTE*) moduleInfo.lpBaseOfDll + moduleInfo.SizeOfImage);

      printLog("sound/fmodex: fmodex.dll found, FMODEx support enabled.\n");

      Real_System_init = (PSYSTEM_INIT) DetourFunction((PBYTE) GetProcAddress(fmodDll,"?init@System@FMOD@@QAG?AW4FMOD_RESULT@@HIPAX@Z"),(PBYTE) Mine_System_init);
      Real_System_setOutput = (PSYSTEM_SETOUTPUT) GetProcAddress(fmodDll,"?setOutput@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_OUTPUTTYPE@@@Z");
      Real_System_playSound = (PSYSTEM_PLAYSOUND) DetourFunction((PBYTE) GetProcAddress(fmodDll,"?playSound@System@FMOD@@QAG?AW4FMOD_RESULT@@W4FMOD_CHANNELINDEX@@PAVSound@2@_NPAPAVChannel@2@@Z"),(PBYTE) Mine_System_playSound);
      Real_Channel_getFrequency = (PCHANNEL_GETFREQUENCY) GetProcAddress(fmodDll,"?getFrequency@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAM@Z");
      Real_Channel_getPosition = (PCHANNEL_GETPOSITION) DetourFunction((PBYTE) GetProcAddress(fmodDll,"?getPosition@Channel@FMOD@@QAG?AW4FMOD_RESULT@@PAII@Z"),(PBYTE) Mine_Channel_getPosition);
    }
  }
}

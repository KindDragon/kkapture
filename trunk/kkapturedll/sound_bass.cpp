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
#include "sound_bass.h"

// ---- BASS

struct BASS_INFO
{
  DWORD flags;      // device capabilities (DSCAPS_xxx flags)
  DWORD hwsize;     // size of total device hardware memory
  DWORD hwfree;     // size of free device hardware memory
  DWORD freesam;    // number of free sample slots in the hardware
  DWORD free3d;     // number of free 3D sample slots in the hardware
  DWORD minrate;    // min sample rate supported by the hardware
  DWORD maxrate;    // max sample rate supported by the hardware
  BOOL eax;         // device supports EAX? (always FALSE if BASS_DEVICE_3D was not used)
  DWORD minbuf;     // recommended minimum buffer length in ms (requires BASS_DEVICE_LATENCY)
  DWORD dsver;      // DirectSound version
  DWORD latency;    // delay (in ms) before start of playback (requires BASS_DEVICE_LATENCY)
  DWORD initflags;  // BASS_Init "flags" parameter
  DWORD speakers;   // number of speakers available
  DWORD freq;       // current output rate (Vista/OSX only)
};

typedef BOOL (__stdcall *PBASS_INIT)(int device,DWORD freq,DWORD flags,HWND win,GUID *clsid);
typedef BOOL (__stdcall *PBASS_GETINFO)(BASS_INFO *info);

static PBASS_INIT Real_BASS_Init = 0;
static PBASS_GETINFO Real_BASS_GetInfo = 0;

static BOOL __stdcall Mine_BASS_Init(int device,DWORD freq,DWORD flags,HWND win,GUID *clsid)
{
  // for BASS, all we need to do is make sure that the BASS_DEVICE_LATENCY flag is cleared.
  return Real_BASS_Init(device,freq,flags & ~256,win,clsid);
}

static BOOL __stdcall Mine_BASS_GetInfo(BASS_INFO *info)
{
  BOOL res = Real_BASS_GetInfo(info);
  if(info) info->latency = 0;
  return res;
}

void initSoundsysBASS()
{
  HMODULE bassDll = LoadLibraryA("bass.dll");
  if(bassDll)
  {
    PBASS_INIT init = (PBASS_INIT) GetProcAddress(bassDll,"BASS_Init");
    PBASS_GETINFO getinfo = (PBASS_GETINFO) GetProcAddress(bassDll,"BASS_GetInfo");

    if(init && getinfo)
    {
      printLog("sound/bass: bass.dll found, BASS support enabled.\n");
      Real_BASS_Init = (PBASS_INIT) DetourFunction((PBYTE) init,(PBYTE) Mine_BASS_Init);
      Real_BASS_GetInfo = (PBASS_GETINFO) DetourFunction((PBYTE) getinfo,(PBYTE) Mine_BASS_GetInfo);
    }
  }
}

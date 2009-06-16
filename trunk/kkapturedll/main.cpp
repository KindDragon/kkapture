// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "tchar.h"
#include "bmp_videoencoder.h"
#include "avi_videoencoder.h"
#include <stdio.h>

#include "video.h"

VideoEncoder *encoder = 0;
float frameRate = 10;
int frameRateScaled = 1000;
bool initialized = false;

void done()
{
  if(initialized)
  {
    printLog("main: shutting down...\n");

    doneTiming();
    doneSound();
    doneVideo();
    
    delete encoder;
    encoder = 0;

    closeLog();
    initialized = false;
  }
}

DETOUR_TRAMPOLINE(void __stdcall Real_ExitProcess(UINT uExitCode), ExitProcess);

void __stdcall Mine_ExitProcess(UINT uExitCode)
{
  done();
  Real_ExitProcess(uExitCode);
}

void init()
{
  bool error = true;

  initLog();
  printLog("main: initializing...\n");
  initVideo();
  initSound();
  initTiming();

  // get file mapping containing capturing info
  HANDLE hMapping = OpenFileMapping(FILE_MAP_READ,FALSE,_T("__kkapture_parameter_block"));
  if(hMapping != INVALID_HANDLE_VALUE)
  {
    ParameterBlock *block = (ParameterBlock *) MapViewOfFile(hMapping,FILE_MAP_READ,0,0,sizeof(ParameterBlock));
    if(block)
    {
      // correct version
      if(block->VersionTag == PARAMVERSION)
      {
        frameRateScaled = block->FrameRate;
        if(block->Encoder == AVIEncoder)
          encoder = new AVIVideoEncoder(block->FileName,frameRateScaled/100.0f,block->VideoCodec,block->VideoQuality);
        else if(block->Encoder == BMPEncoder)
          encoder = new BMPVideoEncoder(block->FileName);
        else
          encoder = new DummyVideoEncoder;

        error = false;
      }

      UnmapViewOfFile(block);
    }

    CloseHandle(hMapping);
  }

  if(error)
  {
    printLog("main: couldn't access parameter block or wrong version");

    frameRateScaled = 1000;
    encoder = new DummyVideoEncoder;
  }

  // install our hook so we get notified of process exit (hopefully)
  DetourFunctionWithTrampoline((PBYTE) Real_ExitProcess, (PBYTE) Mine_ExitProcess);

  frameRate = frameRateScaled / 100.0;
  initialized = true;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, PVOID lpReserved)
{
  if(dwReason == DLL_PROCESS_ATTACH)
    init();
	/*switch (dwReason)
  {
  case DLL_PROCESS_ATTACH:
    init();
    return TRUE;

  case DLL_PROCESS_DETACH:
    done();
    return TRUE;
	}*/
	return TRUE;
}

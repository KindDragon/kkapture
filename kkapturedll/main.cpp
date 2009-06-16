// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "tchar.h"
#include "bmp_videoencoder.h"
#include "avi_videoencoder.h"
#include <stdio.h>

VideoEncoder *encoder = 0;
float frameRate;

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
        frameRate = block->FrameRate;
        encoder = new AVIVideoEncoder(block->AviName,frameRate);
        error = false;
      }

      UnmapViewOfFile(block);
    }

    CloseHandle(hMapping);
  }

  if(error)
  {
    printLog("main: couldn't access parameter block or wrong version");

    frameRate = 10.0f;
    encoder = new DummyVideoEncoder;
  }
}

void done()
{
  printLog("main: shutting down...\n");

  doneTiming();
  doneSound();
  doneVideo();
  
  delete encoder;
  encoder = 0;

  closeLog();
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, PVOID lpReserved)
{
	switch (dwReason)
  {
  case DLL_PROCESS_ATTACH:
    init();
    return TRUE;

  case DLL_PROCESS_DETACH:
    done();
    return TRUE;
	}
	return TRUE;
}

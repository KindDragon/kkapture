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
#include "tchar.h"
#include <stdio.h>
#include <process.h>

#include "video.h"
#include "videoencoder.h"

VideoEncoder *encoder = 0;
float frameRate = 10;
int frameRateScaled = 1000;
ParameterBlock params;

static CRITICAL_SECTION shuttingDown;
static bool initialized = false;
static HMODULE hModule = 0;
static HHOOK hKeyHook = 0;
static HANDLE hHookThread = 0;
static DWORD HookThreadId = 0;

// ---- forwards

static void done();
static void init();

// ---- API hooks

DETOUR_TRAMPOLINE(void __stdcall Real_ExitProcess(UINT uExitCode), ExitProcess);

void __stdcall Mine_ExitProcess(UINT uExitCode)
{
  done();
  Real_ExitProcess(uExitCode);
}

LRESULT CALLBACK LLKeyboardHook(int code,WPARAM wParam,LPARAM lParam)
{
  bool wannaExit = false;
  KBDLLHOOKSTRUCT *hook = (KBDLLHOOKSTRUCT *) lParam;

  if(code == HC_ACTION && hook->vkCode == VK_CANCEL) // ctrl+break
    wannaExit = true;

  LRESULT result = CallNextHookEx(hKeyHook,code,wParam,lParam);
  if(wannaExit)
  {
    printLog("main: ctrl+break pressed, stopping recording...\n");
    Mine_ExitProcess(0);
  }

  return result;
}

static void __cdecl HookThreadProc(void *arg)
{
  HookThreadId = GetCurrentThreadId();

  // install the hook
  hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL,LLKeyboardHook,hModule,0);
  if(!hKeyHook)
    printLog("main: couldn't install keyboard hook\n");

  // message loop
  MSG msg;
  while(GetMessage(&msg,0,0,0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  printLog("main: hook thread exiting\n");

  // remove the hook
  if(hKeyHook)
  {
    UnhookWindowsHookEx(hKeyHook);
    hKeyHook = 0;
  }
}

// ---- public interface

static void done()
{
  if(!initialized)
    return;

  EnterCriticalSection(&shuttingDown);

  if(initialized)
  {
    printLog("main: shutting down...\n");

    // shutdown hook thread
    PostThreadMessage(HookThreadId,WM_QUIT,0,0);

    VideoEncoder *realEncoder = encoder;
    encoder = 0;

    doneTiming();
    doneSound();
    doneVideo();
    
    delete realEncoder;
    printLog("main: everything ok, closing log.\n");

    closeLog();
    initialized = false;
  }

  LeaveCriticalSection(&shuttingDown);
  DeleteCriticalSection(&shuttingDown);
}

static void init()
{
  bool error = true;

  InitializeCriticalSection(&shuttingDown);

  initLog();
  printLog("main: initializing...\n");
  initTiming();
  initVideo();
  initSound();
  printLog("main: all main components initialized.\n");

  // initialize params with all zero (ahem)
  memset(&params,0,sizeof(params));

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
        memcpy(&params,block,sizeof(params));

        printLog("main: reading parameter block...\n");

        frameRateScaled = block->FrameRate;
        encoder = createVideoEncoder(block->FileName);
        printLog("main: video encoder initialized.\n");

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
  hHookThread = (HANDLE) _beginthread(HookThreadProc,0,0);

  frameRate = frameRateScaled / 100.0f;
  initialized = true;

  printLog("main: initialization done\n");
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, PVOID lpReserved)
{
  if(dwReason == DLL_PROCESS_ATTACH)
  {
    ::hModule = hModule;
    init();
  }

  return TRUE;
}

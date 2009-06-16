// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include <malloc.h>

#include "longarith.h"

#pragma comment(lib, "winmm.lib")

// events
static HANDLE nextFrameEvent = 0;
static HANDLE resyncEvent = 0;
static LONGLONG perfFrequency = 0;
static volatile LONG resyncCounter = 0;
static volatile LONG waitCounter = 0;

// trampolines
DETOUR_TRAMPOLINE(BOOL __stdcall Real_QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency), QueryPerformanceFrequency);
DETOUR_TRAMPOLINE(BOOL __stdcall Real_QueryPerformanceCounter(LARGE_INTEGER *lpPerformaceCount), QueryPerformanceCounter);
DETOUR_TRAMPOLINE(DWORD __stdcall Real_GetTickCount(), GetTickCount);
DETOUR_TRAMPOLINE(DWORD __stdcall Real_timeGetTime(), timeGetTime);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_timeGetSystemTime(MMTIME *pmmt,UINT cbmmt), timeGetSystemTime);
DETOUR_TRAMPOLINE(VOID __stdcall Real_Sleep(DWORD dwMilliseconds), Sleep);
DETOUR_TRAMPOLINE(DWORD __stdcall Real_WaitForSingleObject(HANDLE hHandle,DWORD dwMilliseconds), WaitForSingleObject);
DETOUR_TRAMPOLINE(DWORD __stdcall Real_WaitForMultipleObjects(DWORD nCount,CONST HANDLE *lpHandles,BOOL bWaitAll,DWORD dwMilliseconds), WaitForMultipleObjects);
DETOUR_TRAMPOLINE(void __stdcall Real_GetSystemTimeAsFileTime(FILETIME *time), GetSystemTimeAsFileTime);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_timeSetEvent(UINT uDelay,UINT uResolution,LPTIMECALLBACK fptc,DWORD_PTR dwUser,UINT fuEvent), timeSetEvent);
DETOUR_TRAMPOLINE(MMRESULT __stdcall Real_timeKillEvent(UINT uTimerID), timeKillEvent);

BOOL __stdcall Mine_QueryPerformanceFrequency(LARGE_INTEGER *lpFrequency)
{
  lpFrequency->QuadPart = perfFrequency;
  return TRUE;
}

BOOL __stdcall Mine_QueryPerformanceCounter(LARGE_INTEGER *lpCounter)
{
  int frame = getFrameTiming();
  static LARGE_INTEGER firstTime = { 0 };

  if(!frame)
    Real_QueryPerformanceCounter(&firstTime);

  lpCounter->QuadPart = firstTime.QuadPart + ULongMulDiv(perfFrequency,frame*100,frameRateScaled);
  return TRUE;
}

DWORD __stdcall Mine_GetTickCount()
{
  // before the first frame is finished, time still progresses normally
  int frame = getFrameTiming();
  static int firstTime = 0;

  if(!frame)
    firstTime = Real_GetTickCount();

  return firstTime + UMulDiv(frame,1000*100,frameRateScaled);
}

DWORD __stdcall Mine_timeGetTime()
{
  int frame = getFrameTiming();
  static int firstTime = 0;

  if(!frame)
    firstTime = Real_timeGetTime();

  return firstTime + UMulDiv(frame,1000*100,frameRateScaled);
}

MMRESULT __stdcall Mine_timeGetSystemTime(MMTIME *pmmt,UINT cbmmt)
{
  printLog("timing: timeGetSystemTime\n");
  return Real_timeGetSystemTime(pmmt,cbmmt);
}

void __stdcall Mine_GetSystemTimeAsFileTime(FILETIME *time)
{
  int frame = getFrameTiming();
  static FILETIME firstTime = { 0 };

  if(!frame)
    Real_GetSystemTimeAsFileTime(&firstTime);

  LONGLONG baseTime = *((LONGLONG *) &firstTime);
  LONGLONG elapsedSince = ULongMulDiv(10000000,frame,frameRateScaled);
  LONGLONG finalTime = baseTime + elapsedSince;

  *((LONGLONG *) time) = finalTime;
}

// --- event timers

static const int MaxEventTimers = 256;

struct EventTimerDef
{
  UINT Delay;
  LPTIMECALLBACK Callback;
  DWORD_PTR User;
  UINT Flags;
  int Counter;
  bool Dead;
  bool Used;
};

static EventTimerDef EventTimer[MaxEventTimers];
static CRITICAL_SECTION TimerAllocLock;

static MMRESULT __stdcall Mine_timeSetEvent(UINT uDelay,UINT uResolution,LPTIMECALLBACK fptc,DWORD_PTR dwUser,UINT fuEvent)
{
  if(!uDelay) // don't even try to create a timer without delay
    return 0;

  EnterCriticalSection(&TimerAllocLock);

  // try to find a free slot
  for(int i=0;i<MaxEventTimers;i++)
    if(!EventTimer[i].Used)
      break;

  if(i == MaxEventTimers) // no free slot found
  {
    LeaveCriticalSection(&TimerAllocLock);
    return 0;
  }

  // fill out timer struct
  EventTimerDef *timer = &EventTimer[i];
  timer->Delay = uDelay;
  timer->Counter = uDelay;
  timer->Callback = fptc;
  timer->User = dwUser;
  timer->Flags = fuEvent;
  timer->Dead = false;
  timer->Used = true;

  LeaveCriticalSection(&TimerAllocLock);
  return i + 1;
}

static MMRESULT __stdcall Mine_timeKillEvent(UINT uTimerID)
{
  if(!uTimerID || uTimerID > MaxEventTimers)
    return MMSYSERR_INVALPARAM;

  EventTimerDef *timer = &EventTimer[uTimerID-1];
  if(!timer->Used)
    return MMSYSERR_INVALPARAM;

  timer->Used = false;
  return TIMERR_NOERROR;
}

static void FireTimer(UINT index)
{
  EventTimerDef *timer = &EventTimer[index];

  if(timer->Flags & TIME_CALLBACK_EVENT_SET)
    SetEvent((HANDLE) timer->Callback);
  else if(timer->Flags & TIME_CALLBACK_EVENT_PULSE)
    PulseEvent((HANDLE) timer->Callback);
  else // function
    timer->Callback(index+1,0,timer->User,0,0);
}

static void ProcessTimers(int TimeElapsed)
{
  EnterCriticalSection(&TimerAllocLock);

  for(int i=0;i<MaxEventTimers;i++)
  {
    EventTimerDef *timer = &EventTimer[i];
    
    if(timer->Used && !timer->Dead)
    {
      timer->Counter -= TimeElapsed;
      while(timer->Counter <= 0)
      {
        FireTimer(i);
        if(timer->Flags & TIME_PERIODIC)
          timer->Counter += timer->Delay;
        else
        {
          timer->Dead = true;
          break;
        }
      }
    }
  }

  LeaveCriticalSection(&TimerAllocLock);
}

// --- everything that sleeps may accidentially take more than one frame.
// this causes soundsystems to bug, so we have to fix it here.

VOID __stdcall Mine_Sleep(DWORD dwMilliseconds)
{
  if(dwMilliseconds)
  {
    Real_WaitForSingleObject(resyncEvent,INFINITE);

    InterlockedIncrement(&waitCounter);
    if(params.MakeSleepsLastOneFrame)
      Real_WaitForSingleObject(nextFrameEvent,dwMilliseconds);
    else
      Real_WaitForSingleObject(nextFrameEvent,params.SleepTimeout);
    InterlockedDecrement(&waitCounter);
  }
  else
    Real_Sleep(0);
}

DWORD __stdcall Mine_WaitForSingleObject(HANDLE hHandle,DWORD dwMilliseconds)
{
  if(dwMilliseconds != INFINITE)
  {
    Real_WaitForSingleObject(resyncEvent,INFINITE);
    InterlockedIncrement(&waitCounter);

    HANDLE handles[] = { hHandle, nextFrameEvent };
    DWORD result = Real_WaitForMultipleObjects(2,handles,FALSE,dwMilliseconds);

    InterlockedDecrement(&waitCounter);

    if(result == WAIT_OBJECT_0+1)
      result = WAIT_TIMEOUT;

    return result;
  }
  else
    return Real_WaitForSingleObject(hHandle,dwMilliseconds);
}

DWORD __stdcall Mine_WaitForMultipleObjects(DWORD nCount,CONST HANDLE *lpHandles,BOOL bWaitAll,DWORD dwMilliseconds)
{
  // infinite waits are always passed through
  if(dwMilliseconds == INFINITE)
    return Real_WaitForMultipleObjects(nCount,lpHandles,bWaitAll,dwMilliseconds);
  else
  {
    // waitalls are harder to fake, so we just clamp them to timeout 0.
    if(bWaitAll)
      return Real_WaitForMultipleObjects(nCount,lpHandles,TRUE,0);
    else
    {
      // we can't use new/delete, this might be called from a context
      // where they don't work (such as after clib deinit)
      HANDLE *handles = (HANDLE *) _alloca((nCount + 1) * sizeof(HANDLE));
      memcpy(handles,lpHandles,nCount*sizeof(HANDLE));
      handles[nCount] = nextFrameEvent;

      Real_WaitForSingleObject(resyncEvent,INFINITE);
      InterlockedIncrement(&waitCounter);

      DWORD result = Real_WaitForMultipleObjects(nCount+1,handles,FALSE,dwMilliseconds);
      if(result == WAIT_OBJECT_0+nCount)
        result = WAIT_TIMEOUT;

      InterlockedDecrement(&waitCounter);

      return result;
    }
  }
}

static int currentFrame = 0;

void initTiming()
{
  timeBeginPeriod(1);

  nextFrameEvent = CreateEvent(0,TRUE,FALSE,0);
  resyncEvent = CreateEvent(0,TRUE,TRUE,0);

  memset(EventTimer,0,sizeof(EventTimer));
  InitializeCriticalSection(&TimerAllocLock);

  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  perfFrequency = freq.QuadPart;

  DetourFunctionWithTrampoline((PBYTE) Real_QueryPerformanceFrequency, (PBYTE) Mine_QueryPerformanceFrequency);
  DetourFunctionWithTrampoline((PBYTE) Real_QueryPerformanceCounter, (PBYTE) Mine_QueryPerformanceCounter);
  DetourFunctionWithTrampoline((PBYTE) Real_GetTickCount, (PBYTE) Mine_GetTickCount);
  DetourFunctionWithTrampoline((PBYTE) Real_timeGetTime, (PBYTE) Mine_timeGetTime);
  DetourFunctionWithTrampoline((PBYTE) Real_Sleep, (PBYTE) Mine_Sleep);
  DetourFunctionWithTrampoline((PBYTE) Real_WaitForSingleObject, (PBYTE) Mine_WaitForSingleObject);
  DetourFunctionWithTrampoline((PBYTE) Real_WaitForMultipleObjects, (PBYTE) Mine_WaitForMultipleObjects);
  DetourFunctionWithTrampoline((PBYTE) Real_GetSystemTimeAsFileTime, (PBYTE) Mine_GetSystemTimeAsFileTime);
  DetourFunctionWithTrampoline((PBYTE) Real_timeSetEvent, (PBYTE) Mine_timeSetEvent);
  DetourFunctionWithTrampoline((PBYTE) Real_timeKillEvent, (PBYTE) Mine_timeKillEvent);
}

void doneTiming()
{
  DeleteCriticalSection(&TimerAllocLock);

  CloseHandle(nextFrameEvent);
  CloseHandle(resyncEvent);

  timeEndPeriod(1);
}

void resetTiming()
{
  currentFrame = 0;
}

void nextFrameTiming()
{
  /*SetEvent(nextFrameEvent);
  Real_Sleep(5);*/

  ResetEvent(resyncEvent);
  SetEvent(nextFrameEvent);
  while(waitCounter)
    Real_Sleep(5);
  ResetEvent(nextFrameEvent);
  SetEvent(resyncEvent);
  
  Real_Sleep(5);

  DWORD oldFrameTime = UMulDiv(currentFrame,1000*100,frameRateScaled);
  DWORD newFrameTime = UMulDiv(currentFrame+1,1000*100,frameRateScaled);
  ProcessTimers(newFrameTime - oldFrameTime);

  currentFrame++;
}

int getFrameTiming()
{
  return currentFrame;
}
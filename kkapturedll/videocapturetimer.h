// performance/debug

extern DWORD __stdcall Real_timeGetTime();

class VideoCaptureTimer
{
  DWORD Start;
  const char *Desc;

public:
  VideoCaptureTimer(const char *desc)
    : Desc(desc)
  {
    Start = Real_timeGetTime();
  }

  ~VideoCaptureTimer()
  {
    printLog("video: %s took %d ms\n",Desc,Real_timeGetTime() - Start);
  }
};



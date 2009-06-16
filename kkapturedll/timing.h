// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

// setup
void initTiming();
void doneTiming();

// update timing stuff
void resetTiming();
void nextFrameTiming();

int getFrameTiming();

extern VOID __stdcall Real_Sleep(DWORD dwMilliseconds);
extern DWORD __stdcall Real_WaitForSingleObject(HANDLE hHandle,DWORD dwMilliseconds);
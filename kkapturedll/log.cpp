// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"

FILE *logFile = 0;

void initLog()
{
  logFile = fopen("kkapturelog.txt","w");
}

void closeLog()
{
  if(logFile)
  {
    fclose(logFile);
    logFile = 0;
  }
}

void printLog(const char *format,...)
{
  if(logFile)
  {
    va_list arg;
    va_start(arg,format);
    vfprintf(logFile,format,arg);
    va_end(arg);

    fflush(logFile);
  }
}
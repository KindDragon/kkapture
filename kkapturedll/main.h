// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __MAIN_H__
#define __MAIN_H__

class VideoEncoder;

// configuration options

// writing AVIs using DShow (please read top of avi_videoencoder_dshow.cpp)
#define USE_DSHOW_AVI_WRITER      1

// global variables
extern VideoEncoder *encoder;
extern float frameRate;
extern int frameRateScaled;

// parameter block submitted by main app
static const int PARAMVERSION = 2;

enum EncoderType
{
  DummyEncoder,
  BMPEncoder,
  AVIEncoder
};

struct ParameterBlock
{
  unsigned VersionTag;
  TCHAR FileName[_MAX_PATH];
  int FrameRate;
  EncoderType Encoder;
  DWORD VideoCodec;
  DWORD VideoQuality;

  BOOL CaptureVideo;
  BOOL CaptureAudio;

  DWORD SoundMaxSkip; // in milliseconds
  BOOL MakeSleepsLastOneFrame;
  DWORD SleepTimeout;
};

extern ParameterBlock params;

#endif
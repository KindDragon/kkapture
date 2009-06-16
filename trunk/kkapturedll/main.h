// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __MAIN_H__
#define __MAIN_H__

class VideoEncoder;

// global variables
extern VideoEncoder *encoder;
extern float frameRate;

// parameter block submitted by main app
static const int PARAMVERSION = 1;

struct ParameterBlock
{
  unsigned VersionTag;
  TCHAR AviName[_MAX_PATH];
  float FrameRate;
};

#endif
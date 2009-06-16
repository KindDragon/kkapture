// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __MAIN_H__
#define __MAIN_H__

class VideoEncoder;

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
};

#endif
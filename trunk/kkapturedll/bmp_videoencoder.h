// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#pragma once

#ifndef __BMP_VIDEOENCODER_H__
#define __BMP_VIDEOENCODER_H__

#include "videoencoder.h"

class BMPVideoEncoder : public VideoEncoder
{
  struct Internal;
  
  char prefix[384];
  int xRes,yRes;
  int frame;
  Internal *intn;

public:
  BMPVideoEncoder(const char *fileName);
  virtual ~BMPVideoEncoder();

  virtual void SetSize(int xRes,int yRes);
  virtual void WriteFrame(const unsigned char *buffer);

  virtual void SetAudioFormat(const tWAVEFORMATEX *fmt);
  virtual void WriteAudioFrame(const void *buffer,int samples);
};

#endif
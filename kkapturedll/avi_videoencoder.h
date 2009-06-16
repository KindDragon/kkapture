// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#pragma once

#ifndef __AVI_VIDEOENCODER_H__
#define __AVI_VIDEOENCODER_H__

#include "videoencoder.h"

class AVIVideoEncoder : public VideoEncoder
{
  struct Internal;
  
  int xRes,yRes;
  int frame;
  int audioSample,audioBytesSample;
  float fps;
  Internal *d;

  void Cleanup();
  void StartEncode();
  void StartAudioEncode(tWAVEFORMATEX *fmt);

public:
  AVIVideoEncoder(const char *name,float _fps);
  virtual ~AVIVideoEncoder();

  virtual void SetSize(int xRes,int yRes);
  virtual void WriteFrame(const unsigned char *buffer);

  virtual void SetAudioFormat(tWAVEFORMATEX *fmt);
  virtual void WriteAudioFrame(const void *buffer,int samples);
};

#endif
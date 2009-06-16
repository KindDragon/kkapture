// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#pragma once

#ifndef __VIDEOENCODER_H__
#define __VIDEOENCODER_H__

class VideoEncoder
{
public:
  virtual ~VideoEncoder();

  virtual void SetSize(int xRes,int yRes) = 0;
  virtual void WriteFrame(const unsigned char *buffer) = 0;

  virtual void SetAudioFormat(struct tWAVEFORMATEX *fmt) = 0;
  virtual void WriteAudioFrame(const void *buffer,int samples) = 0;
};

class DummyVideoEncoder : public VideoEncoder
{
public:
  DummyVideoEncoder();
  virtual ~DummyVideoEncoder();

  virtual void SetSize(int xRes,int yRes);
  virtual void WriteFrame(const unsigned char *buffer);

  virtual void SetAudioFormat(struct tWAVEFORMATEX *fmt);
  virtual void WriteAudioFrame(const void *buffer,int samples);
};

#endif
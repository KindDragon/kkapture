/* kkapture: intrusive demo video capturing.
 * Copyright (c) 2005-2010 Fabian "ryg/farbrausch" Giesen.
 *
 * This program is free software; you can redistribute and/or modify it under
 * the terms of the Artistic License, Version 2.0beta5, or (at your opinion)
 * any later version; all distributions of this program should contain this
 * license in a file named "LICENSE.txt".
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT UNLESS REQUIRED BY
 * LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT HOLDER OR CONTRIBUTOR
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "stdafx.h"
#include "videoencoder.h"
#include "video.h"

#include "bmp_videoencoder.h"
#include "avi_videoencoder_vfw.h"
#include "avi_videoencoder_dshow.h"
#include "mt_proxy_videoencoder.h"

static CRITICAL_SECTION captureDataLock;
static bool gotDataLock = false;
static int partCounter;
static bool seenFrames;

VideoCaptureDataLock::VideoCaptureDataLock()
{
  if(gotDataLock)
    EnterCriticalSection(&captureDataLock);
}

VideoCaptureDataLock::~VideoCaptureDataLock()
{
  if(gotDataLock)
    LeaveCriticalSection(&captureDataLock);
}

// video encoder handling
VideoEncoder *createVideoEncoder(const char *filename)
{
  VideoEncoder *encoder = 0;

  switch(params.Encoder)
  {
  case BMPEncoder:
    encoder = new BMPVideoEncoder(filename);
    break;

  case AVIEncoderVFW:
    encoder = new AVIVideoEncoderVFW(filename,frameRateScaled,frameRateDenom,params.VideoCodec,params.VideoQuality);
    break;

#if USE_DSHOW_AVI_WRITER
  case AVIEncoderDShow:
    encoder = new AVIVideoEncoderDShow(filename,frameRateScaled,frameRateDenom,params.VideoCodec,params.VideoQuality);
    break;
#endif

  default:
    printLog("video: encoder type not supported in this build - not writing anything.\n");
    encoder = new DummyVideoEncoder;
    break;
  }

  // multithread wrapper
  if(params.UseEncoderThread)
    encoder = new MTProxyVideoEncoder(encoder);

  return encoder;
}

void videoStartNextPart(bool autoSize)
{
  videoNeedEncoder();

  // get the current audio format (so we can resume audio seamlessly)
  WAVEFORMATEX *wfx = encoder->GetAudioFormat();

  // delete the encoder
  delete encoder;
  encoder = 0;

  // build filename for next part
  partCounter++;

  char fileName[_MAX_PATH],baseName[_MAX_PATH];
  strcpy_s(baseName,params.FileName);

  for(int i=strlen(baseName)-1;i>=0;i--)
  {
    if(baseName[i] == '/' || baseName[i] == '\\')
      break;
    else if(baseName[i] == '.')
    {
      baseName[i] = 0;
      break;
    }
  }

  _snprintf_s(fileName,sizeof(fileName),_TRUNCATE,"%s_part%02d.avi",baseName,partCounter);

  // re-create encoder
  encoder = createVideoEncoder(fileName);
  seenFrames = false;

  // set size if requested
  if(autoSize)
    encoder->SetSize(captureBuffer.width,captureBuffer.height);

  // set audio format if we had one
  if(wfx)
    encoder->SetAudioFormat(wfx);

  delete[] (unsigned char*) wfx;
}

void videoNeedEncoder()
{
  if(!encoder)
  {
    encoder = createVideoEncoder(params.FileName);
    printLog("main: video encoder initialized.\n");
  }
}

void setCaptureResolution(int inWidth,int inHeight)
{
  bool unaligned;
  int width,height;

  videoNeedEncoder();
  
  if((inWidth & 3) || (inHeight & 3))
  {
    width = inWidth & ~3;
    height = inHeight & ~3;
    unaligned = true;
  }
  else
  {
    width = inWidth;
    height = inHeight;
    unaligned = false;
  }

  if(width != captureBuffer.width || height != captureBuffer.height)
  {
    // if we've already seen frames, start a new part
    if(seenFrames)
      videoStartNextPart(false);

    if(unaligned)
      printLog("video: width/height not divisible by 4, aligning down (%dx%d)\n",inWidth,inHeight);

    printLog("video: capturing at %dx%d\n",width,height);
    captureBuffer.init(width,height,params.DestBpp);

    if(encoder)
      encoder->SetSize(width,height);
  }
}

// advance frame
void nextFrame()
{ 
  videoNeedEncoder();

  seenFrames = true;
  nextFrameTiming();
  nextFrameSound();
}

// skip this frame (same as nextFrame(), but duplicating old frame data)
void skipFrame()
{
  videoNeedEncoder();
  {
    VideoCaptureDataLock lock;

    // write the old frame again
    if(encoder && params.CaptureVideo)
      encoder->WriteFrame(captureBuffer.data);
  }

  nextFrame();
}

// public interface
void initVideo()
{
  InitializeCriticalSection(&captureDataLock);
  gotDataLock = true;

  partCounter = 1;
  seenFrames = false;

  initVideo_OpenGL();
  initVideo_Direct3D8();
  initVideo_Direct3D9();
  initVideo_Direct3D10();
  initVideo_DirectDraw();
  initVideo_GDI();
}

void doneVideo()
{
  captureBuffer.destroy();

  DeleteCriticalSection(&captureDataLock);
  gotDataLock = false;
}
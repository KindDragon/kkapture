/* kkapture: intrusive demo video capturing.
 * Copyright (c) 2005-2006 Fabian "ryg/farbrausch" Giesen.
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

static int partCounter;
static bool seenFrames;

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
    encoder = new AVIVideoEncoderVFW(filename,frameRateScaled/100.0f,params.VideoCodec,params.VideoQuality);
    break;

  case AVIEncoderDShow:
    encoder = new AVIVideoEncoderDShow(filename,frameRateScaled/100.0f,params.VideoCodec,params.VideoQuality);
    break;

  default:
    encoder = new DummyVideoEncoder;
    break;
  }

  return encoder;
}

void videoStartNextPart(bool autoSize)
{
  // get the current audio format (so we can resume audio seamlessly)
  WAVEFORMATEX wfx;
  encoder->GetAudioFormat(&wfx);

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
    encoder->SetSize(captureWidth,captureHeight);

  // set audio format if we had one
  if(wfx.wFormatTag)
    encoder->SetAudioFormat(&wfx);
}

// capture buffer
int captureWidth = 0, captureHeight = 0;
unsigned char *captureData = 0;

void destroyCaptureBuffer()
{
  if(captureData)
  {
    captureWidth = 0;
    captureHeight = 0;

    delete[] captureData;
    captureData = 0;
  }
}

void createCaptureBuffer(int width,int height)
{
  destroyCaptureBuffer();

  captureWidth = width;
  captureHeight = height;
  captureData = new unsigned char[width*height*4];
}

void setCaptureResolution(int inWidth,int inHeight)
{
  bool unaligned;
  int width,height;

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

  if(width != captureWidth || height != captureHeight)
  {
    // if we've already seen frames, start a new part
    if(seenFrames)
      videoStartNextPart(false);

    if(unaligned)
      printLog("video: width/height not divisible by 4, aligning down (%dx%d)\n",inWidth,inHeight);

    printLog("video: capturing at %dx%d\n",width,height);
    createCaptureBuffer(width,height);

    if(encoder)
      encoder->SetSize(width,height);
  }
}

// advance frame
void nextFrame()
{
  seenFrames = true;
  nextFrameTiming();
  nextFrameSound();
}

static void blit32to24loop(unsigned char *dest,unsigned char *src,int count)
{
  static const unsigned __int64 mask = 0x00ffffff00ffffff;

  __asm
  {
    pxor      mm7, mm7;
    mov       esi, [src];
    mov       edi, [dest];
    mov       ecx, [count];
    shr       ecx, 3;
    jz        tail;

blitloop:
    movq      mm0, [esi];     // mm0=b0g0r0a0b1g1r1a1
    movq      mm1, [esi+8];   // mm1=b2g2r2a2b3g3r3a3
    movq      mm2, [esi+16];  // mm2=b4g4r4a4b5g5r5a5
    movq      mm3, [esi+24];  // mm3=b6g6r6a6b7g7r7a7

    pand      mm0, [mask];    // mm0=b0g0r0__b1g1r1__
    pand      mm1, [mask];    // mm1=b2g2r2__b3g3r3__
    pand      mm2, [mask];    // mm2=b4g4r4__b5g5r5__
    pand      mm3, [mask];    // mm3=b6g6r6__b7g7r7__

    movq      mm4, mm0;       // mm4=b0g0r0__b1g1r1__
    movq      mm5, mm1;       // mm5=b2g2r2__b3g3r3__
    movq      mm6, mm1;       // mm6=b2g2r2__b3g3r3__
    punpckhdq mm0, mm7;       // mm0=b1g1r1__________
    punpckldq mm4, mm7;       // mm4=b0g0r0__________
    psllq     mm5, 48;        // mm5=____________b2g2
    punpckldq mm1, mm7;       // mm1=b2g2r2__________
    psllq     mm0, 24;        // mm0=______b1g1r1____
    por       mm4, mm5;       // mm4=b0g0r0______b2g2
    punpckhdq mm6, mm7;       // mm6=b3g3r3__________
    psrlq     mm1, 16;        // mm1=r2______________
    por       mm0, mm4;       // mm0=b0g0r0b1g1r1b2g2
    movq      mm4, mm2;       // mm4=b4g4r4__b5g5r5__
    pxor      mm5, mm5;       // mm5=________________
    psllq     mm6, 8;         // mm6=__b3g3r3________
    punpckhdq mm2, mm7;       // mm2=b5g5r5__________
    movq      [edi], mm0;
    por       mm1, mm6;       // mm1=r2b3g3r3________
    psllq     mm2, 56;        // mm2=______________b5
    punpckhdq mm5, mm3;       // mm5=________b7g7r7__
    punpckldq mm1, mm4;       // mm1=r2b3g3r3b4g4r4__
    punpckldq mm3, mm7;       // mm3=b6g6r6__________
    psrlq     mm4, 40;        // mm4=g5r5____________
    psllq     mm5, 8;         // mm5=__________b7g7r7
    por       mm1, mm2;       // mm1=r2b3g3r3b4g4r4b5
    psllq     mm3, 16;        // mm3=____b6g6r6______
    por       mm4, mm5;       // mm4=g5r5______b7g7r7
    movq      [edi+8], mm1;
    add       esi, 32;
    por       mm3, mm4;       // mm3=g5r5b6g6r6b7g7r7
    movq      [edi+16], mm3;
    add       edi, 24;
    dec       ecx;
    jnz       blitloop;

tail:
    mov       ecx, [count];
    and       ecx, 7;
    jz        end;

tailloop:
    movsb;
    movsb;
    movsb;
    inc       esi;
    dec       ecx;
    jnz       tailloop;

end:
    emms;
  }
}

void blitAndFlipBGRAToCaptureData(unsigned char *source,unsigned pitch)
{
  for(int y=0;y<captureHeight;y++)
  {
    unsigned char *src = source + (captureHeight - 1 - y) * pitch;
    unsigned char *dst = captureData + y * captureWidth * 3;

    blit32to24loop(dst,src,captureWidth);
    /*// a better copy loop maybe would improve performance (then again, maybe
    // it wouldn't)
    for(int x=0;x<captureWidth;x++)
    {
      *dst++ = *src++;
      *dst++ = *src++;
      *dst++ = *src++;
      src++;
    }*/
  }
}

// public interface
void initVideo()
{
  partCounter = 1;
  seenFrames = false;

	initVideo_OpenGL();
	initVideo_Direct3D9();
	initVideo_Direct3D8();
	initVideo_DirectDraw();
}

void doneVideo()
{
  destroyCaptureBuffer();
}
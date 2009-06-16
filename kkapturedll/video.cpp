// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "videoencoder.h"
#include "video.h"

// capture buffer
int captureWidth = 0, captureHeight = 0;
unsigned char *captureData = 0;

HDC hCaptureDC = 0;
HBITMAP hCaptureBitmap = 0;

void destroyCaptureBuffer()
{
  if(captureData)
  {
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

void setCaptureResolution(int width,int height)
{
  printLog("video: capturing at %dx%d\n",width,height);
  createCaptureBuffer(width,height);

  if(encoder)
    encoder->SetSize(width,height);
}

// advance frame
void nextFrame()
{
  nextFrameTiming();
  nextFrameSound();
}

void captureGDIFrame(HDC hDC)
{
  // grab via DC bitblit (fallback solution)
  if(!hCaptureBitmap)
  {
    hCaptureDC = CreateCompatibleDC(hDC);
    hCaptureBitmap = CreateCompatibleBitmap(hDC,captureWidth,captureHeight);
    
    if(!hCaptureDC || !hCaptureBitmap)
    {
      if(hCaptureDC)
        DeleteDC(hCaptureDC);

      if(hCaptureBitmap)
        DeleteObject(hCaptureBitmap);

      hCaptureDC = 0;
      hCaptureBitmap = 0;
    }
    else
      SelectObject(hCaptureDC,hCaptureBitmap);
  }

  if(hCaptureBitmap && hCaptureDC)
  {
    BITMAPINFOHEADER bih;
    ZeroMemory(&bih,sizeof(bih));
    bih.biSize = sizeof(bih);
    bih.biWidth = captureWidth;
    bih.biHeight = captureHeight;
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    BitBlt(hCaptureDC,0,0,captureWidth,captureHeight,hDC,0,0,SRCCOPY);
    GetDIBits(hCaptureDC,hCaptureBitmap,0,captureHeight,captureData,
      (BITMAPINFO *)&bih,DIB_RGB_COLORS);

    encoder->WriteFrame(captureData);
  }
}

void captureGDIFullScreen()
{
  HDC hDC = GetDC(0);
  captureGDIFrame(hDC);
  ReleaseDC(0,hDC);
}

void blitAndFlipBGRAToCaptureData(unsigned char *source,unsigned pitch)
{
  for(int y=0;y<captureHeight;y++)
  {
    unsigned char *src = source + (captureHeight - 1 - y) * pitch;
    unsigned char *dst = captureData + y * captureWidth * 3;

    // a better copy loop maybe would improve performance (then again, maybe
    // it wouldn't)
    for(int x=0;x<captureWidth;x++)
    {
      *dst++ = *src++;
      *dst++ = *src++;
      *dst++ = *src++;
      src++;
    }
  }
}

// public interface
void initVideo()
{
	initVideo_OpenGL();
	initVideo_Direct3D9();
	initVideo_Direct3D8();
	initVideo_DirectDraw();
}

void doneVideo()
{
  if(hCaptureDC)
  {
    DeleteDC(hCaptureDC);
    hCaptureDC = 0;
  }

  if(hCaptureBitmap)
  {
    DeleteObject(hCaptureBitmap);
    hCaptureBitmap = 0;
  }

  destroyCaptureBuffer();
}
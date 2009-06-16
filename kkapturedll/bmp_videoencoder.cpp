// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "bmp_videoencoder.h"

// internal data
struct BMPVideoEncoder::Internal
{
  BITMAPFILEHEADER bmfh;
  BITMAPINFOHEADER bmih;
};

BMPVideoEncoder::BMPVideoEncoder(const char *namePrefix)
{
  strncpy(prefix,namePrefix,240);
  prefix[240] = 0;
  xRes = yRes = 0;
  intn = new Internal;

  ZeroMemory(&intn->bmfh,sizeof(BITMAPFILEHEADER));
  ZeroMemory(&intn->bmih,sizeof(BITMAPINFOHEADER));

  intn->bmfh.bfType = 0x4d42;
  intn->bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  intn->bmfh.bfSize = intn->bmfh.bfOffBits;

  intn->bmih.biSize = sizeof(BITMAPINFOHEADER);
  intn->bmih.biPlanes = 1;
  intn->bmih.biBitCount = 24;
  intn->bmih.biCompression = BI_RGB;
}

BMPVideoEncoder::~BMPVideoEncoder()
{
  delete intn;
}

void BMPVideoEncoder::SetSize(int _xRes,int _yRes)
{
  xRes = _xRes;
  yRes = _yRes;

  intn->bmfh.bfSize = xRes * yRes * 3 + intn->bmfh.bfOffBits;
  intn->bmih.biWidth = xRes;
  intn->bmih.biHeight = yRes;

  frame = 0;
}

void BMPVideoEncoder::WriteFrame(const unsigned char *buffer)
{
  if(xRes && yRes)
  {
    // create filename
    char filename[256];
    sprintf(filename,"%s%04d.bmp",prefix,frame);

    // create file, write headers+image
    FILE *f = fopen(filename,"wb");
    fwrite(&intn->bmfh,sizeof(BITMAPFILEHEADER),1,f);
    fwrite(&intn->bmih,sizeof(BITMAPINFOHEADER),1,f);
    fwrite(buffer,xRes*yRes,3,f);
    fclose(f);

    // frame is finished.
    frame++;
  }
}

void BMPVideoEncoder::SetAudioFormat(tWAVEFORMATEX *fmt)
{
}

void BMPVideoEncoder::WriteAudioFrame(const void *buffer,int samples)
{
}
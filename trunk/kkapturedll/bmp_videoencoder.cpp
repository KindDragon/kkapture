// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "bmp_videoencoder.h"

// internal data
struct BMPVideoEncoder::Internal
{
  BITMAPFILEHEADER bmfh;
  BITMAPINFOHEADER bmih;
  WAVEFORMATEX wfx;
  FILE *wave;
};

BMPVideoEncoder::BMPVideoEncoder(const char *fileName)
{
  char drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];

  _splitpath(fileName,drive,dir,fname,ext);
  _makepath(prefix,drive,dir,fname,"");
  xRes = yRes = 0;
  
  intn = new Internal;
  intn->wave = 0;

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
  if(intn->wave)
  {
    // finish the wave file by writing overall and data chunk lengths
    long fileLen = ftell(intn->wave);

    long riffLen = fileLen - 8;
    fseek(intn->wave,4,SEEK_SET);
    fwrite(&riffLen,1,sizeof(long),intn->wave);

    long dataLen = fileLen - 44;
    fseek(intn->wave,40,SEEK_SET);
    fwrite(&dataLen,1,sizeof(long),intn->wave);

    fclose(intn->wave);
  }

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
    char filename[512];
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

void BMPVideoEncoder::SetAudioFormat(const tWAVEFORMATEX *fmt)
{
  if(params.CaptureAudio && !intn->wave)
  {
    char filename[512];
    strcpy(filename,prefix);
    strcat(filename,".wav");

    intn->wfx = *fmt;
    intn->wave = fopen(filename,"wb");
    
    if(intn->wave)
    {
      static unsigned char header[] = "RIFF\0\0\0\0WAVEfmt ";
      static unsigned char data[] = "data\0\0\0\0";

      fwrite(header,1,sizeof(header)-1,intn->wave);
      DWORD len = sizeof(WAVEFORMATEX)-2;
      fwrite(&len,1,sizeof(DWORD),intn->wave);
      fwrite(fmt,1,len,intn->wave);
      fwrite(data,1,sizeof(data)-1,intn->wave);
    }

    // fill already written frames with no sound
    unsigned char *buffer = new unsigned char[fmt->nBlockAlign * 1024];
    int sampleFill = MulDiv(frame*100,fmt->nSamplesPerSec,frameRateScaled);

    memset(buffer,0,fmt->nBlockAlign * 1024);
    for(int samplePos=0;samplePos<sampleFill;samplePos+=1024)
      WriteAudioFrame(buffer,min(sampleFill-samplePos,1024));
  }
}

void BMPVideoEncoder::WriteAudioFrame(const void *buffer,int samples)
{
  if(intn->wave)
    fwrite(buffer,intn->wfx.nBlockAlign,samples,intn->wave);
}
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
#include "avi_videoencoder_vfw.h"
#include "video.h"
#include "audio_resample.h"

#pragma comment(lib, "vfw32.lib")

// internal data
struct AVIVideoEncoderVFW::Internal
{
  CRITICAL_SECTION lock;
  PAVIFILE file;
  PAVISTREAM vid,vidC;
  PAVISTREAM aud;
  WAVEFORMATEX wfx;

  char basename[_MAX_PATH];
  int segment;
  unsigned long codec;
  unsigned int quality;
  LONG overflowCounter;

  bool audioOk;
  AudioResampler resampler;
  WAVEFORMATEX targetFormat;
  int resampleSize;
  short *resampleBuf;

  bool initialized;
  bool formatSet;
};

void AVIVideoEncoderVFW::Init()
{
  AVISTREAMINFO asi;
  AVICOMPRESSOPTIONS aco;
  bool error = true;

  AVIFileInit();
  d->overflowCounter = 0;

  // create avi file
  char name[_MAX_PATH];
  _snprintf_s(name,_MAX_PATH,_TRUNCATE,"%s.%02d.avi",d->basename,d->segment);

  if(AVIFileOpen(&d->file,name,OF_CREATE|OF_WRITE,NULL) != AVIERR_OK)
  {
    printLog("avi_vfw: AVIFileOpen failed\n");
    goto cleanup;
  }

  // initialize video stream header
  ZeroMemory(&asi,sizeof(asi));
  asi.fccType               = streamtypeVIDEO;
  asi.dwScale               = 10000;
  asi.dwRate                = (DWORD) (10000*fps + 0.5f);
  asi.dwSuggestedBufferSize = xRes * yRes * 3;
  SetRect(&asi.rcFrame,0,0,xRes,yRes);
  strcpy_s(asi.szName,"Video");

  // create video stream
  if(AVIFileCreateStream(d->file,&d->vid,&asi) != AVIERR_OK)
  {
    printLog("avi_vfw: AVIFileCreateStream (video) failed\n");
    goto cleanup;
  }

  // create compressed stream
  unsigned long codec = d->codec;
  if(!codec)
    codec = mmioFOURCC('D','I','B',' '); // uncompressed frames

  ZeroMemory(&aco,sizeof(aco));
  aco.fccType = streamtypeVIDEO;
  aco.fccHandler = codec;
  aco.dwQuality = d->quality;

  if(AVIMakeCompressedStream(&d->vidC,d->vid,&aco,0) != AVIERR_OK)
  {
    printLog("avi_vfw: AVIMakeCompressedStream (video) failed\n");
    goto cleanup;
  }

  error = false;
  d->initialized = true;

cleanup:
  if(error)
    Cleanup();
}

void AVIVideoEncoderVFW::Cleanup()
{
  EnterCriticalSection(&d->lock);
  if(d->initialized)
  {
    printLog("avi_vfw: stopped recording\n");

    __try
    {
      if(d->aud)
      {
        AVIStreamRelease(d->aud);
        d->aud = 0;
      }

      if(d->vidC)
      {
        AVIStreamRelease(d->vidC);
        d->vidC = 0;
      }

      if(d->vid)
      {
        AVIStreamRelease(d->vid);
        d->vid = 0;
      }

      if(d->file)
      {
        AVIFileRelease(d->file);
        d->file = 0;
      }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
      printLog("avi_vfw: exception during avifile shutdown, video may be corrupted\n");
    }

    delete[] d->resampleBuf;

    AVIFileExit();
    printLog("avi_vfw: avifile shutdown complete\n");
    d->initialized = false;
  }
  
  LeaveCriticalSection(&d->lock);
}

void AVIVideoEncoderVFW::StartEncode()
{
  BITMAPINFOHEADER bmi;
  bool error = true;

  if(!d->file)
    return;

  EnterCriticalSection(&d->lock);

  // set stream format
  ZeroMemory(&bmi,sizeof(bmi));
  bmi.biSize        = sizeof(bmi);
  bmi.biWidth       = xRes;
  bmi.biHeight      = yRes;
  bmi.biPlanes      = 1;
  bmi.biBitCount    = 24;
  bmi.biCompression = BI_RGB;
  bmi.biSizeImage   = xRes * yRes * 3;
  if(AVIStreamSetFormat(d->vidC,0,&bmi,sizeof(bmi)) != AVIERR_OK)
  {
    printLog("avi_vfw: AVIStreamSetFormat (video) failed\n");
    goto cleanup;
  }

  error = false;
  printLog("avi_vfw: opened video stream at %.2f fps\n",fps);
  frame = 0;
  d->formatSet = true;

cleanup:
  LeaveCriticalSection(&d->lock);

  if(error)
    Cleanup();
}

void AVIVideoEncoderVFW::StartAudioEncode(const tWAVEFORMATEX *fmt)
{
  AVISTREAMINFO asi;
  bool error = true;

  if(!d->file)
    return;

  EnterCriticalSection(&d->lock);

  // initialize stream info
  ZeroMemory(&asi,sizeof(asi));
  asi.fccType               = streamtypeAUDIO;
  asi.dwScale               = 1;
  asi.dwRate                = fmt->nSamplesPerSec;
  asi.dwSuggestedBufferSize = fmt->nAvgBytesPerSec;
  asi.dwSampleSize          = fmt->nBlockAlign;
  strcpy_s(asi.szName,"Audio");

  // create the stream
  if(AVIFileCreateStream(d->file,&d->aud,&asi) != AVIERR_OK)
  {
    printLog("avi_vfw: AVIFileCreateStream (audio) failed\n");
    goto cleanup;
  }

  // set format
  if(AVIStreamSetFormat(d->aud,0,(LPVOID) fmt,sizeof(WAVEFORMATEX)) != AVIERR_OK)
  {
    printLog("avi_vfw: AVIStreamSetFormat (audio) failed\n");
    goto cleanup;
  }

  error = false;
  printLog("avi_vfw: opened audio stream at %d hz, %d channels, %d bits\n",
    fmt->nSamplesPerSec, fmt->nChannels, fmt->wBitsPerSample);
  audioSample = 0;
  audioBytesSample = fmt->nBlockAlign;

  d->targetFormat = *fmt;

  // fill already written frames with no sound
  unsigned char *buffer = new unsigned char[audioBytesSample * 1024];
  int sampleFill = int(1.0f * fmt->nSamplesPerSec * frame / fps);

  memset(buffer,0,audioBytesSample * 1024);
  for(int samplePos=0;samplePos<sampleFill;samplePos+=1024)
    WriteAudioFrame(buffer,min(sampleFill-samplePos,1024));

  delete[] buffer;

cleanup:
  LeaveCriticalSection(&d->lock);

  if(error)
    Cleanup();
}

AVIVideoEncoderVFW::AVIVideoEncoderVFW(const char *name,float _fps,unsigned long codec,unsigned int quality)
{
  bool error = true;

  xRes = yRes = 0;
  frame = 0;
  audioSample = 0;
  fps = _fps;

  d = new Internal;
  d->file = 0;
  d->vid = 0;
  d->vidC = 0;
  d->aud = 0;
  
  // determine file base name
  strcpy_s(d->basename,sizeof(d->basename),name);
  for(int i=strlen(d->basename)-1;i>=0;i--)
  {
    if(d->basename[i] == '/' || d->basename[i] == '\\')
      break;
    else if(d->basename[i] == '.')
    {
      d->basename[i] = 0;
      break;
    }
  }
  d->segment = 1;
  d->codec = codec;
  d->quality = quality;

  d->audioOk = false;
  d->resampleSize = 0;
  d->resampleBuf = 0;

  d->initialized = false;
  d->formatSet = false;
  InitializeCriticalSection(&d->lock);

  Init();
}

AVIVideoEncoderVFW::~AVIVideoEncoderVFW()
{
  Cleanup();
  DeleteCriticalSection(&d->lock);
  delete d;
}

void AVIVideoEncoderVFW::SetSize(int _xRes,int _yRes)
{
  xRes = _xRes;
  yRes = _yRes;
}

void AVIVideoEncoderVFW::WriteFrame(const unsigned char *buffer)
{
  // encode the frame
  EnterCriticalSection(&d->lock);

  if(!frame && !d->formatSet && xRes && yRes && params.CaptureVideo)
    StartEncode();

  if(d->formatSet && d->vidC)
  {
    if(d->overflowCounter >= 1024*1024*1800)
    {
      bool gotAudio = d->aud != 0;

      printLog("avi_vfw: segment %d reached maximum size, creating next segment.\n",d->segment);
      Cleanup();
      d->segment++;
      Init();

      StartEncode();
      if(gotAudio)
        StartAudioEncode(&d->wfx);
    }

    LONG written = 0;
    AVIStreamWrite(d->vidC,frame,1,(void *)buffer,3*xRes*yRes,0,0,&written);
    frame++;
    d->overflowCounter += written;
  }
  
  LeaveCriticalSection(&d->lock);
}

void AVIVideoEncoderVFW::SetAudioFormat(const tWAVEFORMATEX *fmt)
{
  d->wfx = *fmt;
  d->audioOk = d->resampler.Init(fmt,d->aud ? &d->targetFormat : fmt);
}

void AVIVideoEncoderVFW::GetAudioFormat(tWAVEFORMATEX *fmt)
{
  if(d->aud)
    *fmt = d->wfx;
  else
    ZeroMemory(fmt,sizeof(tWAVEFORMATEX));
}

void AVIVideoEncoderVFW::WriteAudioFrame(const void *buffer,int samples)
{
  EnterCriticalSection(&d->lock);

  if(params.CaptureAudio && !d->aud)
    StartAudioEncode(&d->wfx);

  if(d->aud)
  {
    int needSize = d->resampler.MaxOutputSamples(samples);
    if(needSize > d->resampleSize)
    {
      delete[] d->resampleBuf;
      d->resampleBuf = new short[needSize * 4];
      d->resampleSize = needSize;
    }

    int outSamples = d->resampler.Resample(buffer,d->resampleBuf,samples,false);

    if(outSamples)
    {
      LONG written = 0;
      /*AVIStreamWrite(d->aud,audioSample,samples,(LPVOID) buffer,
        samples*audioBytesSample,0,0,&written);*/
      AVIStreamWrite(d->aud,audioSample,outSamples,(LPVOID) d->resampleBuf,
        outSamples*audioBytesSample,0,0,&written);
      audioSample += outSamples;
      d->overflowCounter += written;
    }
  }

  LeaveCriticalSection(&d->lock);
}
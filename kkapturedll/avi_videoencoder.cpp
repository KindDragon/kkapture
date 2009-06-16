// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "avi_videoencoder.h"
#include "video.h"

#if !USE_DSHOW_AVI_WRITER

#pragma comment(lib, "vfw32.lib")

// internal data
struct AVIVideoEncoder::Internal
{
  CRITICAL_SECTION lock;
  PAVIFILE file;
  PAVISTREAM vid,vidC;
  PAVISTREAM aud;
  bool initialized;
  bool formatSet;
};

void AVIVideoEncoder::Cleanup()
{
  EnterCriticalSection(&d->lock);
  if(d->initialized)
  {
    printLog("avi: stopped recording\n");

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
      printLog("avi: exception during avifile shutdown, video may be corrupted\n");
    }

    AVIFileExit();
    printLog("avi: avifile shutdown complete\n");
    d->initialized = false;
  }
  
  LeaveCriticalSection(&d->lock);
}

void AVIVideoEncoder::StartEncode()
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
    printLog("avi: AVIStreamSetFormat (video) failed\n");
    goto cleanup;
  }

  error = false;
  printLog("avi: opened video stream at %.2f fps\n",fps);
  frame = 0;
  d->formatSet = true;

cleanup:
  LeaveCriticalSection(&d->lock);

  if(error)
    Cleanup();
}

void AVIVideoEncoder::StartAudioEncode(const tWAVEFORMATEX *fmt)
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
  strcpy(asi.szName,"Audio");

  // create the stream
  if(AVIFileCreateStream(d->file,&d->aud,&asi) != AVIERR_OK)
  {
    printLog("avi: AVIFileCreateStream (audio) failed\n");
    goto cleanup;
  }

  // set format
  if(AVIStreamSetFormat(d->aud,0,(LPVOID) fmt,sizeof(WAVEFORMATEX)) != AVIERR_OK)
  {
    printLog("avi: AVIStreamSetFormat (audio) failed\n");
    goto cleanup;
  }

  error = false;
  printLog("avi: opened audio stream at %d hz, %d channels, %d bits\n",
    fmt->nSamplesPerSec, fmt->nChannels, fmt->wBitsPerSample);
  audioSample = 0;
  audioBytesSample = fmt->nBlockAlign;

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

AVIVideoEncoder::AVIVideoEncoder(const char *name,float _fps,unsigned long codec,unsigned quality)
{
  AVISTREAMINFO asi;
  AVICOMPRESSOPTIONS aco;
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
  d->initialized = true;
  d->formatSet = false;
  InitializeCriticalSection(&d->lock);

  AVIFileInit();

  // create avi file
  if(AVIFileOpen(&d->file,name,OF_CREATE|OF_WRITE,NULL) != AVIERR_OK)
  {
    printLog("AVIFileOpen failed\n");
    goto cleanup;
  }

  // initialize video stream header
  ZeroMemory(&asi,sizeof(asi));
  asi.fccType               = streamtypeVIDEO;
  asi.dwScale               = 10000;
  asi.dwRate                = (DWORD) (10000*fps + 0.5f);
  asi.dwSuggestedBufferSize = xRes * yRes * 3;
  SetRect(&asi.rcFrame,0,0,xRes,yRes);
  strcpy(asi.szName,"Video");

  // create video stream
  if(AVIFileCreateStream(d->file,&d->vid,&asi) != AVIERR_OK)
  {
    printLog("avi: AVIFileCreateStream (video) failed\n");
    goto cleanup;
  }

  // create compressed stream
  if(!codec)
    codec = mmioFOURCC('D','I','B',' '); // uncompressed frames

  ZeroMemory(&aco,sizeof(aco));
  aco.fccType = streamtypeVIDEO;
  aco.fccHandler = codec;
  aco.dwQuality = quality;

  if(AVIMakeCompressedStream(&d->vidC,d->vid,&aco,0) != AVIERR_OK)
  {
    printLog("avi: AVIMakeCompressedStream (video) failed\n");
    goto cleanup;
  }

  error = false;

cleanup:
  if(error)
    Cleanup();
}

AVIVideoEncoder::~AVIVideoEncoder()
{
  Cleanup();
  DeleteCriticalSection(&d->lock);
  delete d;
}

void AVIVideoEncoder::SetSize(int _xRes,int _yRes)
{
  xRes = _xRes;
  yRes = _yRes;
}

void AVIVideoEncoder::WriteFrame(const unsigned char *buffer)
{
  // encode the frame
  EnterCriticalSection(&d->lock);

  if(!frame && !d->formatSet && xRes && yRes && params.CaptureVideo)
    StartEncode();

  if(d->formatSet && d->vidC)
  {
    AVIStreamWrite(d->vidC,frame,1,(void *)buffer,3*xRes*yRes,0,0,0);
    frame++;
  }
  
  LeaveCriticalSection(&d->lock);
}

void AVIVideoEncoder::SetAudioFormat(const tWAVEFORMATEX *fmt)
{
  if(params.CaptureAudio && !d->aud)
    StartAudioEncode(fmt);
}

void AVIVideoEncoder::WriteAudioFrame(const void *buffer,int samples)
{
  EnterCriticalSection(&d->lock);

  if(d->aud)
  {
    AVIStreamWrite(d->aud,audioSample,samples,(LPVOID) buffer,
      samples*audioBytesSample,0,0,0);
    audioSample += samples;
  }

  LeaveCriticalSection(&d->lock);
}

#endif // !USE_DSHOW_AVI_WRITER
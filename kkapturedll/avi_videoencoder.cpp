// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "avi_videoencoder.h"

// internal data
struct AVIVideoEncoder::Internal
{
  PAVIFILE file;
  PAVISTREAM vid,vidC;
  PAVISTREAM aud;
};

void AVIVideoEncoder::Cleanup()
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
}

void AVIVideoEncoder::StartEncode()
{
  BITMAPINFOHEADER bmi;
  bool error = true;

  if(!d->file)
    return;

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

cleanup:
  if(error)
    Cleanup();
}

void AVIVideoEncoder::StartAudioEncode(tWAVEFORMATEX *fmt)
{
  AVISTREAMINFO asi;
  bool error = true;

  if(!d->file)
    return;

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
  if(AVIStreamSetFormat(d->aud,0,fmt,sizeof(WAVEFORMATEX)) != AVIERR_OK)
  {
    printLog("avi: AVIStreamSetFormat (audio) failed\n");
    goto cleanup;
  }

  error = false;
  printLog("avi: opened audio stream at %d hz, %d channels, %d bits\n",
    fmt->nSamplesPerSec, fmt->nChannels, fmt->wBitsPerSample);
  audioSample = 0;
  audioBytesSample = fmt->nBlockAlign;

  // fill already written frames with 0 sound
  unsigned char *buffer = new unsigned char[audioBytesSample * 1024];
  memset(buffer,0,audioBytesSample * 1024);
  int sampleFill = int(1.0f * fmt->nSamplesPerSec * frame / fps);

  for(int samplePos=0;samplePos<sampleFill;samplePos+=1024)
    WriteAudioFrame(buffer,min(sampleFill-samplePos,1024));

  delete[] buffer;

cleanup:
  if(error)
    Cleanup();
}

AVIVideoEncoder::AVIVideoEncoder(const char *name,float _fps)
{
  AVISTREAMINFO asi;
  AVICOMPRESSOPTIONS aco;
  AVICOMPRESSOPTIONS *optlist[1];
  bool gotOptions = false, error = true;

  xRes = yRes = 0;
  frame = 0;
  audioSample = 0;
  fps = _fps;

  AVIFileInit();

  d = new Internal;
  d->file = 0;
  d->vid = 0;
  d->vidC = 0;
  d->aud = 0;

  // create avi file
  if(AVIFileOpen(&d->file,name,OF_WRITE|OF_CREATE,NULL) != AVIERR_OK)
  {
    printLog("AVIFileOpen failed\n");
    d->file = 0;
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

  // get coding options
  ZeroMemory(&aco,sizeof(aco));
  aco.fccType = streamtypeVIDEO;
  optlist[0] = &aco;

  gotOptions = true;
  if(!AVISaveOptions(GetForegroundWindow(),0,1,&d->vid,optlist))
  {
    printLog("avi: AVISaveOptions failed (video)\n");
    goto cleanup;
  }

  // create compressed stream
  if(AVIMakeCompressedStream(&d->vidC,d->vid,optlist[0],0) != AVIERR_OK)
  {
    printLog("avi: AVIMakeCompressedStream (video) failed\n");
    goto cleanup;
  }

  error = false;

cleanup:
  if(gotOptions)
    AVISaveOptionsFree(1,optlist);

  if(error)
    Cleanup();
}

AVIVideoEncoder::~AVIVideoEncoder()
{
  Cleanup();
  delete d;
}

void AVIVideoEncoder::SetSize(int _xRes,int _yRes)
{
  xRes = _xRes;
  yRes = _yRes;
  if(d->file && !frame)
    StartEncode();
}

void AVIVideoEncoder::WriteFrame(const unsigned char *buffer)
{
  // encode the frame
  if(d->vidC)
  {
    AVIStreamWrite(d->vidC,frame,1,(void *)buffer,3*xRes*yRes,0,0,0);

    frame++;
  }
}

void AVIVideoEncoder::SetAudioFormat(tWAVEFORMATEX *fmt)
{
  if(!d->aud)
    StartAudioEncode(fmt);
}

void AVIVideoEncoder::WriteAudioFrame(const void *buffer,int samples)
{
  if(d->aud)
  {
    AVIStreamWrite(d->aud,audioSample,samples,(LPVOID) buffer,
      samples*audioBytesSample,0,0,0);
    audioSample += samples;
  }
}
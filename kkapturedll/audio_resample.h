// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.
//
// note this is by no means a general audio resampler, it's only meant
// for the purposes of kkapture... so no really good resampling routines,
// only cubic spline (catmull-rom) interpolation

#ifndef __AUDIO_RESAMPLE_H__
#define __AUDIO_RESAMPLE_H__

struct tWAVEFORMATEX;

class AudioResampler
{
  bool Initialized;
  bool In8Bit;
  int InChans;
  int InRate,OutRate;
  bool Identical;

  int tFrac;
  int dStep;
  float invScale;
  int InitSamples;
  float ChannelBuf[2][4];

  template<class T> int ResampleChan(T *in,short *out,int nInSamples,float *chanBuf);
  template<class T> int ResampleInit(T *in,int nInSamples,float *chanBuf);
  template<class T> int ResampleBlock(T *in,short *out,int nInSamples);

public:
  AudioResampler();
  ~AudioResampler();

  bool Init(const tWAVEFORMATEX *srcFormat,const tWAVEFORMATEX *dstFormat);
  int MaxOutputSamples(int nInSamples) const;
  int Resample(const void *src,short *out,int nInSamples,bool last);
};

#endif
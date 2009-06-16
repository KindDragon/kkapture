// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#ifndef __VIDEO_H__
#define __VIDEO_H__

class VideoEncoder;

// setup
void initVideo();
void doneVideo();

// init functions
void initVideo_OpenGL();
void initVideo_Direct3D9();
void initVideo_Direct3D8();
void initVideo_DirectDraw();

// helpers
void destroyCaptureBuffer();
void createCaptureBuffer(int width,int height);
void setCaptureResolution(int width,int height);
void nextFrame();

// fallback capturing+buffering
void captureGDIFrame(HDC hDC);
void captureGDIFullScreen();

void blitAndFlipBGRAToCaptureData(unsigned char *source,unsigned pitch);

extern int captureWidth, captureHeight;
extern unsigned char *captureData;

extern HDC hCaptureDC;
extern HBITMAP hCaptureBitmap;

#endif
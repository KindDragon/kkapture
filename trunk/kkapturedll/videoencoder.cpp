// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#include "stdafx.h"
#include "videoencoder.h"

VideoEncoder::~VideoEncoder()
{
}

// ---

DummyVideoEncoder::DummyVideoEncoder()
{
}

DummyVideoEncoder::~DummyVideoEncoder()
{
}

void DummyVideoEncoder::SetSize(int xRes,int yRes)
{
}

void DummyVideoEncoder::WriteFrame(const unsigned char *buffer)
{
}

void DummyVideoEncoder::SetAudioFormat(tWAVEFORMATEX *fmt)
{
}

void DummyVideoEncoder::WriteAudioFrame(const void *buffer,int samples)
{
}
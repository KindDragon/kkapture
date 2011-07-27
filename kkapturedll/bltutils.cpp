/* kkapture: intrusive demo video capturing.
 * Copyright (c) 2005-2010 Fabian "ryg/farbrausch" Giesen.
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
#include "video.h"
#include "bltutils.h"


// capture buffer
VideoCaptureBuffer captureBuffer;

VideoCaptureBuffer::VideoCaptureBuffer()
: width(0), height(0), dstBpp(params.DestBpp),
  data(0)
{
}

void VideoCaptureBuffer::init(int inWidth, int inHeight, int inBpp)
{
  destroy();

  VideoCaptureDataLock lock;

  width = inWidth;
  height = inHeight;
  dstBpp = inBpp;
  data = (unsigned char*)_aligned_malloc(width*height*4, 16);
}

void VideoCaptureBuffer::destroy()
{
  VideoCaptureDataLock lock;

  if(data)
  {
    width = 0;
    height = 0;

    _aligned_free(data);
    data = 0;
  }
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

void VideoCaptureBuffer::blitAndFlipBGRA(unsigned char *source,unsigned pitch)
{
  if (dstBpp == 3)
  {
	  for(int y=0;y<height;y++)
	  {
	    unsigned char *src = source + (height - 1 - y) * pitch;
	    unsigned char *dst = data + y * width * 3;

	    blit32to24loop(dst,src,width);
	  }
  } else if (dstBpp == 4)
  {
	  for(int y=0;y<height;y++)
	  {
	    unsigned char *src = source + (height - 1 - y) * pitch;
	    unsigned char *dst = data + y * width * 4;

		memcpy(dst, src, width * 4);
	  }
  }
}

void VideoCaptureBuffer::blitAndFlipRGBA(unsigned char *source,unsigned pitch)
{
  if (dstBpp == 3)
  {
	  for(int y=0;y<height;y++)
	  {
	    unsigned char *src = source + (height - 1 - y) * pitch;
	    unsigned char *dst = data + y * width * 3;

	    for(int x=0;x<width;x++)
	    {
	      dst[0] = src[2];
	      dst[1] = src[1];
	      dst[2] = src[0];
	      dst += 3;
	      src += 4;
	    }
	  }
  } else if (dstBpp == 4)
  {
	  for(int y=0;y<height;y++)
	  {
	    unsigned char *src = source + (height - 1 - y) * pitch;
	    unsigned char *dst = data + y * width * 4;

	    for(int x=0;x<width;x++)
	    {
	      dst[0] = src[2];
	      dst[1] = src[1];
	      dst[2] = src[0];
	      dst[3] = src[3];
	      dst += 3;
	      src += 3;
	    }
	  }
  }
}

// generic blitter class

static void CalcLookupFromMask(unsigned char *lookup,int &outShift,int &outMask,unsigned inMask)
{
  outShift = 0;
  while(outShift<32 && !(inMask & 1))
  {
    outShift++;
    inMask >>= 1;
  }

  outMask = min(inMask,255);
  for(int i=0;i<=outMask;i++)
    lookup[i] = (i * 255) / outMask;
}


void GenericBlitter::Blit1ByteSrc(unsigned char *src,unsigned char *dst,int count)
{
  do
  {
    unsigned source = *src++;
    *dst++ = BTab[(source >> BShift) & BMask];
    *dst++ = GTab[(source >> GShift) & GMask];
    *dst++ = RTab[(source >> RShift) & RMask];
  }
  while(--count);
}

void GenericBlitter::Blit2ByteSrc(unsigned char *src,unsigned char *dst,int count)
{
  unsigned short *srcp = (unsigned short *) src;

  do
  {
    unsigned source = *srcp++;
    *dst++ = BTab[(source >> BShift) & BMask];
    *dst++ = GTab[(source >> GShift) & GMask];
    *dst++ = RTab[(source >> RShift) & RMask];
  }
  while(--count);
}

void GenericBlitter::Blit3ByteSrc(unsigned char *src,unsigned char *dst,int count)
{
  do
  {
    unsigned source = src[0] | (src[1] << 8) | (src[2] << 16);
    src += 3;
    *dst++ = BTab[(source >> BShift) & BMask];
    *dst++ = GTab[(source >> GShift) & GMask];
    *dst++ = RTab[(source >> RShift) & RMask];
  }
  while(--count);
}

void GenericBlitter::Blit4ByteSrc(unsigned char *src,unsigned char *dst,int count)
{
  unsigned long *srcp = (unsigned long *) src;

  do
  {
    unsigned source = *srcp++;
    *dst++ = BTab[(source >> BShift) & BMask];
    *dst++ = GTab[(source >> GShift) & GMask];
    *dst++ = RTab[(source >> RShift) & RMask];
  }
  while(--count);
}

GenericBlitter::GenericBlitter()
{
  SetInvalidFormat();
}

void GenericBlitter::SetInvalidFormat()
{
  BytesPerPixel = 0;
  Paletted = false;
  
  RTab[0] = GTab[0] = BTab[0] = 0;
  RMask = GMask = BMask = 0;
}

void GenericBlitter::SetRGBFormat(int bits,unsigned int redMask,unsigned int greenMask,unsigned int blueMask)
{
  if(bits < 8)
    SetInvalidFormat();
  else
  {
    BytesPerPixel = (bits + 7) / 8;
    Paletted = false;

    CalcLookupFromMask(RTab,RShift,RMask,redMask);
    CalcLookupFromMask(GTab,GShift,GMask,greenMask);
    CalcLookupFromMask(BTab,BShift,BMask,blueMask);
  }
}

void GenericBlitter::SetPalettedFormat(int bits)
{
  if(bits != 8)
    SetInvalidFormat();
  else
  {
    BytesPerPixel = 1;
    Paletted = true;

    RShift = GShift = BShift = 0;
    RMask = GMask = BMask = 255;
  }
}

void GenericBlitter::SetPalette(const struct tagPALETTEENTRY *palette,int nEntries)
{
  if(Paletted)
  {
    nEntries = min(nEntries,256);

    for(int i=0;i<nEntries;i++)
    {
      RTab[i] = palette[i].peRed;
      GTab[i] = palette[i].peGreen;
      BTab[i] = palette[i].peBlue;
    }

    for(int i=nEntries;i<256;i++)
    {
      RTab[i] = 0;
      GTab[i] = 0;
      BTab[i] = 0;
    }
  }
}

bool GenericBlitter::IsPaletted() const
{
  return Paletted;
}

int GenericBlitter::GetBytesPerPixel() const
{
  return BytesPerPixel;
}

void GenericBlitter::BlitOneLine(unsigned char *src,unsigned char *dst,int count)
{
  if(BytesPerPixel == 3 && RShift == 16 && RMask == 255 &&
    GShift == 8 && GMask == 255 && BShift == 0 && BMask == 255)
  {
    memcpy(dst,src,count*3);
  }
  else if(BytesPerPixel == 4 && RShift == 16 && RMask == 255 &&
    GShift == 8 && GMask == 255 && BShift == 0 && BMask == 255)
    blit32to24loop(dst,src,count);
  else
  {
    switch(BytesPerPixel)
    {
    case 1: Blit1ByteSrc(src,dst,count); break;
    case 2: Blit2ByteSrc(src,dst,count); break;
    case 3: Blit3ByteSrc(src,dst,count); break;
    case 4: Blit4ByteSrc(src,dst,count); break;
    }
  }
}

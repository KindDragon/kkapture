// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#pragma once

#include <windows.h>
#include "detours.h"

#include <vfw.h>

#include <stdio.h>
#include <stdarg.h>

#include "main.h"
#include "sound.h"
#include "timing.h"
#include "util.h"

#if USE_DSHOW_AVI_WRITER
#include "streams.h"
#endif
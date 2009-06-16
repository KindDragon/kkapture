// kkapture: intrusive demo video capturing.
// by fabian "ryg/farbrausch" giesen 2005.

#pragma once

#include <windows.h>
#include <dsound.h>
#include <vfw.h>
#include <gl/gl.h>
#include "d3d9.h"
#include "detours.h"

#include <stdio.h>
#include <stdarg.h>

#include "main.h"
#include "vtbl_patch.h"
#include "video.h"
#include "sound.h"
#include "timing.h"
#include "log.h"
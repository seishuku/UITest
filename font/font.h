#ifndef __FONT_H__
#define __FONT_H__

#include <stdint.h>
#include <ddraw.h>
#include "font_6x10.h"

void Font_Print(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, const char *string, ...);

#endif

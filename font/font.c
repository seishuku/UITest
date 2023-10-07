#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <ddraw.h>
#include "font.h"

void point(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, float c[3]);

static void Font_PutChar(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, char c)
{
	for(uint32_t j=0;j<FONT_HEIGHT;j++)
	{
		for(uint32_t i=0;i<FONT_WIDTH;i++)
		{
			if(fontdata[(c*FONT_HEIGHT)+j]&(0x80>>i))
				point(ddsd, x+i, y+j, (float[]){ 1.0f, 1.0f, 1.0f });
		}
	}
}

void Font_Print(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, const char *string, ...)
{
	char *ptr, text[1024]; //Big enough for full screen.
	va_list	ap;
	int sx=x;

	if(string==NULL)
		return;

	va_start(ap, string);
		vsnprintf(text, 1024, string, ap);
	va_end(ap);

	for(ptr=text;*ptr!='\0';ptr++)
	{
		if(*ptr=='\n'||*ptr=='\r')
		{
			x=sx;
			y+=FONT_HEIGHT;
			continue;
		}

		if(*ptr=='\t')
		{
			x+=FONT_WIDTH*4;
			continue;
		}

		Font_PutChar(ddsd, x, y, *ptr);
		x+=FONT_WIDTH;
	}
}

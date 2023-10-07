#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <ddraw.h>
#include "../utils/genid.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Initialize UI system.
bool UI_Init(UI_t *UI, uint32_t Width, uint32_t Height)
{
	if(UI==NULL||Width==0||Height==0)
		return false;

	// Set screen width/height
	UI->Width=Width;
	UI->Height=Height;

	// Initial 10 pre-allocated list of buttons, uninitialized
	List_Init(&UI->Controls, sizeof(UI_Control_t), 10, NULL);

	return true;
}

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID)
{
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->Controls, i);

		// Check for matching ID and type
		if(Control->ID==ID)
			return Control;
	}

	return NULL;
}

// Returns ID of hit, otherwise returns UINT32_MAX
// Position could be mouse coords in the area where the controls are placed
uint32_t UI_TestHit(UI_t *UI, vec2 Position)
{
	if(UI==NULL)
		return UINT32_MAX;

	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		switch(Control->Type)
		{
			case UI_CONTROL_BUTTON:
				if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->Button.Size.x&&
				   Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->Button.Size.y)
				{
					// TODO: This could potentionally be an issue if the callback blocks
					if(Control->Button.Callback)
						Control->Button.Callback(NULL);
					return Control->ID;
				}
				break;

			case UI_CONTROL_CHECKBOX:
				vec2 Normal=Vec2_Subv(Control->Position, Position);

				if(Vec2_Dot(Normal, Normal)<=Control->CheckBox.Radius*Control->CheckBox.Radius)
				{
					Control->CheckBox.Value=!Control->CheckBox.Value;
					return Control->ID;
				}
				break;

			case UI_CONTROL_BARGRAPH:
				if(!Control->BarGraph.Readonly)
				{
					// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
					if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->BarGraph.Size.x&&
					   Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->BarGraph.Size.y)
					{
						Control->BarGraph.Value=((Position.x-Control->Position.x)/Control->BarGraph.Size.x)*(Control->BarGraph.Max-Control->BarGraph.Min)+Control->BarGraph.Min;
						return Control->ID;
					}
				}
				break;
		}
	}

	// Nothing found
	return UINT32_MAX;
}

void point(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, float c[3]);
void line(DDSURFACEDESC2 ddsd, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, float c[3]);
void roundedrect(DDSURFACEDESC2 ddsd, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t r, float c[3]);
void fillroundedrect(DDSURFACEDESC2 ddsd, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t r, float c[3]);
void circle(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, uint32_t r, float c[3]);
void fillcircle(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, uint32_t r, float c[3]);

bool UI_Draw(UI_t *UI, DDSURFACEDESC2 ddsd)
{
	if(UI==NULL)
		return false;

	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		switch(Control->Type)
		{
			case UI_CONTROL_BUTTON:
			{
				uint32_t x=(uint32_t)Control->Position.x;
				uint32_t y=(uint32_t)Control->Position.y;
				uint32_t w=(uint32_t)Control->Button.Size.x;
				uint32_t h=(uint32_t)Control->Button.Size.y;
				uint32_t textlen=(uint32_t)strlen(Control->TitleText);

				fillroundedrect(ddsd, x, y, x+w, y+h, 5, (float[]){ 1.0f, 1.0f, 1.0f });
				fillroundedrect(ddsd, x+1, y+1, x+w, y+h, 5, (float[]){ 0.25f, 0.25f, 0.25f });
				Font_Print(ddsd, x+(w-textlen*FONT_WIDTH)/2, y+(h-FONT_HEIGHT)/2, "%s", Control->TitleText);
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				uint32_t x=(uint32_t)Control->Position.x;
				uint32_t y=(uint32_t)Control->Position.y;
				uint32_t r=(uint32_t)Control->CheckBox.Radius;

				circle(ddsd, x, y, r, (float[]){ 1.0f, 1.0f, 1.0f });
				circle(ddsd, x+1, y+1, r, (float[]){ 0.25f, 0.25f, 0.25f });
				Font_Print(ddsd, x+r+2, y-(FONT_HEIGHT/2), "%s", Control->TitleText);

				if(Control->CheckBox.Value)
					fillcircle(ddsd, x, y, r-3, (float *)&Control->Color.x);
				break;
			}

			case UI_CONTROL_BARGRAPH:
			{
				uint32_t x=(uint32_t)Control->Position.x;
				uint32_t y=(uint32_t)Control->Position.y;
				uint32_t w=(uint32_t)Control->BarGraph.Size.x;
				uint32_t h=(uint32_t)Control->BarGraph.Size.y;
				uint32_t textlen=(uint32_t)strlen(Control->TitleText);
				float normalize_value=(Control->BarGraph.Value-Control->BarGraph.Min)/(Control->BarGraph.Max-Control->BarGraph.Min);
				uint32_t value=(uint32_t)(normalize_value*(Control->BarGraph.Size.x-6));

				roundedrect(ddsd, x, y, x+w, y+h, 5, (float[]){ 1.0f, 1.0f, 1.0f });
				roundedrect(ddsd, x+1, y+1, x+w, y+h, 5, (float[]){ 0.25f, 0.25f, 0.25f });
				fillroundedrect(ddsd, x+3, y+3, x+3+value, y-3+h, 2, (float *)&Control->Color.x);
				Font_Print(ddsd, x+(w-textlen*FONT_WIDTH)/2, y+(h-FONT_HEIGHT)/2, "%s", Control->TitleText);
				break;
			}
		}
	}

	return true;
}

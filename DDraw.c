#include <windows.h>
#include <ddraw.h>
#include <dsound.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <intrin.h>
#include "math/math.h"
#include "font/font.h"
#include "ui/ui.h"

LPDIRECTDRAW7 lpDD=NULL;
LPDIRECTDRAWSURFACE7 lpDDSFront=NULL;
LPDIRECTDRAWSURFACE7 lpDDSBack=NULL;
HWND hWnd=NULL;

char *szAppName="DirectDraw";

uint32_t Width=512, Height=512;

bool Done=false, Key[256];
bool MouseClicked=false;

double StartTime, EndTime;
double fTimeStep, fTime=0.0;

float X=0.0f, Y=0.0f;

UI_t UI;

typedef struct
{
	vec2 Position;
	vec2 PrevPosition;
	float Mass;
	bool Locked;
} Point_t;

typedef struct
{
	Point_t *PointA, *PointB;
	float Length;
} Stick_t;

void VerletIntegration(uint32_t NumPoints, Point_t *Points, uint32_t NumSticks, Stick_t *Sticks, float DeltaTime, uint32_t Iterations)
{
	for(uint32_t i=0;i<NumPoints;i++)
	{
		if(!Points[i].Locked)
		{
			float drag=0.1f;
			vec2 acceleration={ 0.0f, 9.8f*10000 };
			vec2 PrevPosition=Points[i].Position;

			Points[i].Position.x=Points[i].Position.x+(Points[i].Position.x-Points[i].PrevPosition.x)*(1.0f-drag)+acceleration.x*(1.0f-drag)*DeltaTime*DeltaTime;
			Points[i].Position.y=Points[i].Position.y+(Points[i].Position.y-Points[i].PrevPosition.y)*(1.0f-drag)+acceleration.y*(1.0f-drag)*DeltaTime*DeltaTime;

			Points[i].PrevPosition=PrevPosition;
		}
	}

	// Integrate stick length over multiple iterations to stabilize simulation
	for(uint32_t j=0;j<Iterations;j++)
	{
		for(uint32_t i=0;i<NumSticks;i++)
		{
			vec2 StickCenter, StickDir;

			// Find the center of the stick (A-B)/2
			StickCenter=Vec2_Muls(Vec2_Addv(Sticks[i].PointA->Position, Sticks[i].PointB->Position), 0.5f);

			// Find the direction the stick is pointing
			StickDir=Vec2_Subv(Sticks[i].PointA->Position, Sticks[i].PointB->Position);
			Vec2_Normalize(&StickDir);

			// Get one half of the stick's length in that direction
			StickDir=Vec2_Muls(StickDir, Sticks[i].Length*0.5f);

			if(!Sticks[i].PointA->Locked)
			{
				// Adjust point A's position from the center of the stick going toward point A
				Sticks[i].PointA->Position=Vec2_Addv(StickCenter, StickDir);
			}

			if(!Sticks[i].PointB->Locked)
			{
				// Adjust point B's position from the center of the stick going toward point B
				Sticks[i].PointB->Position=Vec2_Subv(StickCenter, StickDir);
			}
		}
	}
}

Point_t Points[4];
Stick_t Sticks[6];

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void Render(void);
int Init(void);
int Create(void);
void Destroy(void);

double GetClock(void)
{
	static uint64_t Frequency=0;
	uint64_t Count;

	if(!Frequency)
		QueryPerformanceFrequency((LARGE_INTEGER *)&Frequency);

	QueryPerformanceCounter((LARGE_INTEGER *)&Count);

	return (double)Count/Frequency;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
	WNDCLASS wc;
	wc.style=CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
	wc.lpfnWndProc=WndProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=hInstance;
	wc.hIcon=LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName=NULL;
	wc.lpszClassName=szAppName;

	RegisterClass(&wc);

	RECT WindowRect;
	WindowRect.left=0;
	WindowRect.right=Width;
	WindowRect.top=0;
	WindowRect.bottom=Height;

	AdjustWindowRect(&WindowRect, WS_OVERLAPPEDWINDOW, FALSE);

	hWnd=CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, WindowRect.right-WindowRect.left, WindowRect.bottom-WindowRect.top, NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, SW_SHOW);
	SetForegroundWindow(hWnd);

	if(!Create())
	{
		DestroyWindow(hWnd);
		return -1;
	}

	if(!Init())
	{
		Destroy();
		DestroyWindow(hWnd);
		return -1;
	}

	MSG msg={ 0 };
	while(!Done)
	{
		double StartTime=GetClock();

		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
				Done=1;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			RECT RectSrc, RectDst;
			POINT Point={ 0, 0 };

			if(IDirectDrawSurface7_IsLost(lpDDSBack)==DDERR_SURFACELOST)
				IDirectDrawSurface7_Restore(lpDDSBack);

			if(IDirectDrawSurface7_IsLost(lpDDSFront)==DDERR_SURFACELOST)
				IDirectDrawSurface7_Restore(lpDDSFront);

			Render();

			ClientToScreen(hWnd, &Point);
			GetClientRect(hWnd, &RectDst);
			OffsetRect(&RectDst, Point.x, Point.y);
			SetRect(&RectSrc, 0, 0, Width, Height);

			IDirectDrawSurface7_Blt(lpDDSFront, &RectDst, lpDDSBack, &RectSrc, DDBLT_WAIT, NULL);
		}

		fTimeStep=(float)(GetClock()-StartTime);
		fTime+=fTimeStep;
	}

	Destroy();
	DestroyWindow(hWnd);

	return (int)msg.wParam;
}

#include <windowsx.h>

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_CREATE:
		break;

	case WM_CLOSE:
		PostQuitMessage(0);
		break;

	case WM_DESTROY:
		break;

	case WM_SIZE:
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		ShowCursor(FALSE);

		X=(float)GET_X_LPARAM(lParam);
		Y=(float)GET_Y_LPARAM(lParam);

		MouseClicked=true;
		break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		ShowCursor(TRUE);
		MouseClicked=false;

		UI_TestHit(&UI, Vec2(X, Y));
		break;

	case WM_MOUSEMOVE:
		if(MouseClicked)
		{
			X=(float)GET_X_LPARAM(lParam);
			Y=(float)GET_Y_LPARAM(lParam);
			UI_TestHit(&UI, Vec2(X, Y));
		}
		break;

	case WM_KEYDOWN:
		Key[wParam]=true;

		switch(wParam)
		{
		case VK_SPACE:
			Points[0].Locked=!Points[0].Locked;
			break;

		case VK_ESCAPE:
			PostQuitMessage(0);
			break;

		default:
			break;
		}
		break;

	case WM_KEYUP:
		Key[wParam]=false;
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

void Clear(LPDIRECTDRAWSURFACE7 lpDDS)
{
	DDBLTFX ddbltfx;

	ddbltfx.dwSize=sizeof(DDBLTFX);
	ddbltfx.dwFillColor=0x00000000;

	if(IDirectDrawSurface7_Blt(lpDDS, NULL, NULL, NULL, DDBLT_COLORFILL, &ddbltfx)!=DD_OK)
		return;
}

void point(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, float c[3])
{
	if(x<1)
		return;
	if(x>ddsd.dwWidth-1)
		return;
	if(y<1)
		return;
	if(y>ddsd.dwHeight-1)
		return;

	int i=(y*ddsd.lPitch+x*(ddsd.ddpfPixelFormat.dwRGBBitCount>>3));

	((uint8_t *)ddsd.lpSurface)[i+0]=(unsigned char)(c[2]*255.0f)&0xFF;
	((uint8_t *)ddsd.lpSurface)[i+1]=(unsigned char)(c[1]*255.0f)&0xFF;
	((uint8_t *)ddsd.lpSurface)[i+2]=(unsigned char)(c[0]*255.0f)&0xFF;
}

void line(DDSURFACEDESC2 ddsd, uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, float c[3])
{
	int32_t dx=abs(x1-x0), sx=x0<x1?1:-1;
	int32_t dy=abs(y1-y0), sy=y0<y1?1:-1;

	int32_t err=(dx>dy?dx:-dy)/2;

	while(point(ddsd, x0, y0, c), x0!=x1||y0!=y1)
	{
		int32_t e2=err;

		if(e2>-dx)
		{
			err-=dy;
			x0+=sx;
		}

		if(e2<dy)
		{
			err+=dx;
			y0+=sy;
		}
	}
}

void hline(DDSURFACEDESC2 ddsd, uint32_t x0, uint32_t x1, uint32_t y, float c[3])
{
	uint32_t minX=x0<x1?x0:x1;
	uint32_t maxX=x0<x1?x1:x0;

	for(uint32_t i=minX;i<=maxX;i++)
		point(ddsd, i, y, c);
}

void vline(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y0, uint32_t y1, float c[3])
{
	uint32_t minY=y0<y1?y0:y1;
	uint32_t maxY=y0<y1?y1:y0;

	for(uint32_t i=minY;i<=maxY;i++)
		point(ddsd, x, i, c);
}

void rect(DDSURFACEDESC2 ddsd, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, float c[3])
{
	vline(ddsd, x1, y1, y2, c);
	vline(ddsd, x2, y1, y2, c);
	hline(ddsd, x1, x2, y1, c);
	hline(ddsd, x1, x2, y2, c);
}

void fillrect(DDSURFACEDESC2 ddsd, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, float c[3])
{
	uint32_t minX=x1<x2?x1:x2;
	uint32_t maxX=x1<x2?x2:x1;

	for(uint32_t i=minX;i<=maxX;i++)
		vline(ddsd, i, y1, y2, c);
}

void roundedrect(DDSURFACEDESC2 ddsd, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t r, float c[3])
{
	float f=1.0f-(float)r;
	float ddF_x=0.0f;
	float ddF_y=-2.0f*r;
	uint32_t x=0, y=r;

	line(ddsd, x1+r, y1, x2-r, y1, c);
	line(ddsd, x1+r, y2, x2-r, y2, c);
	line(ddsd, x1, y1+r, x1, y2-r, c);
	line(ddsd, x2, y1+r, x2, y2-r, c);
	uint32_t cx1=x1+r;
	uint32_t cx2=x2-r;
	uint32_t cy1=y1+r;
	uint32_t cy2=y2-r;

	while(x<y)
	{
		if(f>=0.0f)
		{
			y--;
			ddF_y+=2.0f;
			f+=ddF_y;
		}
		x++;
		ddF_x+=2.0f;
		f+=ddF_x+1.0f;

		point(ddsd, cx2+x, cy2+y, c);
		point(ddsd, cx1-x, cy2+y, c);
		point(ddsd, cx2+x, cy1-y, c);
		point(ddsd, cx1-x, cy1-y, c);
		point(ddsd, cx2+y, cy2+x, c);
		point(ddsd, cx1-y, cy2+x, c);
		point(ddsd, cx2+y, cy1-x, c);
		point(ddsd, cx1-y, cy1-x, c);
	}
}

void fillroundedrect(DDSURFACEDESC2 ddsd, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t r, float c[3])
{
	float f=1.0f-(float)r;
	float ddF_x=0.0f;
	float ddF_y=-2.0f*r;
	uint32_t x=0, y=r;

	fillrect(ddsd, x1+r, y1, x2-r, y2, c);

	uint32_t cx1=x1+r;
	uint32_t cx2=x2-r;
	uint32_t cy1=y1+r;
	uint32_t cy2=y2-r;

	while(x<y)
	{
		if(f>=0.0f)
		{
			y--;
			ddF_y+=2.0f;
			f+=ddF_y;
		}

		x++;
		ddF_x+=2.0f;
		f+=ddF_x+1.0f;

		line(ddsd, cx1-x, cy1-y, cx1-x, cy2+y, c);
		line(ddsd, cx1-y, cy1-x, cx1-y, cy2+x, c);
		line(ddsd, cx2+x, cy1-y, cx2+x, cy2+y, c);
		line(ddsd, cx2+y, cy1-x, cx2+y, cy2+x, c);
	}
}

void circle(DDSURFACEDESC2 ddsd, uint32_t x, uint32_t y, uint32_t r, float c[3])
{
	int32_t d;
	uint32_t curx, cury;

	if(!r)
		return;

	d=3-(r<<1);
	curx=0;
	cury=r;

	while(curx<=cury)
	{
		point(ddsd, x+curx, y-cury, c);
		point(ddsd, x-curx, y-cury, c);
		point(ddsd, x+cury, y-curx, c);
		point(ddsd, x-cury, y-curx, c);
		point(ddsd, x+curx, y+cury, c);
		point(ddsd, x-curx, y+cury, c);
		point(ddsd, x+cury, y+curx, c);
		point(ddsd, x-cury, y+curx, c);

		if(d<0)
			d+=(curx<<2)+6;
		else
		{
			d+=((curx-cury)<<2)+10;
			cury--;
		}

		curx++;
	}
}

void fillcircle(DDSURFACEDESC2 ddsd, uint32_t x0, uint32_t y0, uint32_t r, float c[3])
{
	int32_t x=r, y=0;
	int32_t xChange=1-(r<<1), yChange=0;
	int32_t radiusError=0;

	while(x>=y)
	{
		for(uint32_t i=x0-x; i<=x0+x; i++)
		{
			point(ddsd, i, y0+y, c);
			point(ddsd, i, y0-y, c);
		}
		for(uint32_t i=x0-y; i<=x0+y; i++)
		{
			point(ddsd, i, y0+x, c);
			point(ddsd, i, y0-x, c);
		}

		y++;
		radiusError+=yChange;
		yChange+=2;
		if(((radiusError<<1)+xChange)>0)
		{
			x--;
			radiusError+=xChange;
			xChange+=2;
		}
	}
}

vec2 SpherePointCollision(vec2 Point, vec2 Collider, float Radius)
{
	vec2 SphereDistance=Vec2_Subv(Point, Collider);

	if(Vec2_Dot(SphereDistance, SphereDistance)<Radius*Radius)
	{
		Vec2_Normalize(&SphereDistance);

		return Vec2(Collider.x+SphereDistance.x*Radius, Collider.y+SphereDistance.y*Radius);
	}

	return Point;
}

void KeepInsideView(Point_t *Point, int windowWidth, int windowHeight)
{
	if(Point->Position.x>windowWidth)
	{
		Point->Position.x=(float)windowWidth;
		Point->PrevPosition.x=Point->Position.x;
	}
	else if(Point->Position.x<0.0f)
	{
		Point->Position.x=0.0f;
		Point->PrevPosition.x=Point->Position.x;
	}

	if(Point->Position.y>windowHeight)
	{
		Point->Position.y=(float)windowHeight;
		Point->PrevPosition.y=Point->Position.y;
	}
	else if(Point->Position.y<0.0f)
	{
		Point->Position.y=0.0f;
		Point->PrevPosition.y=Point->Position.y;
	}
}

bool SphereSphereIntersect(vec2 PositionA, float RadiusA, vec2 PositionB, float RadiusB)
{
	float RadSum=RadiusA+RadiusB;
	return Vec2_Dot(PositionA, PositionB)<=RadSum*RadSum;
}

float Message1Time=0.0f;
float Message2Time=0.0f;
float BargraphValue=0.0f;

uint32_t CheckboxID=UINT32_MAX;
uint32_t BargraphID=UINT32_MAX;
uint32_t BargraphROID=UINT32_MAX;
uint32_t RedID=UINT32_MAX;
uint32_t GreenID=UINT32_MAX;
uint32_t BlueID=UINT32_MAX;

void Render(void)
{
	DDSURFACEDESC2 ddsd;
	HRESULT ret=DDERR_WASSTILLDRAWING;

	// Update the first point's position to the mouse movement
	if(MouseClicked)
		Points[1].Position=Vec2(X, Y);

	VerletIntegration(4, Points, 6, Sticks, (float)fTimeStep, 10);

	vec2 Collider={ (float)Width/2, (float)Height };
	float Radius=(float)Height/6;

	for(uint32_t i=0;i<4;i++)
	{
		Points[i].Position=SpherePointCollision(Points[i].Position, Collider, Radius);
		KeepInsideView(&Points[i], Width, Height);
	}

	Clear(lpDDSBack);

	memset(&ddsd, 0, sizeof(DDSURFACEDESC2));
	ddsd.dwSize=sizeof(ddsd);

	while(ret==DDERR_WASSTILLDRAWING)
		ret=IDirectDrawSurface7_Lock(lpDDSBack, NULL, &ddsd, 0, NULL);

	BargraphValue=UI_GetBarGraphValue(&UI, BargraphID);

	Font_Print(ddsd, 0, 0,
			   "%s\n%s\nCheckbox: %s\nBargraph: %0.5f",
			   Message1Time>0.0f?"Button 1 clicked~!":"",
			   Message2Time>0.0f?"Button 2 clicked~!":"",
			   UI_GetCheckBoxValue(&UI, CheckboxID)?"true":"false",
			   BargraphValue
	);

	if(Message1Time>0.0f)
		Message1Time-=(float)fTimeStep;

	if(Message2Time>0.0f)
		Message2Time-=(float)fTimeStep;

	UI_UpdateBarGraphValue(&UI, BargraphROID, BargraphValue);

	vec3 Color=Vec3(
		UI_GetBarGraphValue(&UI, RedID),
		UI_GetBarGraphValue(&UI, GreenID),
		UI_GetBarGraphValue(&UI, BlueID)
	);

	UI_UpdateBarGraphColor(&UI, BargraphID, Color);
	UI_UpdateBarGraphColor(&UI, BargraphROID, Color);

	circle(ddsd, (uint32_t)Collider.x, (uint32_t)Collider.y, (uint32_t)Radius, (float[])
	{
		1.0f, 1.0f, 1.0f
	});

	// Draw sticks as white lines
	for(uint32_t i=0;i<6;i++)
	{
		line(ddsd,
			 (int)Sticks[i].PointA->Position.x, (int)Sticks[i].PointA->Position.y,
			 (int)Sticks[i].PointB->Position.x, (int)Sticks[i].PointB->Position.y,
			 (float[]) { 1.0f, 1.0f, 1.0f }
		);
	}

	UI_Draw(&UI, ddsd);

	IDirectDrawSurface7_Unlock(lpDDSBack, NULL);
}

void SetStick(Stick_t *Stick, Point_t *PointA, Point_t *PointB)
{
	Stick->PointA=PointA;
	Stick->PointB=PointB;
	Stick->Length=Vec2_Distance(Stick->PointA->Position, Stick->PointB->Position);
}

void SetPoint(Point_t *Point, float x, float y, bool Locked)
{
	Point->Position=Vec2(x, y);
	Point->PrevPosition=Vec2(x, y);
	Point->Locked=Locked;
}

void Callback1(void *arg)
{
	UI_UpdateCheckBoxTitleText(&UI, CheckboxID, ":D");
	Message1Time=2.0f;
}

void Callback2(void *arg)
{
	UI_UpdateCheckBoxTitleText(&UI, CheckboxID, ":(");
	Message2Time=2.0f;
}

void CallbackExit(void *arg)
{
	Done=true;
}

int Init(void)
{
	float x=(float)Width/2.0f;
	float y=0.0f;

	SetPoint(&Points[0], x-10.0f, y-10.0f, false);
	SetPoint(&Points[1], x+10.0f, y-10.0f, false);
	SetPoint(&Points[2], x+10.0f, y+10.0f, false);
	SetPoint(&Points[3], x-10.0f, y+10.0f, false);

	X=Points[0].Position.x;
	Y=Points[0].Position.y;

	SetStick(&Sticks[0], &Points[0], &Points[1]);
	SetStick(&Sticks[1], &Points[1], &Points[2]);
	SetStick(&Sticks[2], &Points[2], &Points[3]);
	SetStick(&Sticks[3], &Points[3], &Points[0]);
	SetStick(&Sticks[4], &Points[0], &Points[2]);
	SetStick(&Sticks[5], &Points[1], &Points[3]);

	UI_Init(&UI, Vec2b(0.0f), Vec2((float)Width, (float)Height));

	UI_AddButton(&UI,
				 Vec2((float)Width/2.0f, (float)Height/4.0f),
				 Vec2(100.0f, 50.0f),
				 Vec3(0.25f, 0.25f, 0.25f),
				 "Test button 1", Callback1
	);
	UI_AddButton(&UI,
				 Vec2((float)Width/2.0f, (float)Height/4.0f+75.0f),
				 Vec2(100.0f, 50.0f),
				 Vec3(0.25f, 0.25f, 0.25f),
				 "Test button 2", Callback2
	);
	UI_AddButton(&UI,
				 Vec2(10.0f, (float)Height-60.0f),
				 Vec2(100.0f, 50.0f),
				 Vec3(0.25f, 0.25f, 0.25f),
				 "Exit", CallbackExit
	);

	CheckboxID=UI_AddCheckBox(&UI,
							  Vec2((float)Width/2.0f+10.0f, (float)Height/4.0f+75.0f+75.0f),
							  10.0f,
							  Vec3(1.0f, 1.0f, 1.0f),
							  "Test checkbox 1", true
	);

	BargraphID=UI_AddBarGraph(&UI,
							  Vec2((float)Width/2.0f, (float)Height/4.0f+75.0f+75.0f+25.0f),
							  Vec2(200.0f, 25.0f),
							  Vec3(1.0f, 1.0f, 1.0f),
							  "Click me, I can change!",
							  false,
							  0.0f, 1.0f, 1.0f
	);
	BargraphROID=UI_AddBarGraph(&UI,
								Vec2((float)Width/2.0f, (float)Height/4.0f+75.0f+75.0f+25.0f+25.0f),
								Vec2(200.0f, 25.0f),
								Vec3(1.0f, 1.0f, 1.0f),
								"I can't! :(",
								true,
								-1.0f, 1.0f, 0.0f
	);

	RedID=UI_AddBarGraph(&UI,
						 Vec2((float)10.0f, (float)Height/4.0f),
						 Vec2(200.0f, 25.0f),
						 Vec3(1.0f, 0.0f, 0.0f),
						 "Red value",
						 false,
						 0.0f, 1.0f, 1.0f
	);
	GreenID=UI_AddBarGraph(&UI,
						   Vec2((float)10.0f, (float)Height/4.0f+25.0f),
						   Vec2(200.0f, 25.0f),
						   Vec3(0.0f, 1.0f, 0.0f),
						   "Green value",
						   false,
						   0.0f, 1.0f, 1.0f
	);
	BlueID=UI_AddBarGraph(&UI,
						  Vec2((float)10.0f, (float)Height/4.0f+50.0f),
						  Vec2(200.0f, 25.0f),
						  Vec3(0.0f, 0.0f, 1.0f),
						  "Blue value",
						  false,
						  0.0f, 1.0f, 1.0f
	);

	return 1;
}

BOOL Create(void)
{
	DDSURFACEDESC2 ddsd;
	LPDIRECTDRAWCLIPPER lpClipper=NULL;

	if(DirectDrawCreateEx(NULL, &lpDD, &IID_IDirectDraw7, NULL)!=DD_OK)
	{
		MessageBox(hWnd, "DirectDrawCreateEx failed.", "Error", MB_OK);
		return FALSE;
	}

	if(IDirectDraw7_SetCooperativeLevel(lpDD, hWnd, DDSCL_NORMAL)!=DD_OK)
	{
		MessageBox(hWnd, "IDirectDraw7_SetCooperativeLevel failed.", "Error", MB_OK);
		return FALSE;
	}

	memset(&ddsd, 0, sizeof(ddsd));
	ddsd.dwSize=sizeof(ddsd);
	ddsd.dwFlags=DDSD_CAPS;
	ddsd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;

	if(IDirectDraw7_CreateSurface(lpDD, &ddsd, &lpDDSFront, NULL)!=DD_OK)
	{
		MessageBox(hWnd, "IDirectDraw7_CreateSurface (Front) failed.", "Error", MB_OK);
		return FALSE;
	}

	ddsd.dwFlags=DDSD_WIDTH|DDSD_HEIGHT|DDSD_CAPS;
	ddsd.dwWidth=Width;
	ddsd.dwHeight=Height;
	ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN;

	if(IDirectDraw7_CreateSurface(lpDD, &ddsd, &lpDDSBack, NULL)!=DD_OK)
	{
		MessageBox(hWnd, "IDirectDraw7_CreateSurface (Back) failed.", "Error", MB_OK);
		return FALSE;
	}

	if(IDirectDraw7_CreateClipper(lpDD, 0, &lpClipper, NULL)!=DD_OK)
	{
		MessageBox(hWnd, "IDirectDraw7_CreateClipper failed.", "Error", MB_OK);
		return FALSE;
	}

	if(IDirectDrawClipper_SetHWnd(lpClipper, 0, hWnd)!=DD_OK)
	{
		MessageBox(hWnd, "IDirectDrawClipper_SetHWnd failed.", "Error", MB_OK);
		return FALSE;
	}

	if(IDirectDrawSurface_SetClipper(lpDDSFront, lpClipper)!=DD_OK)
	{
		MessageBox(hWnd, "IDirectDrawSurface_SetClipper failed.", "Error", MB_OK);
		return FALSE;
	}

	if(lpClipper!=NULL)
	{
		IDirectDrawClipper_Release(lpClipper);
		lpClipper=NULL;
	}

	return TRUE;
}

void Destroy(void)
{
	if(lpDDSBack!=NULL)
	{
		IDirectDrawSurface7_Release(lpDDSBack);
		lpDDSBack=NULL;
	}

	if(lpDDSFront!=NULL)
	{
		IDirectDrawSurface7_Release(lpDDSFront);
		lpDDSFront=NULL;
	}

	if(lpDD!=NULL)
	{
		IDirectDraw7_Release(lpDD);
		lpDD=NULL;
	}
}

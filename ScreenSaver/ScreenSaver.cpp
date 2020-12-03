//Barebones screensaver example from the-generalist.com
//http://www.cityintherain.com/howtoscr.html
#include <windows.h>
#include <scrnsave.h>
#include <strsafe.h>
#include <winuser.h>

#include <queue>
#include <cmath>
#include <ctime>
#include "GL/gl.h"
#include "GL/glu.h"
#include "resource.h"
#pragma comment (lib, "scrnsavw.lib")
#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "opengl32.lib")

#define  DLG_SCRNSAVECONFIGURE 2003

#define M_PI 3.14159265358979323846

//define a Windows timer
#define TIMER 1
enum RGBColor_e{eRGBColor_R,eRGBColor_G,eRGBColor_B};

#define MaxDepth 32 //Far too big for any mortal computer
#define FramesPerSecond 24.
#define ColorAdjustment 0.15

extern "C" {
	_declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
}

// I use these variables to track CPU usage and tune the recursion depth accordingly
static double accumulatedSeconds;
static double accumulatedFrames;
static double framesBetweenDepthChanges;
static unsigned int targetDepth = 14;
static unsigned int viewsCount;
static double totalPixelCount;

typedef double Rotator[2];
float alphaForDepth[MaxDepth];

typedef struct {
	double x;
	double y;
} Point;

struct Size{
	double width;
	double height;
	Size():width(.0),height(.0){}
	Size(double w,double h):width(w),height(h){}
};

struct Rect{
	Point origin;
	Size size;
	Rect():origin(),size(){};
};

static double transition(double now, double transitionSeconds, ...){
	va_list ap;

	double totalSeconds = 0;
	va_start(ap, transitionSeconds);
	while (1) {
		double seconds = va_arg(ap, double);
		if (seconds == 0)
			break;
		totalSeconds += seconds + transitionSeconds; 
		va_arg(ap, double);
	}
	va_end(ap);

	double modnow = fmod(now, totalSeconds);
	double level0;
	va_start(ap, transitionSeconds);
	va_arg(ap, double);
	level0 = va_arg(ap, double);
	va_end(ap);

	double startLevel, endLevel;
	va_start(ap, transitionSeconds);
	while(1){
		double seconds = va_arg(ap,double);
		startLevel = va_arg(ap,double);

		if (modnow < seconds) {			
			endLevel = startLevel;
			break;
		}

		modnow -=seconds;
		if (modnow <= transitionSeconds) {
			seconds = va_arg(ap, double);
			endLevel = (seconds == 0) ? level0 : va_arg(ap,double);
			break;
		}
		modnow -= transitionSeconds;
	}
	va_end(ap);

	if (startLevel == endLevel)
		return startLevel;
	else
	{
		return endLevel + (startLevel - endLevel) * 
			(cos(M_PI * modnow / transitionSeconds) + 1) * .5;
	}
}

/** Set `rotator` to the top half of the rotation matrix 
for a rotation of `rotation` (1 = a full revolution), 
scaled by `scale`. */
static void initRotator(Rotator rotator, double rotation, double scale)
{
	double radians = 2 * M_PI * rotation;
	rotator[0] = cos(radians) * scale;
	rotator[1] = sin(radians) * scale;
}

/** Apply `rotator` to the vector described by `s0`, returning a new vector. */
static inline Size rotateSize(Rotator rotator, Size s0){
   return Size( 
	   double(s0.width * rotator[0] - s0.height * rotator[1]),
	   double(s0.width * rotator[1] + s0.height * rotator[0])
   );
}

/*** Return the number of seconds since midnight, localtime. */

static double getNow(){
	SYSTEMTIME st;
	GetLocalTime(&st);	
	double now = ((st.wHour * 60) + st.wMinute) * 60 + 
		st.wSecond + (st.wMilliseconds/1000.);
	return now;
}

static double getRotation(double now, double period) {
	return .25 - fmod(now, period) / period;
}

static inline double midX(Rect rect){
	return rect.size.width / 2.;
}

static inline double midY(Rect rect){
	return rect.size.height / 2.;
}

static Rect getRootAndRotators(Rect bounds, Rotator secondRotator, Rotator minuteRotator)
{
	double now = getNow();
	double hourRotation = getRotation(now, 12 * 60 * 60);
	double minuteRotation = getRotation(now, 60 * 60);
	double secondRotation = getRotation(now, 60);

	double scale = transition(now, 12.,
		61., 1.,
		61., 0.793900525984099737375852819636, // cube root of 1/2
		0.);

	initRotator(secondRotator, secondRotation - hourRotation, -scale);
	initRotator(minuteRotator, minuteRotation - hourRotation, -scale);

	Rotator hourRotator;
	initRotator(hourRotator, hourRotation, 1);
	double rootSize = min(bounds.size.width, bounds.size.height)/6.;
	Rect root;
	Size rootSizeStruct(-rootSize, 0);
	root.size = rotateSize(hourRotator, rootSizeStruct);
	root.origin.x = midX(bounds) - root.size.width;
	root.origin.y = midY(bounds) - root.size.height;
	return root;
}

static void drawBranch(Rect* line, Rotator &r0, Rotator &r1, 
					   unsigned int depth, unsigned int depthLeft, double* rgb)
{
	Point p2 = {
		line->origin.x + line->size.width,
		line->origin.y + line->size.height
	};

	double r=r0[1]>0?r0[1]:-r0[1];
	double R=r1[1]>0?r1[1]:-r1[1];
	double R_=R-r;
	if(R_<0)R_=-R_;
	
	if(depth>1)
	glColor4f(
		/*rand()%2?*/rgb[eRGBColor_R]*(rand()%255)/255,//*R_,//:rgb[eRGBColor_R]+(r1[1]+0.1),
		/*rand()%2?*/rgb[eRGBColor_G]*(rand()%255)/255,//*R_,//:rgb[eRGBColor_G]+(r1[1]+0.1),
		/*rand()%2?*/rgb[eRGBColor_B]*(rand()%255)/255,//*R_,//:rgb[eRGBColor_B]+(r1[1]+0.1),
		alphaForDepth[depth]);
	else
		glColor4f(rgb[eRGBColor_R],rgb[eRGBColor_G],rgb[eRGBColor_B],alphaForDepth[depth]);
	if (depth == 0) {
		glVertex2f(
			line->origin.x + line->size.width * .5,
			line->origin.y + line->size.height * .5
			);
	} else
		glVertex2f(line->origin.x, line->origin.y);
	glVertex2f(p2.x, p2.y);

	if (depthLeft >= 1)
	{
		const int dd1(depth?depth:1);
		const int dd2(depthLeft?depthLeft:1);
		double newRGB[3] = { depth>=1?double(rand()%255)/255.:1.,  //* rgb[eRGBColor_R],///*-((((double) rand() / (RAND_MAX))*/ + 1))/*100.*(depth%(eRGBColor_R+1))*/,
							 depth>=1?double(rand()%255)/255.:1.,  //* rgb[eRGBColor_G],///*-((((double) rand() / (RAND_MAX))*/ + 1))/*100.*(depth%(eRGBColor_G+1))*/,
							 depth>=1?double(rand()%255)/255.:1.,};// * rgb[eRGBColor_B]};///*-((((double) rand() / (RAND_MAX))*/ + 1))/*100.*(depth%(eRGBColor_B+1))*/ };

		Rect newLine;
		newLine.origin = p2;
		newLine.size = rotateSize(r0, line->size);

	
		//const double RGBStep1[3] = {
		//	(!(depth%dd2+rgb[eRGBColor_R]))?0:((eRGBColor_R+1)*0.321),
		//	(!(depth%dd2+rgb[eRGBColor_G]))?0:((eRGBColor_G+1)*0.321),
		//	(!(depth%dd2+rgb[eRGBColor_B]))?0:((eRGBColor_B+1)*0.321)};

		//newRGB[eRGBColor_R] = /*RGBStep1[eRGBColor_R]*ColorAdjustment +*/ rgb[eRGBColor_R];
		//newRGB[eRGBColor_G] = /*RGBStep1[eRGBColor_G]*ColorAdjustment +*/ rgb[eRGBColor_G];
		//newRGB[eRGBColor_B] = /*RGBStep1[eRGBColor_B]*ColorAdjustment +*/ rgb[eRGBColor_B];

		const double RGBStep2[3] = {
			(!(depthLeft%dd1+rgb[eRGBColor_R]))?0:((eRGBColor_R+1)*0.1),
			(!(depthLeft%dd1+rgb[eRGBColor_G]))?0:((eRGBColor_G+1)*0.1),
			(!(depthLeft%dd1+rgb[eRGBColor_B]))?0:((eRGBColor_B+1)*0.1)};

		if(depth>=1)
		{
			newRGB[rand()%3] = RGBStep2[eRGBColor_R]*ColorAdjustment + rgb[eRGBColor_R];
			newRGB[rand()%3] = RGBStep2[eRGBColor_G]*ColorAdjustment + rgb[eRGBColor_G];
			newRGB[rand()%3] = RGBStep2[eRGBColor_B]*ColorAdjustment + rgb[eRGBColor_B];
		}

		drawBranch(&newLine, r0, r1, depth + 1, depthLeft - 1, newRGB);

		newLine.size = rotateSize(r1, line->size);

		drawBranch(&newLine, r0, r1, depth + 1, depthLeft - 1, newRGB);
	}
}


static void InitGL(HWND hWnd, HDC & hDC, HGLRC & hRC)
{
  
  PIXELFORMATDESCRIPTOR pfd;
  ZeroMemory( &pfd, sizeof pfd );
  pfd.nSize = sizeof pfd;
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = 24;
  
  hDC = GetDC( hWnd );
  
  int i = ChoosePixelFormat( hDC, &pfd );  
  SetPixelFormat( hDC, i, &pfd );

  hRC = wglCreateContext( hDC );
  wglMakeCurrent( hDC, hRC );

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_LIGHTING);
  glDisable(GL_DITHER);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_ALPHA_TEST);
  glAlphaFunc(GL_GREATER, 1./255);

  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glEnable(GL_LINE_SMOOTH);

}
 
static void CloseGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
  wglMakeCurrent( NULL, NULL );
  wglDeleteContext( hRC );
  ReleaseDC( hWnd, hDC );
}

double Log2(double n)
{
	return log(n)/log(2);
}

int init_srand()
{
	srand(time_t());
	return 1;
}
//Required Function
LRESULT WINAPI ScreenSaverProc(HWND hWnd, UINT message, 
                               WPARAM wParam, LPARAM lParam)
{
	//static int srander=init_srand();
	srand(0&time(0));
	static HDC hDC;
	static HGLRC hRC;
	static RECT rect;
	static Point origin;
	static Size windowSize;
	static Rect windowRect;
	static double rootColor[3];

	rootColor[eRGBColor_R]=1.0;
	rootColor[eRGBColor_G]=1.0;
	rootColor[eRGBColor_B]=1.0;

	switch(message) {
		case WM_CREATE:
			// get window dimensions
			GetClientRect(hWnd, &rect);
			windowSize.height = rect.bottom;
			windowSize.width = rect.right;
			origin.x = windowSize.height / 2.;
			origin.y = windowSize.width / 2.;
			windowRect.origin = origin;
			windowRect.size = windowSize;

			//get configuration from registry if applicable

			//set up OpenGL
			InitGL(hWnd, hDC, hRC);

			//Initialize perspective, viewpoint, and
			//any objects you wish to animate here
			for (int i = 0; i < 3; ++i)
				alphaForDepth[i] = 1;
			for (int i = 3; i < MaxDepth; ++i)
				//alphaForDepth[i] = pow(1.0, -i)/i;
				alphaForDepth[i] = pow(i, -.87654321);///i;

			//create timer that ticks every 10 ms
			//SetTimer( hWnd, TIMER, 10, NULL );
			SetTimer( hWnd, TIMER, 15, NULL );
			//SetTimer( hWnd, TIMER, 1000, NULL );
			return(0);

		case WM_TIMER:{
			//Put your drawing code here
			//This is called every 10 ms
			//time_t startTime = time(NULL);

			Rect bounds = windowRect;


			glViewport(0, 0, bounds.size.width, bounds.size.height);
			glMatrixMode(GL_PROJECTION);
			glLoadIdentity();
			glOrtho(0, bounds.size.width, 0, bounds.size.height, 0, 1);

			glClearColor(0., 0., 0., 1.);
			glClear(GL_COLOR_BUFFER_BIT);
		
			Rotator r0;
			Rotator r1;
			Rect root = getRootAndRotators(bounds, r0, r1);

			//if(1||time(0)%2)
			//{
				glLineWidth(1.5);
				glBegin(GL_LINES);
			//}else{
			//	glPointSize(1.5);
			//	glBegin(GL_POINTS);
			//}

			drawBranch(&root, r0, r1, 0, targetDepth, rootColor);
			glEnd();

			glFlush();
			SwapBuffers(hDC);
		
			//accumulatedSeconds += time(NULL) - startTime;
			//++accumulatedFrames;
			return(0);
		}
		case WM_DESTROY:
			//Put you cleanup code here.
			//This will be called on close.
			KillTimer(hWnd, TIMER);
			//delete any objects created during animation
			//and close down OpenGL nicely

			CloseGL(hWnd, hDC, hRC);
			return(0);
	}

	return DefScreenSaverProc(hWnd, message, wParam, lParam);
}

#define MINVEL  1                 // minimum redraw speed value     
#define MAXVEL  10                // maximum redraw speed value    
#define DEFVEL  5                 // default redraw speed value    

LONG    lSpeed = DEFVEL;          // redraw speed variable         
extern HINSTANCE hMainInstance;   // screen saver instance handle
TCHAR   szTemp[20];                // temporary array of characters
LPCWSTR szRedrawSpeed = L"Redraw Speed";   // .ini speed entry 

//Required Function
BOOL WINAPI ScreenSaverConfigureDialog(HWND hDlg, UINT message, 
									   WPARAM wParam, LPARAM lParam)
{
	static HWND hSpeed;   // handle to speed scroll bar 
	static HWND hOK;      // handle to OK push button  
	static HRESULT hr;

	switch (message)
	{
	case WM_INITDIALOG:

		// Retrieve the application name from the .rc file.  
		LoadString(hMainInstance, idsAppName, szAppName,
			80 * sizeof(TCHAR));

		// Retrieve the .ini (or registry) file name. 
		LoadString(hMainInstance, idsIniFile, szIniFile,
			MAXFILELEN * sizeof(TCHAR));

		// TODO: Add error checking to verify LoadString success
		//       for both calls.

		// Retrieve any redraw speed data from the registry. 
		lSpeed = GetPrivateProfileInt(szAppName, szRedrawSpeed,
			DEFVEL, szIniFile);

		// If the initialization file does not contain an entry 
		// for this screen saver, use the default value. 
		if (lSpeed > MAXVEL || lSpeed < MINVEL)
			lSpeed = DEFVEL;

		// Initialize the redraw speed scroll bar control.
		hSpeed = GetDlgItem(hDlg, ID_SPEED);
		SetScrollRange(hSpeed, SB_CTL, MINVEL, MAXVEL, FALSE);
		SetScrollPos(hSpeed, SB_CTL, lSpeed, TRUE);

		// Retrieve a handle to the OK push button control.  
		hOK = GetDlgItem(hDlg, ID_OK);

		return TRUE;

	case WM_HSCROLL:

		// Process scroll bar input, adjusting the lSpeed 
		// value as appropriate. 
		switch (LOWORD(wParam))
		{
		case SB_PAGEUP:
			--lSpeed;
			break;

		case SB_LINEUP:
			--lSpeed;
			break;

		case SB_PAGEDOWN:
			++lSpeed;
			break;

		case SB_LINEDOWN:
			++lSpeed;
			break;

		case SB_THUMBPOSITION:
			lSpeed = HIWORD(wParam);
			break;

		case SB_BOTTOM:
			lSpeed = MINVEL;
			break;

		case SB_TOP:
			lSpeed = MAXVEL;
			break;

		case SB_THUMBTRACK:
		case SB_ENDSCROLL:
			return TRUE;
			break;
		}

		if ((int)lSpeed <= MINVEL)
			lSpeed = MINVEL;
		if ((int)lSpeed >= MAXVEL)
			lSpeed = MAXVEL;

		SetScrollPos((HWND)lParam, SB_CTL, lSpeed, TRUE);
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_OK:

			// Write the current redraw speed variable to
			// the .ini file. 
			hr = StringCchPrintf(szTemp, 20, L"%ld", lSpeed);
			if (SUCCEEDED(hr))
				WritePrivateProfileString(szAppName, szRedrawSpeed,
					szTemp, szIniFile);

		case ID_CANCEL:
			EndDialog(hDlg, LOWORD(wParam) == ID_OK);

			return TRUE;
		}
	}
	return FALSE;
}

//Required Function by SCRNSAVE.LIB
BOOL WINAPI RegisterDialogClasses(HANDLE hInst)
{
	return(TRUE);
}

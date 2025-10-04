// NoSnooze.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "NoSnooze.h"
#include <shellapi.h> //Needed for tray icon

//#include <commctrl.h> //Needed for some of they syling code suggested. Omitted for now!
//#pragma comment(lib, "comctl32.lib")


#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
HWND g_hWnd, g_btnStart, g_btnStop;				//AJG - Window & Buttons
NOTIFYICONDATA g_nid = { 0 };
WCHAR szTitle[MAX_LOADSTRING];					// The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
HICON g_hIconStarted = nullptr;					//AJG - Icon to use when no snooze starts
HICON g_hIconStopped = nullptr;					//AJG - Icon to use when no snooze stops
bool s_bStarted = false;						//AJG - No Snooze current state

//System Single Instance Mutex
const WCHAR* g_szMutexName = L"Local\\NoSnoozeAppMutex_v1.0";	//Local\ means local to current session (RDP, Fast User Switching, etc)
HANDLE g_hMutex = nullptr;

#define ID_BTN_START 1001
#define ID_BTN_STOP  1002
#define ID_TRAY_ICON 2001
#define WM_TRAYICON  (WM_USER + 1)
#define ID_TRAY_EXIT 3001
#define ID_IGNORE 9991

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
//LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);  //Don't need to fwd declare if we just define in correct order!
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);

void AddTrayIcon(HWND hWnd)	//AJG - Create a tray icon
{
	g_nid.cbSize = sizeof(NOTIFYICONDATA);
	g_nid.hWnd = hWnd;
	g_nid.uID = ID_TRAY_ICON;
	g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	g_nid.uCallbackMessage = WM_TRAYICON;
	g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcscpy_s(g_nid.szTip, L"No Snooze!");
	Shell_NotifyIcon(NIM_ADD, &g_nid);
}

void UpdateTrayIcon()	//AJG - Update state of tray icon
{
	if(g_hIconStarted && g_hIconStopped)
	{
		g_nid.hIcon = s_bStarted ? g_hIconStarted : g_hIconStopped;
	}

	if(s_bStarted)
		wcscpy_s(g_nid.szTip, L"No Snooze - Enabled");
	else
		wcscpy_s(g_nid.szTip, L"No Snooze - Disabled");
	Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

void RemoveTrayIcon()
{
	Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

void ShowTrayMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);
	HMENU hMenu = CreatePopupMenu();

	//Add menu items
	UINT pos = 0u;
	InsertMenu(hMenu, pos++, MF_BYPOSITION | MF_STRING | (s_bStarted ? MF_GRAYED | MF_DISABLED : MF_ENABLED), ID_BTN_START, L"Start");
	InsertMenu(hMenu, pos++, MF_BYPOSITION | MF_STRING | (s_bStarted ? MF_ENABLED : MF_GRAYED | MF_DISABLED), ID_BTN_STOP, L"Stop");
	InsertMenu(hMenu, pos++, MF_BYPOSITION | MF_SEPARATOR, ID_IGNORE, nullptr);
	InsertMenu(hMenu, pos++, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Exit");

	// Required to make the popup menu work correctly
	SetForegroundWindow(hWnd);
	TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
	DestroyMenu(hMenu);
}

bool EnsureSingleInstance()
{
	g_hMutex = CreateMutex(NULL, TRUE, g_szMutexName);
	if(!g_hMutex) return false;		//Failed to create mutex

	if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		//Another instance is already running
		HWND hOtherWnd = FindWindow(szWindowClass, nullptr); //szTitle);
		if(hOtherWnd)
		{
			PostMessage(hOtherWnd, WM_COMMAND, WPARAM(ID_BTN_START), 0); //Tell other instance to start
			//PostMessage(hOtherWnd, WM_TRAYICON, 0, WM_LBUTTONDOWN); //Simulate a click on the tray icon to show the window
		}
		return false;
	}

	return true;
}

void ReleaseSingleInstance()
{
	if(g_hMutex)
	{
		ReleaseMutex(g_hMutex);
		CloseHandle(g_hMutex);
		g_hMutex = nullptr;
	}
}

void OnStartBtn(HWND hWnd)
{
	//MessageBox(hWnd, L"Start It clicked!", L"Starting...", MB_OK | MB_ICONINFORMATION);
	s_bStarted = true;
	SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);	//No sleep
	UpdateTrayIcon();
	EnableWindow(g_btnStart, FALSE);
	EnableWindow(g_btnStop, TRUE);
}

void OnStopBtn(HWND hWnd)
{
	//MessageBox(hWnd, L"Stop It clicked!", L"Stopping...", MB_OK | MB_ICONINFORMATION);
	s_bStarted = false;
	SetThreadExecutionState(ES_CONTINUOUS);	//Reset
	UpdateTrayIcon();
	EnableWindow(g_btnStart, TRUE);
	EnableWindow(g_btnStop, FALSE);
}

void InitApp(HWND hWnd)
{
	g_hIconStarted = LoadIcon(hInst, MAKEINTRESOURCE(IDI_STARTED));	//TODO: Replace with correct icons when available
	g_hIconStopped = LoadIcon(hInst, MAKEINTRESOURCE(IDI_STOPPED));
}

void ShutdownApp(HWND hWnd)
{
	OnStopBtn(hWnd);	//'Click' stop Btn - B/C we don't want to forget to reset thread priority. May not matter, but w/e
	if(g_hIconStarted) DestroyIcon(g_hIconStarted);	//Don't forget to destroy this? Is the if redundant?
	if(g_hIconStopped) DestroyIcon(g_hIconStopped);
	ReleaseSingleInstance();
}

void OnExit(HWND hWnd)
{
	ShutdownApp(hWnd);
	PostMessage(hWnd, WM_CLOSE, 0, 0);	//What's the better way to quit? PostMessage()? DestroyWindow()? PostQuitMessage(0)?
}

void OnExitFromXBtn(HWND hWnd)
{
	ShutdownApp(hWnd);
	DestroyWindow(hWnd);
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch(wmId)
		{
		case ID_BTN_START:
			OnStartBtn(hWnd);
			break;
		case ID_BTN_STOP:
			OnStopBtn(hWnd);
			break;
		case ID_TRAY_EXIT:
			OnExit(hWnd);
			break;
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			OnExitFromXBtn(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_TRAYICON:
	{
		switch (lParam)
		{
		case WM_LBUTTONDOWN:
			ShowWindow(hWnd, SW_RESTORE);
			ShowWindow(hWnd, SW_SHOW);
			SetForegroundWindow(hWnd);
			break;
		case WM_RBUTTONDOWN:
			ShowTrayMenu(hWnd);
			break;
		}
		break;
	}
	case WM_SIZE:
	{
		if(wParam == SIZE_MINIMIZED)
		{
			ShowWindow(hWnd, SW_HIDE);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		RemoveTrayIcon();
		ShutdownApp(hWnd);	//1-stop shp for shutdown
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.
	// Enable modern visual styles
	//INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_WIN95_CLASSES };	//AJG: Needs CommCtrl - Omitting for now
	//InitCommonControlsEx(&icex);

	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_NOSNOOZE, szWindowClass, MAX_LOADSTRING);
	if(!EnsureSingleInstance())
	{
		//Another instance is already running
		return FALSE;
	}
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if(!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NOSNOOZE));

	MSG msg;

	// Main message loop:
	while(GetMessage(&msg, nullptr, 0, 0))
	{
		if(!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NOSNOOZE));	//AJG: Can change icons here if we like
    wcex.hCursor		= LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= MAKEINTRESOURCEW(IDC_NOSNOOZE);
    wcex.lpszClassName	= szWindowClass;
    wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));	//AJG: Here too. Depends where the icon is used.

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	//HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);	//This makes a large window. Let's make it a bit smaller //AJG: TODO: Lookup WS_OVERLAPPEDWINDOW vs what I have.
	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 300, 120, nullptr, nullptr, hInstance, nullptr);	//Horizontal Stack
	//HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 300, 160, nullptr, nullptr, hInstance, nullptr);	//Vertical Stack

	if(!hWnd)
	{
		return FALSE;
	}

	//g_hWnd = hWnd; //Do we need to store this?

	//Create Buttons!
	//Horizontal Stack
	g_btnStart = CreateWindow(L"BUTTON", L"Start It", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 30, 20, 100, 30, hWnd, reinterpret_cast<HMENU>(ID_BTN_START), hInstance, NULL);
	g_btnStop = CreateWindow(L"BUTTON", L"Stop It", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 150, 20, 100, 30, hWnd, reinterpret_cast<HMENU>(ID_BTN_STOP), hInstance, NULL);

	//Vertical Stack
	//g_btnStart = CreateWindow(L"BUTTON", L"Start It", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 30, 0, 100, 30, hWnd, reinterpret_cast<HMENU>(ID_BTN_START), hInstance, NULL);
	//g_btnStop = CreateWindow(L"BUTTON", L"Stop It", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 30, 30, 100, 30, hWnd, reinterpret_cast<HMENU>(ID_BTN_STOP), hInstance, NULL);

	InitApp(hWnd);

	bool startMinimized = true;
	bool startDisabled = false;
	int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if(argv)
	{
		for(int i = 1; i < argc; ++i)
		{
			if(_wcsicmp(argv[i], L"--showWnd") == 0)
			{
				startMinimized = false;
			}
			if(_wcsicmp(argv[i], L"--startDisabled") == 0)
			{
				startDisabled = true;
			}
		}
		LocalFree(argv); //Don't forget to free!
	}

	ShowWindow(hWnd, startMinimized ? SW_HIDE : nCmdShow);
	UpdateWindow(hWnd);

	AddTrayIcon(hWnd);

	//Start app in requested or default start state!
	if(startDisabled)
		OnStopBtn(hWnd);
	else
		OnStartBtn(hWnd);

	return TRUE;
}
/*BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if(!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}*/

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
/*LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch(wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code that uses hdc here...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}*/

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch(message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if(LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

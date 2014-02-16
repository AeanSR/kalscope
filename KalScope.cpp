// KalScope.cpp : ����Ӧ�ó������ڵ㡣
//

#include "stdafx.h"
#include "KalScope.h"

#define MAX_LOADSTRING 100

// ȫ�ֱ���: 
ULONG_PTR           gdiplusToken;
HWND ghDlg;
HINSTANCE hInst;								// ��ǰʵ��
TCHAR szTitle[MAX_LOADSTRING];					// �������ı�
TCHAR szWindowClass[MAX_LOADSTRING];			// ����������

// �˴���ģ���а����ĺ�����ǰ������: 
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	Splash(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	GdiplusStartupInput gdiplusStartupInput;

	// Initialize GDI+.
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	srand((unsigned int)time(NULL));
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	ghDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_SPLASH), 0, Splash);
	ShowWindow(ghDlg, SW_SHOW);
	
	// TODO:  �ڴ˷��ô��롣
	MSG msg;
	HACCEL hAccelTable;

	// ��ʼ��ȫ���ַ���
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_KALSCOPE, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// ִ��Ӧ�ó����ʼ��: 
	if (!InitInstance (hInstance, nCmdShow))
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_KALSCOPE));
	
	// ����Ϣѭ��: 
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  ����:  MyRegisterClass()
//
//  Ŀ��:  ע�ᴰ���ࡣ
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_KALSCOPE));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_KALSCOPE);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   ����:  InitInstance(HINSTANCE, int)
//
//   Ŀ��:  ����ʵ�����������������
//
//   ע��: 
//
//        �ڴ˺����У�������ȫ�ֱ����б���ʵ�������
//        ��������ʾ�����򴰿ڡ�
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // ��ʵ������洢��ȫ�ֱ�����

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 450, 450, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

//
//  ����:  WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  Ŀ��:    ���������ڵ���Ϣ��
//
//  WM_COMMAND	- ����Ӧ�ó���˵�
//  WM_PAINT	- ����������
//  WM_DESTROY	- �����˳���Ϣ������
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;


	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// �����˵�ѡ��: 
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case ID_HELP_SPLASHSCREEN:
			ghDlg = CreateDialog(hInst, MAKEINTRESOURCE(IDD_SPLASH), 0, Splash);
			ShowWindow(ghDlg, SW_SHOW);
			break;
		case ID_CONTROL_YIELD:
			if (!eval_null()){
				MessageBoxA(0, "You could only yield before a game start.", "Yield", 0);
				break;
			}
			mainboard[7][7] = 1;
			paint_board(hWnd);
			break;
		case ID_CONTROL_RESTART:
			clear_board(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		Graphics *myGraphics;
		Pen *myPen;
		hdc = BeginPaint(hWnd, &ps);
		// TODO:  �ڴ���������ͼ����...
		myGraphics = new Graphics(hdc);
		myPen = new Pen(Color(255, 0, 0, 0), 1);
		myGraphics->DrawRectangle(myPen, Rect(400, 20, 20, 20));
		for (int i = 0; i < 375; i+= 25)
			myGraphics->DrawLine(myPen, 20, 20 + i, 370, 20 + i);
		for (int i = 0; i < 375; i += 25)
			myGraphics->DrawLine(myPen, 20 + i, 20, 20 + i, 370);
		delete myGraphics;
		delete myPen;
		EndPaint(hWnd, &ps); 
		paint_board(hWnd);
		break;
	case WM_DESTROY:
		GdiplusShutdown(gdiplusToken);
		PostQuitMessage(0);
		break;
	case WM_LBUTTONDOWN:
		board_clicked(hWnd, LOWORD(lParam), HIWORD(lParam));
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// �����ڡ������Ϣ�������
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDURL && HIWORD(wParam) == BN_CLICKED)
		{
			ShellExecute(NULL, TEXT("open"), TEXT("http://aean.sinaapp.com"), NULL, NULL, SW_SHOWNORMAL);
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		else if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;

	}
	return (INT_PTR)FALSE;
}

BLENDFUNCTION    m_Blend;
size_t splashcount=0;
// Splash��Ϣ�������
INT_PTR CALLBACK Splash(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
	{SetTimer(hDlg, 1, 1000, NULL);
		m_Blend.BlendOp = AC_SRC_OVER; //theonlyBlendOpdefinedinWindows2000
		m_Blend.BlendFlags = 0; //nothingelseisspecial...
		m_Blend.AlphaFormat = AC_SRC_ALPHA; //...
		m_Blend.SourceConstantAlpha = 255;//AC_SRC_ALPHA 
		Image *m_pImageBack;
		ImageFromIDResource(IDB_PNG1, L"PNG", m_pImageBack);
		//----����
		PAINTSTRUCT ps;
		HDC hdcTemp = BeginPaint(hDlg, &ps);
		HDC hMemDC = CreateCompatibleDC(hdcTemp);
		RECT rct;
		GetWindowRect(hDlg, &rct);
		HBITMAP hBitMap = CreateCompatibleBitmap(hdcTemp, rct.right - rct.left, rct.bottom - rct.top);
		SelectObject(hMemDC, hBitMap);
		HDC hdcScreen = GetDC(hDlg);

		POINT ptWinPos = { rct.left, rct.top };

		Graphics imageGraphics(hMemDC);
		Point points[] = { Point(0, 0),
			Point(400, 0),
			Point(0, 255) };

		// ���ò�δ���
		DWORD dwExStyle = GetWindowLong(hDlg, GWL_EXSTYLE);

		if ((dwExStyle & 0x80000) != 0x80000){
			SetWindowLong(hDlg, GWL_EXSTYLE, dwExStyle ^ 0x80000);
		}

		POINT    ptSrc = { 0, 0 };
		SIZE    sizeWindow = { rct.right - rct.left, rct.bottom - rct.top };

		// ���͸�������򴰿ڵĻ���
		imageGraphics.DrawImage(m_pImageBack, points, 3);
		UpdateLayeredWindow(hDlg, hdcScreen, &ptWinPos, &sizeWindow, hMemDC, &ptSrc, 255, &m_Blend, ULW_ALPHA);


		// �ͷſռ�
		imageGraphics.ReleaseHDC(hMemDC);
		DeleteObject(hBitMap);
		DeleteDC(hMemDC);
		hMemDC = NULL;
		hdcScreen = NULL;
		EndPaint(hDlg, &ps);
		return (INT_PTR)TRUE;
	}
	
		return (INT_PTR)TRUE;

	case WM_PAINT:
		return (INT_PTR)TRUE;
	case WM_TIMER:
		KillTimer(hDlg, 1);
		if (splashcount++ == 0)DestroyWindow(hDlg);
		return (INT_PTR)TRUE;
	case WM_LBUTTONDOWN:
		KillTimer(hDlg, 1);
		if (splashcount++ > 0)DestroyWindow(hDlg);
		return (INT_PTR)TRUE;

	}
	return (INT_PTR)FALSE;
}

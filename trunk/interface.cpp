#include "stdafx.h"
#include "KalScope.h"

#define _CRT_SECURE_NO_WARNINGS

//以下为自订义的读取资源函数
BOOL ImageFromIDResource(UINT nID, LPCTSTR sTR, Image * &pImg)
{
	HRSRC hRsrc = ::FindResource(hInst, MAKEINTRESOURCE(nID), sTR); // type
	if (!hRsrc)
		return FALSE;
	// load resource into memory
	DWORD len = SizeofResource(hInst, hRsrc);
	BYTE* lpRsrc = (BYTE*)LoadResource(hInst, hRsrc);
	if (!lpRsrc)
		return FALSE;
	// Allocate global memory on which to create stream
	HGLOBAL m_hMem = GlobalAlloc(GMEM_FIXED, len);
	BYTE* pmem = (BYTE*)GlobalLock(m_hMem);
	memcpy(pmem, lpRsrc, len);
	IStream* pstm;
	CreateStreamOnHGlobal(m_hMem, FALSE, &pstm);
	// load from stream
	pImg = Gdiplus::Image::FromStream(pstm);
	// free/release stuff
	GlobalUnlock(m_hMem);
	pstm->Release();
	FreeResource(lpRsrc);
	return TRUE;
}

void paint_board(HWND hWnd){
	int i;
	::Graphics *myGraphics;
	Gdiplus::Brush *myPen[3];
	HDC hdc;
	
	hdc = GetDC(hWnd);
	myGraphics = new ::Graphics(hdc);
	myPen[1] = new Gdiplus::SolidBrush(Color(255, 255, 0, 0));
	myPen[2] = new Gdiplus::SolidBrush(Color(255, 0, 0, 255));
	for (i = 0; i < 256; i++){
		char t = mainboard[i >> 4][i & 15];
		if (t){
			myGraphics->FillEllipse(myPen[t], Rect(14 + 25 * (i >> 4), 14 + 25 * (i & 15), 12, 12));
		}
	}
	delete myPen[1];
	delete myPen[2];
	myPen[1] = new Gdiplus::SolidBrush(Color(255, 255, 255, 255));
	myGraphics->FillEllipse(myPen[1], Rect(16 + 25 * (output_x), 16 + 25 * (output_y), 8, 8));
	delete myPen[1];

	char* str = (char*)alloca(256);
	sprintf(str, "%d knps", node_statistic / time_limit);
	WCHAR* drawString = (WCHAR*)calloc(256, sizeof(WCHAR));
	MultiByteToWideChar(CP_ACP, 0, str, (int)strlen(str), drawString, 255);
	// Create font and brush.
	Font* drawFont = new Font(L"Tahoma", 8);
	SolidBrush* drawBrush = new SolidBrush(Color::Black);
	// Create point for upper-left corner of drawing.
	PointF drawPoint = PointF(0.0F, 0.0F);
	// Draw string to screen.
	myGraphics->DrawString(drawString, -1, drawFont, drawPoint, drawBrush);

	delete myGraphics;
}

void clear_board(HWND hWnd){
	int x, y;
	for (x = 0; x < 16; x++)
		for (y = 0; y < 16; y++)
			mainboard[x][y] = 0;
	output_x = 0xfe;
	InvalidateRect(hWnd, NULL, TRUE);
}

int buzy = 0;

void thinking(HWND hWnd){
	HDC hdc = GetDC(hWnd);
	double x = 0.0;
	unsigned char r = 0;
	unsigned char g = 0;
	unsigned char b = 0;
	Rect rct(401, 21, 19, 19);
	Graphics gr(hdc);
	std::chrono::milliseconds dur(50);
	while (buzy){
		r = (unsigned char)(256.0*cos(x));
		g = (unsigned char)(256.0*cos(x + 1.0));
		b = (unsigned char)(256.0*sin(x));
		x += 0.1;
		Brush* br = new SolidBrush(Color(r, g, b));
		gr.FillRectangle(br, rct);
		delete br;
		std::this_thread::sleep_for(dur);
	}
}
void piece(HWND hWnd, unsigned int x, unsigned int y){
	if (buzy) return;
	buzy = 1;
	x -= 15;
	y -= 15;
	if (x % 25 > 10 || y % 25 > 10){ buzy = 0; return; }
	x /= 25;
	y /= 25;
	if (x > 14 || y > 14){ buzy = 0; return; }
	if (mainboard[x][y]){ buzy = 0; return; }
	mainboard[x][y] = 2;
	paint_board(hWnd);
	if (eval_win()){
		MessageBox(hWnd, TEXT("Blue win."), TEXT(""), NULL);
		clear_board(hWnd);
		buzy = 0;
		return;
	}
	if (eval_draw()){
		MessageBox(hWnd, TEXT("Draw."), TEXT(""), NULL);
		clear_board(hWnd);
		buzy = 0;
		return;
	}
	std::thread* t = new std::thread(thinking, hWnd);
	t->detach();
	ai_run();

	InvalidateRect(hWnd, NULL, TRUE);

	if (eval_win()){
		MessageBox(hWnd, TEXT("Red win."), TEXT(""), NULL);
		clear_board(hWnd);
		buzy = 0;
		return;
	}
	if (eval_draw()){
		MessageBox(hWnd, TEXT("Draw."), TEXT(""), NULL);
		clear_board(hWnd);
		buzy = 0;
		return;
	}
	buzy = 0;
}

void board_clicked(HWND hWnd, unsigned int x, unsigned int y){
	if (buzy) return;
	std::thread* t = new std::thread(piece, hWnd, x, y);
	t->detach();
}

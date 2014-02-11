#include "stdafx.h"
#include "KalScope.h"


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
	myGraphics->FillEllipse(myPen[1], Rect(16 + 25 * (mx), 16 + 25 * (my), 8, 8));
	delete myPen[1];
	delete myGraphics;
}

void clear_board(HWND hWnd){
	int x, y;
	for (x = 0; x < 16; x++)
		for (y = 0; y < 16; y++)
			mainboard[x][y] = 0;
	mx = 0xfe;
	InvalidateRect(hWnd, NULL, TRUE);
}

void board_clicked(HWND hWnd, unsigned int x, unsigned int y){
	x -= 15;
	y -= 15;
	if (x % 25 > 10 || y % 25 > 10) return;
	x /= 25;
	y /= 25;
	if (x > 14 || y > 14) return;
	if (mainboard[x][y]) return;
	mainboard[x][y] = 2;
	paint_board(hWnd);
	if (eval_win()){
		MessageBox(hWnd, TEXT("Blue win."), TEXT(""), NULL);
		clear_board(hWnd);
		return;
	}
	if (eval_draw()){
		MessageBox(hWnd, TEXT("Draw."), TEXT(""), NULL);
		clear_board(hWnd);
		return;
	}
	ai_run();
	paint_board(hWnd);
	if (eval_win()){
		MessageBox(hWnd, TEXT("Red win."), TEXT(""), NULL);
		clear_board(hWnd);
		return;
	}
}
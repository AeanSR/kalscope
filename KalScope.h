#pragma once

#include "Resource.h"

extern char mainboard[16][16];
extern int output_y, output_x;
extern HINSTANCE hInst;
void board_clicked(HWND hWnd, unsigned int x, unsigned int y);
BOOL ImageFromIDResource(UINT nID, LPCTSTR sTR, Image * &pImg);
char eval_win();
char eval_draw();
char eval_null();
void ai_run();
void init_table();
void paint_board(HWND hWnd);
void clear_board(HWND hWnd);

extern const char codename_str[];
extern const int time_limit;
extern char* init_str;
extern int init_finished;
extern unsigned long long node_statistic;
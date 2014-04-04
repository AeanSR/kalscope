/*
	KalScope - A Gomoku AI Implement
	Evaluation Table Viewer
	Copyright (C) 2014 AeanSR <http://aean.net/>

	Free to use, copy, modify or distribute. No warranty is given.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static int32_t* table_f;
__thread int board[16] = {0};

int main(int argc, char** argv){
    table_f = (int32_t*)calloc(3*3*3*3*3*3*3*3*3*3*3*3*3*3*3,sizeof(int32_t));

    FILE* in = fopen( "eval.tbl", "rb" );
    fread(table_f, sizeof(int32_t), 3*3*3*3*3*3*3*3*3*3*3*3*3*3*3, in );
    fclose(in);

    while(argc-->1){
        sscanf(argv[argc], "%d", &board[argc-1]);
    }
    int s=0;
    for (int i = 0; i<15; i++){
        s*=3;
        s+=board[i];
    }

    printf("0x%08X", table_f[s]);

    return 0;
}

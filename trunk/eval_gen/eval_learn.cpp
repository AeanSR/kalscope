/*
	KalScope - A Gomoku AI Implement
	Evaluation Table Self-Learn Tuner
	Copyright (C) 2014 AeanSR <http://aean.net/>

	Free to use, copy, modify or distribute. No warranty is given.
*/

/**
    You should build this file with C++11/C++0x standard enabled.
    Decrease MAXD to 3 if you do not have much patient.
    For more details, see eval_gen.cpp
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <thread>

//Evaluate score defination.
/**
    DO NOT CHANGE THIS BLOCK !!!
    IF YOU CHANGED THIS BLOCK YOU MUST MAKE A EQUIVALENT COPY TO OTHER KALSCOPE ROUTINES
    AND VICE VERSA !!!
*/
#define SCORE_BASE (0UL)
#define SCORE_WIN  ((int32_t)(1UL << 30))
#define SCORE_LOSE (-SCORE_WIN)
#define SCORE_MMASK ((int32_t)( 1UL << 10 ) - 1)

static int32_t* table_f;
__thread char board[16] = {0};
#define MAXD 5 //Max search depth.

int32_t eval_w(){
    int i;
    for(i=0;i<11;i++){
            if (board[i]>0&&
                board[i]==board[i+1]&&
                board[i]==board[i+2]&&
                board[i]==board[i+3]&&
                board[i]==board[i+4]){
                    return board[i]==1?SCORE_WIN:SCORE_LOSE;
                }
        }
    return 0;
}

int32_t eval_s(){
    int i, s = 0;
    int32_t score = SCORE_BASE;
    for (i = 0; i < 15; i++){
        s = s * 3 + board[i];
    }
    score += table_f[s];
    return score;
}

int32_t minimax(int32_t alpha, int32_t beta, int depth, int who2move){
    int32_t score= eval_w();
    if (score) return score >> (MAXD-depth);
    if (depth==0) return eval_s() >> (MAXD-depth);

    int i, flag = 0;
    for (i=0; i<15; i++){
        if(board[i]) continue;
        flag = 1;
        board[i] = who2move > 0 ? 1 : 2;
        score = minimax(alpha, beta, depth-1, -who2move);
        board[i] = 0;
        if (who2move>0){
            if (score >= beta) return beta;
            if (score > alpha) alpha = score;
        }
        else{
            if (score < beta) beta = score;
            if (score <= alpha) return alpha;
        }
    }
    if (!flag) return eval_s() >> (MAXD-depth);
    if (who2move>0)
        return alpha;
    else
        return beta;
}

void thread_body(int id){
    int s, i, t;
    int64_t score;
    for (s=id; s<3*3*3*3*3*3*3*3*3*3*3*3*3*3*3; s+=4){
         t=s;
         for(i=0;i<15;i++){
             board[i]=t%3;
             t/=3;
         }
         score = minimax(SCORE_LOSE, SCORE_WIN, MAXD, 1 );
         score += minimax(SCORE_LOSE, SCORE_WIN, MAXD, -1 );
         score /= 2;
         table_f[s] = score;
         if (!(s&0xfff)) printf("%d%d%d%d%d%d%d%d%d%d%d%d%d%d%d - 0x%08X\n",
                                board[0],board[1],board[2],board[3],
                                board[4],board[5],board[6],board[7],
                                board[8],board[9],board[10],board[11],
                                board[12],board[13],board[14],score);
    }

}

int main(void){
    table_f = (int32_t*)calloc(3*3*3*3*3*3*3*3*3*3*3*3*3*3*3,sizeof(int32_t));

    FILE* in = fopen( "eval.tbl", "rb" );
    fread(table_f, sizeof(int32_t), 3*3*3*3*3*3*3*3*3*3*3*3*3*3*3, in );
    fclose(in);

    std::thread* t1 = new std::thread(thread_body, 0);
    std::thread* t2 = new std::thread(thread_body, 1);
    std::thread* t3 = new std::thread(thread_body, 2);
    std::thread* t4 = new std::thread(thread_body, 3);

    t1->join();
    t2->join();
    t3->join();
    t4->join();

    FILE* out = fopen( "eval.tbl", "wb" );
    rewind(out);
    fwrite(table_f, sizeof(int32_t), 3*3*3*3*3*3*3*3*3*3*3*3*3*3*3, out );
    fclose(out);
    return 0;
}

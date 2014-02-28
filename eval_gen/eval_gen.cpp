/*
	KalScope - A Gomoku AI Implement
	Evaluation Table Generator
	Copyright (C) 2014 AeanSR <http://aean.net/>

	Free to use, copy, modify or distribute. No warranty is given.
*/

/**
    This subroutine auto generate an proto evaluation lookup table.
    The proto table only include knowledges about which line is a "win"
    and which line is a "lose". All of the human-given informations in
    KalScope are no more than "just go connect five", the only, very
    complete rules of freestyle gomoku.
    Then you run another subroutine "eval_learn", the computer will start
    thinking about the game based on given simple rules, and will find
    "living-four led to a certain win" and "charging-four plus a sente
    led to a certain win", and vice versa, and et cetera.
    Each time you run "eval_learn", the evaluation table will get more
    details, more knowledges. Your computer will learn what is so called
    "living-three" or "jumping-three", and find all possible threats on
    a strip of the board at last.
    Isn't this intresting? I did not teach the computer how to play gomoku.
    I just tell it what is the rule, then the computer learned it by itself,
    and played well.
    The smallest subroutine "eval_looper" will start a infinite loop of
    "eval_learn". If you want to start your own training, just start it and
    let it run one whole night, no supervision needed.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

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
char board[16] = {0};

int32_t eval_w(){
    int i;
    for(i=0;i<11;i++){
            if (board[i]==board[i+1]&&
                board[i]==board[i+2]&&
                board[i]==board[i+3]&&
                board[i]==board[i+4]){
                    return board[i]==1?SCORE_WIN:SCORE_LOSE;
                }
        }
    return 0;
}

int main(void){
    table_f = (int32_t*)calloc(3*3*3*3*3*3*3*3*3*3*3*3*3*3*3,sizeof(int32_t));
    int s, i, t;
    int32_t score;
    for(s=0;s<3*3*3*3*3*3*3*3*3*3*3*3*3*3*3;s++){
        t=s;
        for(i=0;i<15;i++){
            board[i]=t%3;
            t/=3;
        }
        score = eval_w();
        if ( score )
            table_f[s] = score;
    }
    FILE* out = fopen( "eval.tbl", "wb" );
    fwrite(table_f, sizeof(int32_t), 3*3*3*3*3*3*3*3*3*3*3*3*3*3*3, out );
    fclose(out);
    return 0;
}

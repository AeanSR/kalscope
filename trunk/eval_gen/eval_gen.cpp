/*
	KalScope - A Gomoku AI Implement
	Evaluation Table Generator
	Copyright (C) 2014 AeanSR <http://aean.net/>

	Free to use, copy, modify or distribute. No warranty is given.
*/

#if defined(_MSC_VER)
    /*
        DUE TO SOME LINKER BUG MSVC WONT COMPILE.
        It is the initial purpose to make this subroutine forked out of KalScope.
        If your MSVC works fine, just comment this block out and use it happily.
    */
    #pragma message("MSVC won't compile eval_gen subroutine. Will abort.")
	int main(){};
#else

#include <stdio.h>
#include <stdint.h>
#include <time.h>

//Evaluate score defination.
/**
    DO NOT CHANGE THIS BLOCK !!!
    IF YOU CHANGED THIS BLOCK YOU MUST MAKE A EQUIVALENT COPY TO KALSCOPE ROUTINE "core.cpp"
    AND VICE VERSA !!!
*/
#define SCORE_BASE (0UL)
#define SCORE_WIN  ((int32_t)(1UL << 30))
#define SCORE_LOSE (-SCORE_WIN)
#define SCORE_MMASK ((int32_t)( 1UL << 10 ) - 1)
#define SCORE_AL4  ( (int32_t)( 1UL << 25 ))
#define SCORE_EL4  (-(int32_t)( 1UL << 25 ))
#define SCORE_AL3  ( (int32_t)( 1UL << 20 ))
#define SCORE_EL3  (-(int32_t)( 1UL << 20 ))
#define SCORE_AC4  ( (int32_t)( 1UL << 21 ))
#define SCORE_EC4  (-(int32_t)( 1UL << 21 ))
#define SCORE_AC3  ( (int32_t)( 1UL << 10 ))
#define SCORE_EC3  (-(int32_t)( 1UL << 10 ))
#define SCORE_AM   ( (int32_t)( 1UL << 0  ))
#define SCORE_EM   (-(int32_t)( 1UL << 0  ))

static int32_t table_f[4][4][4][4][4][4] = { 0 };

int main(void){
    int a, b, c, d, e, f;

    /*
    "AC4" represent "Ally's Charging-Four"
        "charging-four" means four pieces in same color are already in a row,
        and there is only one space left for the fifth piece to place.
    "EC4" represent "Enemy's Charging-Four".
    "AL4" represent "Ally's Living-Four"
        "living-four" means four pieces in same color are already in a row,
        and there are multiple spaces left for the fifth piece to place.
        player who get a "living-four" first will get a certain win.
    "EL4" represent "Enemy's Living-Four"
    */
	table_f[0][1][1][1][1][3] = SCORE_AC4;
	table_f[0][1][1][1][1][2] = SCORE_AC4;
	table_f[0][1][1][1][1][0] = SCORE_AL4;
	table_f[1][0][1][1][1][3] = SCORE_AC4;
	table_f[1][0][1][1][1][2] = SCORE_AC4;
	table_f[1][0][1][1][1][0] = SCORE_AC4 * 2;
	table_f[1][1][0][1][1][3] = SCORE_AC4;
	table_f[1][1][0][1][1][2] = SCORE_AC4;
	table_f[1][1][0][1][1][0] = SCORE_AC4 * 2;
	table_f[1][1][1][0][1][3] = SCORE_AC4;
	table_f[1][1][1][0][1][2] = SCORE_AC4;
	table_f[1][1][1][0][1][0] = SCORE_AC4 * 2;

	table_f[0][2][2][2][2][3] = SCORE_EC4;
	table_f[0][2][2][2][2][1] = SCORE_EC4;
	table_f[0][2][2][2][2][0] = SCORE_EL4;
	table_f[2][0][2][2][2][3] = SCORE_EC4;
	table_f[2][0][2][2][2][1] = SCORE_EC4;
	table_f[2][0][2][2][2][0] = SCORE_EC4 * 2;
	table_f[2][2][0][2][2][3] = SCORE_EC4;
	table_f[2][2][0][2][2][1] = SCORE_EC4;
	table_f[2][2][0][2][2][0] = SCORE_EC4 * 2;
	table_f[2][2][2][0][2][3] = SCORE_EC4;
	table_f[2][2][2][0][2][1] = SCORE_EC4;
	table_f[2][2][2][0][2][0] = SCORE_EC4 * 2;

    /*
    "L3/C3" means "living-three / charging-three" which are likely the same concept above.
    Getting "double living-three" or "charging-four with living-three" first will lead to a certain win.
    */
	table_f[0][0][1][1][1][0] = SCORE_AL3 * 2;
	table_f[0][0][1][1][1][2] = SCORE_AC3;
	table_f[0][0][1][1][1][3] = SCORE_AC3;
	table_f[0][1][0][1][1][0] = SCORE_AL3;
	table_f[0][1][0][1][1][2] = SCORE_AC3;
	table_f[0][1][0][1][1][3] = SCORE_AC3;
	table_f[0][1][1][0][1][0] = SCORE_AL3;
	table_f[0][1][1][0][1][2] = SCORE_AC3;
	table_f[0][1][1][0][1][3] = SCORE_AC3;

	table_f[2][0][1][1][1][0] = SCORE_AC3 * 2; // We cannot judge this is a living-three or charging-three. Get a average.
	table_f[2][1][0][1][1][0] = SCORE_AC3;
	table_f[2][1][1][0][1][0] = SCORE_AC3;
	table_f[3][0][1][1][1][0] = SCORE_AC3 * 2; // Same problem.
	table_f[3][1][0][1][1][0] = SCORE_AC3;
	table_f[3][1][1][0][1][0] = SCORE_AC3;

	table_f[0][0][2][2][2][0] = SCORE_EL3 * 2;
	table_f[0][0][2][2][2][1] = SCORE_EC3;
	table_f[0][0][2][2][2][3] = SCORE_EC3;
	table_f[0][2][0][2][2][0] = SCORE_EL3;
	table_f[0][2][0][2][2][1] = SCORE_EC3;
	table_f[0][2][0][2][2][3] = SCORE_EC3;
	table_f[0][2][2][0][2][0] = SCORE_EL3;
	table_f[0][2][2][0][2][1] = SCORE_EC3;
	table_f[0][2][2][0][2][3] = SCORE_EC3;

	table_f[1][0][2][2][2][0] = SCORE_EC3 * 2; // Same problem
	table_f[1][2][0][2][2][0] = SCORE_EC3;
	table_f[1][2][2][0][2][0] = SCORE_EC3;
	table_f[3][0][2][2][2][0] = SCORE_EC3 * 2; // Same problem
	table_f[3][2][0][2][2][0] = SCORE_EC3;
	table_f[3][2][2][0][2][0] = SCORE_EC3;

    /*
    Make a mirror.
    */
	for (a = 0; a < 4; a++){
		for (b = 0; b < 4; b++){
			for (c = 0; c < 4; c++){
				for (d = 0; d < 4; d++){
					for (e = 0; e < 4; e++){
						for (f = 0; f < 4; f++)
						{
							int32_t t1 = table_f[a][b][c][d][e][f];
							int32_t t2 = table_f[f][e][d][c][b][a];
							if (t2 == 0)
								table_f[f][e][d][c][b][a] = t1;
							if (t1 == 0)
								table_f[a][b][c][d][e][f] = t2;
						}
					}
				}
			}
		}
	}

    /*
    Some tweaks appreciate pieces which are linked together.
    */
	for (a = 0; a < 4; a++)
		for (b = 0; b < 4; b++)
			for (c = 0; c < 4; c++){
				table_f[1][1][1][a][b][c] += SCORE_AM;
				table_f[2][2][2][a][b][c] += SCORE_EM;
				table_f[a][1][1][1][b][c] += SCORE_AM;
				table_f[a][2][2][2][b][c] += SCORE_EM;
				table_f[a][b][1][1][1][c] += SCORE_AM;
				table_f[a][b][2][2][2][c] += SCORE_EM;
				table_f[a][b][c][1][1][1] += SCORE_AM;
				table_f[a][b][c][2][2][2] += SCORE_EM;
				table_f[1][0][1][a][b][c] += SCORE_AM;
				table_f[2][0][2][a][b][c] += SCORE_EM;
				table_f[a][1][0][1][b][c] += SCORE_AM;
				table_f[a][2][0][2][b][c] += SCORE_EM;
				table_f[a][b][1][0][1][c] += SCORE_AM;
				table_f[a][b][2][0][2][c] += SCORE_EM;
				table_f[a][b][c][1][0][1] += SCORE_AM;
				table_f[a][b][c][2][0][2] += SCORE_EM;
				table_f[0][1][1][a][b][c] += SCORE_AM;
				table_f[0][2][2][a][b][c] += SCORE_EM;
				table_f[a][0][1][1][b][c] += SCORE_AM;
				table_f[a][0][2][2][b][c] += SCORE_EM;
				table_f[a][b][0][1][1][c] += SCORE_AM;
				table_f[a][b][0][2][2][c] += SCORE_EM;
				table_f[a][b][c][0][1][1] += SCORE_AM;
				table_f[a][b][c][0][2][2] += SCORE_EM;
				table_f[1][1][0][a][b][c] += SCORE_AM;
				table_f[2][2][0][a][b][c] += SCORE_EM;
				table_f[a][1][1][0][b][c] += SCORE_AM;
				table_f[a][2][2][0][b][c] += SCORE_EM;
				table_f[a][b][1][1][0][c] += SCORE_AM;
				table_f[a][b][2][2][0][c] += SCORE_EM;
				table_f[a][b][c][1][1][0] += SCORE_AM;
				table_f[a][b][c][2][2][0] += SCORE_EM;
				for (d = 0; d < 4; d++){
					table_f[1][1][a][b][c][d] += SCORE_AM;
					table_f[2][2][a][b][c][d] += SCORE_EM;
					table_f[a][1][1][b][c][d] += SCORE_AM;
					table_f[a][2][2][b][c][d] += SCORE_EM;
					table_f[a][b][1][1][c][d] += SCORE_AM;
					table_f[a][b][2][2][c][d] += SCORE_EM;
					table_f[a][b][c][1][1][d] += SCORE_AM;
					table_f[a][b][c][2][2][d] += SCORE_EM;
					table_f[a][b][c][d][1][1] += SCORE_AM;
					table_f[a][b][c][d][2][2] += SCORE_EM;
				}
			}
    /*
    Notice, not all "charging-four" or "living-four" get a score here,
    because we initially forced the fifth dimension to be always a legal piece.
    Combinations such as "1-1-1-1-0-1" will never be evaluated in this table.
    So you needn't worry about that.
    */

    /*
    Write into eval.inc file. You can make some tweaks to evaltbl above here.
    */
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    FILE* out = fopen( "eval.inc", "w" );
	fprintf(out, "/*\n"
		"\tKalScope - A Gomoku AI Implement\n"
		"\tEvaluation Table @%s\n"
		"\tCopyright(C) 2014 AeanSR <http://aean.net/>\n"
		"\n"
		"\tFree to use, copy, modify or distribute. No warranty is given.\n"
		"*/\n\n"
		"/**\n"
		"\tTHIS FILE IS AUTOMATICALLY GENERATED BY EVAL_GEN, A KALSCOPE SUBROUTINE.\n"
		"\tAS MOST PPL READ THIS FILE WOULD NOT HELP UNDERSTAND HOW KALSCOPE WORKS,\n"
		"\tSEE eval_gen\\eval_gen.cpp FOR IMPLEMENT DETAILS.\n"
		"*/\n\n"
		"#pragma once\n"
		"static const int32_t table_f[4][4][4][4][4][4] = {\n",
		asctime (timeinfo)
		);
	for (a = 0; a < 4; a++){
		for (b = 0; b < 4; b++){
			for (c = 0; c < 4; c++){
				for (d = 0; d < 4; d++){
					for (e = 0; e < 4; e++){
						fprintf(out, "\t/*");
						for (f = 0; f < 4; f++){
							fprintf(out, "    %d-%d-%d-%d-%d-%d,", a, b, c, d, e, f);
						}
						fprintf(out, "*/\n\t  ");
						for (f = 0; f < 4; f++)
						{
							int32_t t1 = table_f[a][b][c][d][e][f];
							printf( "%d%d%d%d%d%d: %8x\n", a, b, c, d, e, f, t1 );
							fprintf(out, "     0x%08X,", t1);
						}
						fprintf(out, "\n\n");
					}
				}
			}
		}
	}
	fprintf(out, "};\n");
    fclose(out);
    return 0;
}
#endif

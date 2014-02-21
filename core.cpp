/*
	KalScope - A Gomoku AI Implement
	AI Core Module
	Copyright (C) 2014 AeanSR <http://aean.net/>

	Free to use, copy, modify or distribute. No warranty is given.
*/

#include "stdafx.h"
#include "KalScope.h"

static const char codename_str[] = "AI Core Module \"Shadowglen\" 2014Feb.";

/* Search Depth. This macro is equal to max depth minus 1.  */
#define intelligence 5

//Some branch-less macros.
#define sshr32(v,d) (-(int32_t)((uint32_t)(v) >> d))
#define max32(x,y)  ((x) - (((x) - (y)) & sshr32((x) - (y), 31)))
#define min32(x,y)  ((y) + (((x) - (y)) & sshr32((x) - (y), 31)))
#define abs32(v)    (((v) ^ sshr32((v), 31)) - sshr32((v), 31))

//Evaluate score defination.
#define SCORE_WIN  ((int32_t)(1UL << 30))
#define SCORE_LOSE (-SCORE_WIN)
#define SCORE_AL4  ( (int32_t)( 1UL << 25 ))
#define SCORE_EL4  (-(int32_t)( 1UL << 25 ))
#define SCORE_AL3  ( (int32_t)( 1UL << 20 ))
#define SCORE_EL3  (-(int32_t)( 1UL << 20 ))
#define SCORE_AC4  ( (int32_t)( 1UL << 20 ))
#define SCORE_EC4  (-(int32_t)( 1UL << 20 ))
#define SCORE_AC3  ( (int32_t)( 1UL << 10 ))
#define SCORE_EC3  (-(int32_t)( 1UL << 10 ))
#define SCORE_AM   ( (int32_t)( 1UL << 0  ))
#define SCORE_EM   (-(int32_t)( 1UL << 0  ))
#define SCORE_MMASK ((int32_t)( 1UL << 10 ) - 1)
#define SCORE_BASE (0UL)

enum{ TYPE_NON = 0, TYPE_PV = 1, TYPE_A = 2, TYPE_B = 3, };
typedef struct{
	uint64_t key;
	int32_t value;
	char x;
	char y;
	char type;
	char depth;
} hash_t;
struct move_t{
	int32_t score;
	short x;
	short y;
	void swap(move_t& m){
		move_t t;
		t = m;
		m = *this;
		*this = t;
	}
};

//Structure for board representation.
char mainboard[16][16] = { 0 };
char __declspec(thread) board[19][32] = { 0 };

//Structure for evaluate lookup table.
static int32_t table_f[4][4][4][4][4][4] = { 0 };
static int init_flag = 0;

//Structure for result reduce.
int32_t m;
int my, mx = 0xfe;

//Structure for thread sync.
std::mutex tlock;
std::mutex hlock[1024];
std::thread* thm[225] = { NULL };
size_t tid = 0;

//Structure for TT.
#define HASH_SIZE_DEFAULT (0x1000000)
size_t HASH_SIZE = HASH_SIZE_DEFAULT - 1;
static uint64_t zobrist[2][16][16] = { 0 };
uint64_t __declspec(thread) key = 0;
hash_t* hash_table;

int count_processor(){
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}

size_t memory_to_use(){
	MEMORYSTATUSEX mem;
	mem.dwLength = sizeof(MEMORYSTATUSEX);
	if (!GlobalMemoryStatusEx(&mem))
		return HASH_SIZE_DEFAULT;
	uint64_t a;
	a = mem.ullAvailPhys;
	a /= 2; /* Use half of available memory. */
	a--;
	a |= a >> 1;
	a |= a >> 2;
	a |= a >> 4;
	a |= a >> 8;
	a |= a >> 16;
	a |= a >> 32;
	a++;
	a /= sizeof(hash_t);
	return a > 0 ? a : HASH_SIZE_DEFAULT;
}

uint64_t zobrist_key(){
	int x, y;
	unsigned long c;
	uint64_t z = 0;
	static const __m128i xmm0 = _mm_setzero_si128();
	static const __m128i xmm2 = _mm_set1_epi32(0xff);
	for (x = 0; x < 15; x++){
		y = 0;
		__m128i xmm1 = _mm_loadu_si128((__m128i*)board[x]);
		unsigned int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(xmm0, xmm1));
		mask = (~mask) & 0x7fff;
		while (mask){
			_BitScanForward(&c, mask);
			y += c;
			z ^= zobrist[board[x][y] - 1][x][y];
			mask >>= c + 1;
			y++;
		}
	}
	return z;
}

void __fastcall record_hash(int32_t score, int x = 0xfe, int y = 0xfe, int type = TYPE_NON, int depth = 0){
	hash_t* p = &hash_table[key & HASH_SIZE];
	hlock[key % 1024].lock();
	if (p->depth > depth){
		hlock[key % 1024].unlock();
		return;
	}
	p->key = key;
	p->value = score;
	p->x = x;
	p->y = y;
	p->type = type;
	p->depth = depth;
	hlock[key % 1024].unlock();
}

char __forceinline idle(int x, int y){
	if (board[x][y] > 0) return 1;
	int lx = x > 0 ? x - 1 : x;
	int rx = x + 1;
	int ty = y > 0 ? y - 1 : y;
	int by = y + 1;
	char a = board[lx][ty];
	char b = board[lx][y];
	char c = board[lx][by];
	char d = board[x][ty];
	a |= board[x][by];
	b |= board[rx][ty];
	c |= board[rx][y];
	d |= board[rx][by];
	a |= b;
	c |= d;
	a |= c;
	a = (unsigned char)(-a) >> 7;
	return a - 1;
}

char __forceinline mainidle(int x, int y){
	if (mainboard[x][y]) return 1;
	char t = 0;
	int lx = x > 0 ? x - 1 : x;
	int rx = x < 14 ? x + 1 : x;
	int ty = y > 0 ? y - 1 : y;
	int by = y < 14 ? y + 1 : y;
	t |= mainboard[lx][ty];
	t |= mainboard[x][ty];
	t |= mainboard[rx][ty];
	t |= mainboard[lx][y];
	t |= mainboard[rx][y];
	t |= mainboard[lx][by];
	t |= mainboard[x][by];
	t |= mainboard[rx][by];

	return !t;
}

char eval_w(){
	int x, y;
	unsigned long c;
	static const __m128i xmm0 = _mm_setzero_si128();
	static const __m128i xmm2 = _mm_set1_epi32(0xff);
	for (x = 0; x < 15; x++){
		y = 0;
		__m128i xmm1 = _mm_loadu_si128((__m128i*)board[x]);
		unsigned int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(xmm0, xmm1));
		mask = (~mask) & 0x7fff;
		while (mask){
			_BitScanForward(&c, mask);
			y += c;
			int xm4, xm3, xm2, xm1;
			if (x >> 2){
				xm4 = x - 4;
				xm3 = x - 3;
				xm2 = x - 2;
				xm1 = x - 1;
			}
			else{
				xm4 = x;
				xm3 = x;
				xm2 = x;
				xm1 = x;
			}
			__declspec(align(16)) char cache[] = {
				board[xm4  ][y + 4], board[xm3  ][y + 3], board[xm2  ][y + 2], board[xm1  ][y + 1],
				board[x    ][y + 1], board[x    ][y + 2], board[x    ][y + 3], board[x    ][y + 4],
				board[x + 1][y    ], board[x + 2][y    ], board[x + 3][y    ], board[x + 4][y    ],
				board[x + 1][y + 1], board[x + 2][y + 2], board[x + 3][y + 3], board[x + 4][y + 4],

			};
			xmm1 = _mm_load_si128((__m128i*)cache);
			xmm1 = _mm_and_si128(_mm_srli_epi32(xmm1, 16), xmm1);
			xmm1 = _mm_and_si128(_mm_srli_epi32(xmm1,  8), xmm1);
			xmm1 = _mm_and_si128(xmm2, xmm1);
			xmm1 =  _mm_or_si128(_mm_srli_si128(xmm1, 8), xmm1);
			xmm1 =  _mm_or_si128(_mm_srli_si128(xmm1, 4), xmm1);
			char result = board[x][y] & _mm_extract_epi16(xmm1, 0);
			if (result) return result;
			mask >>= c + 1;
			y ++;
		}
	}
	return 0;
}

char eval_draw(){
	int x;
	__m128i xmm1, xmm2 = _mm_setzero_si128();
	__m128i xmm0 = _mm_setzero_si128();
	for (x = 0; x < 15; x++){
		xmm1 = _mm_loadu_si128((__m128i*)mainboard[x]);
		xmm1 = _mm_cmpeq_epi8(xmm1, xmm0);
		xmm2 = _mm_or_si128(xmm2, xmm1);
	}
	int mask = _mm_movemask_epi8(xmm2);
	mask = mask & 0x7fff;
	return !mask;
}

char eval_null(){
	int x;
	__m128i xmm1, xmm2 = _mm_set1_epi8(-1);
	__m128i xmm0 = _mm_setzero_si128();
	for (x = 0; x < 15; x++){
		xmm1 = _mm_loadu_si128((__m128i*)mainboard[x]);
		xmm1 = _mm_cmpeq_epi8(xmm1, xmm0);
		xmm2 = _mm_and_si128(xmm2, xmm1);
	}
	int mask = _mm_movemask_epi8(xmm2);
	mask = mask & 0x7fff;
	return mask == 0x7fff;
}

char eval_win(){
	int x, y;
	bool not_be, not_le, not_re;
	char ret = 0;
	for (x = 0; x < 15; x++)
		for (y = 0; y < 15; y++){
			if (!mainboard[x][y]) continue;
			not_be = y < 11;
			not_le = x > 3;
			not_re = x < 11;
			if (not_be){
				ret |= mainboard[x][y]
				     & mainboard[x][y + 1]
				     & mainboard[x][y + 2]
				     & mainboard[x][y + 3]
				     & mainboard[x][y + 4];
				if (not_le)
					ret |= mainboard[x][y]
					     & mainboard[x - 1][y + 1]
					     & mainboard[x - 2][y + 2]
					     & mainboard[x - 3][y + 3]
					     & mainboard[x - 4][y + 4];
			}
			if (not_re){
				ret |= mainboard[x][y]
                     & mainboard[x + 1][y]
                     & mainboard[x + 2][y]
                     & mainboard[x + 3][y]
                     & mainboard[x + 4][y];
				if (not_be)
					ret |= mainboard[x][y]
					     & mainboard[x + 1][y + 1]
					     & mainboard[x + 2][y + 2]
					     & mainboard[x + 3][y + 3]
					     & mainboard[x + 4][y + 4];
			}
			if (ret) return ret;
		}
	return 0;
}

void init_table(){
	if (init_flag) return;
	init_flag++;
	int a, b, c, d, e, f;
	static std::mt19937_64 rng;
	HASH_SIZE = memory_to_use() - 1;
	hash_table = new hash_t[HASH_SIZE+1];
	memset(hash_table, 0, sizeof(hash_t)*(HASH_SIZE+1)); //Occupy memory. Avoid another kalscope process allocate hash table too large.
	for (a = 0; a < 15; a++)
		for (b = 0; b < 15; b++){
			zobrist[0][a][b] = rng();
			zobrist[1][a][b] = rng();
		}

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

	table_f[0][1][1][1][1][3] = SCORE_AC4;
	table_f[0][1][1][1][1][2] = SCORE_AC4;
	table_f[0][1][1][1][1][0] = SCORE_AL4 * 2;
	table_f[1][0][1][1][1][3] = SCORE_AC4;
	table_f[1][0][1][1][1][2] = SCORE_AC4;
	table_f[1][0][1][1][1][0] = SCORE_AL4;
	table_f[1][1][0][1][1][3] = SCORE_AC4;
	table_f[1][1][0][1][1][2] = SCORE_AC4;
	table_f[1][1][0][1][1][0] = SCORE_AL4;
	table_f[1][1][1][0][1][3] = SCORE_AC4;
	table_f[1][1][1][0][1][2] = SCORE_AC4;
	table_f[1][1][1][0][1][0] = SCORE_AL4;

	table_f[0][2][2][2][2][3] = SCORE_EC4;
	table_f[0][2][2][2][2][1] = SCORE_EC4;
	table_f[0][2][2][2][2][0] = SCORE_EL4 * 2;
	table_f[2][0][2][2][2][3] = SCORE_EC4;
	table_f[2][0][2][2][2][1] = SCORE_EC4;
	table_f[2][0][2][2][2][0] = SCORE_EL4;
	table_f[2][2][0][2][2][3] = SCORE_EC4;
	table_f[2][2][0][2][2][1] = SCORE_EC4;
	table_f[2][2][0][2][2][0] = SCORE_EL4;
	table_f[2][2][2][0][2][3] = SCORE_EC4;
	table_f[2][2][2][0][2][1] = SCORE_EC4;
	table_f[2][2][2][0][2][0] = SCORE_EL4;

	table_f[0][0][1][1][1][0] = SCORE_AL3;
	table_f[0][0][1][1][1][2] = SCORE_AC3;
	table_f[0][0][1][1][1][3] = SCORE_AC3;
	table_f[0][1][0][1][1][0] = SCORE_AL3;
	table_f[0][1][0][1][1][2] = SCORE_AC3;
	table_f[0][1][0][1][1][3] = SCORE_AC3;
	table_f[0][1][1][0][1][0] = SCORE_AL3;
	table_f[0][1][1][0][1][2] = SCORE_AC3;
	table_f[0][1][1][0][1][3] = SCORE_AC3;

	table_f[2][0][1][1][1][0] = SCORE_AC3 * 2;
	table_f[2][1][0][1][1][0] = SCORE_AC3;
	table_f[2][1][1][0][1][0] = SCORE_AC3;
	table_f[3][0][1][1][1][0] = SCORE_AC3 * 2;
	table_f[3][1][0][1][1][0] = SCORE_AC3;
	table_f[3][1][1][0][1][0] = SCORE_AC3;

	table_f[0][0][2][2][2][0] = SCORE_EL3;
	table_f[0][0][2][2][2][1] = SCORE_EC3;
	table_f[0][0][2][2][2][3] = SCORE_EC3;
	table_f[0][2][0][2][2][0] = SCORE_EL3;
	table_f[0][2][0][2][2][1] = SCORE_EC3;
	table_f[0][2][0][2][2][3] = SCORE_EC3;
	table_f[0][2][2][0][2][0] = SCORE_EL3;
	table_f[0][2][2][0][2][1] = SCORE_EC3;
	table_f[0][2][2][0][2][3] = SCORE_EC3;

	table_f[1][0][2][2][2][0] = SCORE_EC3 * 2;
	table_f[1][2][0][2][2][0] = SCORE_EC3;
	table_f[1][2][2][0][2][0] = SCORE_EC3;
	table_f[3][0][2][2][2][0] = SCORE_EC3 * 2;
	table_f[3][2][0][2][2][0] = SCORE_EC3;
	table_f[3][2][2][0][2][0] = SCORE_EC3;

	for (a = 0; a < 4; a++)
		for (b = 0; b < 4; b++)
			for (c = 0; c < 4; c++)
				for (d = 0; d < 4; d++)
					for (e = 0; e < 4; e++)
						for (f = 0; f < 4; f++){
							int32_t t1 = table_f[a][b][c][d][e][f];
							int32_t t2 = table_f[f][e][d][c][b][a];
							if (abs32(t1) > abs32(t2)){
								table_f[f][e][d][c][b][a] = t1;
							}
							else{
								table_f[a][b][c][d][e][f] = t2;
							}
						}

}

int32_t eval_s(){
	/*hash_t* p = &hash_table[key & HASH_SIZE];
	if (p->key == key)
		return p->value;*/
	switch (eval_w()){
	case 1:
		//record_hash(SCORE_WIN);
		return SCORE_WIN;
	case 2:
		//record_hash(SCORE_LOSE);
		return SCORE_LOSE;
	default:
		break;
	}
	int x, y;
	unsigned long c;
	static const __m128i xmm0 = _mm_setzero_si128();
	int32_t score = SCORE_BASE;
	for (x = 0; x < 15; x++){
		y = 0;
		__m128i xmm1 = _mm_loadu_si128((__m128i*)board[x]);
		unsigned int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(xmm0, xmm1));
		mask = (~mask) & 0x7fff;
		while (mask){
			_BitScanForward(&c, mask);
			y += c;
			if (y < 11){
				score += table_f[board[x][y + 4]]
					[board[x][y + 3]]
				[board[x][y + 2]]
				[board[x][y + 1]]
				[board[x][y]]
				[(y == 0) ? 3 : board[x][y - 1]];
				if (x > 3){
					score += table_f[board[x - 4][y + 4]]
						[board[x - 3][y + 3]]
					[board[x - 2][y + 2]]
					[board[x - 1][y + 1]]
					[board[x][y]]
					[(y == 0 || x == 14) ? 3 : board[x + 1][y - 1]];
				}

			}
			else if (y < 12){
				score += table_f[3]
					[board[x][y + 3]]
				[board[x][y + 2]]
				[board[x][y + 1]]
				[board[x][y]]
				[(y == 0) ? 3 : board[x][y - 1]];
				if (x > 3){
					score += table_f[3]
						[board[x - 3][y + 3]]
					[board[x - 2][y + 2]]
					[board[x - 1][y + 1]]
					[board[x][y]]
					[(y == 0 || x == 14) ? 3 : board[x + 1][y - 1]];
				}
			}
			if (x < 11){
				score += table_f[board[x + 4][y]]
					[board[x + 3][y]]
				[board[x + 2][y]]
				[board[x + 1][y]]
				[board[x][y]][(x == 0) ? 3 : board[x - 1][y]];
				if (y < 11){
					score += table_f[board[x + 4][y + 4]]
						[board[x + 3][y + 3]]
					[board[x + 2][y + 2]]
					[board[x + 1][y + 1]]
					[board[x][y]]
					[(y == 0 || x == 0) ? 3 : board[x - 1][y - 1]];
				}
			}
			else if (x < 12){
				score += table_f[3]
					[board[x + 3][y]]
				[board[x + 2][y]]
				[board[x + 1][y]]
				[board[x][y]][(x == 0) ? 3 : board[x - 1][y]];
				if (y < 12){
					score += table_f[3]
						[board[x + 3][y + 3]]
					[board[x + 2][y + 2]]
					[board[x + 1][y + 1]]
					[board[x][y]]
					[(y == 0 || x == 0) ? 3 : board[x - 1][y - 1]];
				}
			}
			mask >>= c + 1;
			y++;
		}
	}
	//record_hash(score);
	return score;
}


void __fastcall move_sort(move_t* movelist, int first, int last){
	if (last - first > 1){
		int i = first + 1;
		int j = last;
		int32_t key = movelist[first].score;
		while (1){
			while (key > movelist[j].score)
				j--;
			while (key < movelist[i].score && i<j)
				i++;
			if (i >= j) break;
			movelist[i].swap(movelist[j]);
			if (movelist[i].score == key)
				j--;
			else
				i++;
		}
		movelist[j].swap(movelist[first]);
		if (first  < i - 1) move_sort(movelist, first, i - 1);
		if (j + 1 < last) move_sort(movelist, j + 1, last);
	}
	else{
		if (movelist[first].score < movelist[last].score){
			movelist[first].swap(movelist[last]);
		}
	}
}

int move_gen(move_t* movelist, hash_t* h, int color, int depth){
	int count = 0;
	int hx = 0xfe;
	int hy = 0xfe;
	int x, y;
	uint64_t k;
	hash_t* p;
	if (h->key == key && h->type && h->x != 0xfe){
		hx = h->x;
		hy = h->y;
	}
	if(depth >= 2){
		for (x = 0; x < 15; x++)
			for (y = 0; y < 15; y++){
				if (idle(x, y)) continue;
				if ((x ^ hx) | (y ^ hy)) {
					k = key ^ zobrist[color - 1][x][y];
					p = &hash_table[k & HASH_SIZE];
					int64_t mask = p->key;
					movelist[count].x = x;
					movelist[count].y = y;
					count++;
					mask ^= k;
					mask |= (-mask);
					mask = (uint64_t)mask >> 63;
					mask = -mask;
					movelist[count].score = mask & p->value;
				}
				else{
					movelist[count].score = INT32_MAX;
					movelist[count].x = x;
					movelist[count].y = y;
					count++;
				}

			}
		move_sort(movelist, 0, count - 1);
	}
	else{
		for (x = 0; x < 15; x++)
			for (y = 0; y < 15; y++){
				if (idle(x, y)) continue;
				movelist[count].x = x;
				movelist[count].y = y;
				if ((x ^ hx) | (y ^ hy))
					movelist[count].score = 0;
				else{
					movelist[count].score = INT32_MAX;
					movelist[0].swap(movelist[count]);
				}
				count++;
			}
	}

	return count;
}

int32_t __fastcall alpha_beta(int32_t alpha, int32_t beta, int depth, int is_pv){
	int32_t reg;
	int x, y;
	int by, bx = 0xfe;
	int hy = 0xfe;
	int hx = 0xfe;
	int who2move = ((depth ^ intelligence) & 1 ? 1 : -1);
	char color = (who2move > 0 ? 1 : 2);
	int alpha_raised = 0;
	hash_t* h = &hash_table[key & HASH_SIZE];
	bool found = !(h->key ^ key);

	if (depth){
		// If TT returned a deeper history result, use it.
		if (found && h->depth >= depth){
			if (h->type == TYPE_PV)
				return h->value;
			else if (h->type == TYPE_B)
				alpha = max32(alpha, h->value);
			else if (h->type == TYPE_A)
				beta = min32(beta, h->value);
			if (alpha >= beta)
				return h->value;
		}

		/* Judge win / lose during recursive. */
		if (0){
			// TT (disabled due to bug).
			if (h->value <= SCORE_LOSE + intelligence || h->value >= SCORE_WIN){
				return - h->value;
			}
		}
		else
			// If TT not hit, call eval_w().
			switch (eval_w()){
			case 1:
				return who2move * (SCORE_WIN + depth);
			case 2:
				return who2move * (SCORE_LOSE + intelligence - depth);
			default:
				break;
			}

		//Generate all moves and sort.
		move_t mlist[225];
		int count = move_gen(mlist, h, color, depth);

		// Search all moves.
		for (int i = 0; i < count; i++)
		{
			x = mlist[i].x;
			y = mlist[i].y;
			board[x][y] = color;
			key ^= zobrist[color - 1][x][y];

			// Do principle variation search.
			if (!is_pv || depth < 3)
				reg = -alpha_beta(-beta, -alpha, depth - 1, 0);
			else if (!alpha_raised)
				reg = -alpha_beta(-beta, -alpha, depth - 1, 1);
			else{
				reg = -alpha_beta(-alpha - 1, -alpha, depth - 1, 0);
				if (reg > alpha && reg < beta)
					reg = -alpha_beta(-beta, -alpha, depth - 1, 1);
			}

			board[x][y] = 0;
			key ^= zobrist[color - 1][x][y];
			if (reg >= beta){
				record_hash(reg, x, y, TYPE_B, depth);
				return beta;
			}
			if (reg > alpha){
				alpha_raised = 1;
				alpha = reg;
				bx = x;
				by = y;
			}
		}
		if (bx != 0xfe) record_hash(alpha, bx, by, TYPE_PV, depth);
		else record_hash(alpha, 0xfe, 0xfe, TYPE_A, depth);
		return alpha;
	}
	else{
		/* Depth == 0: call evaluation function. */
		if (found) return h->value;
		reg = who2move * eval_s();
		record_hash(reg);
		return reg;
	}
}

#define CPY(v) memcpy(board[(v)],mainboard[(v)],16)
#define DCPY(v) CPY(v);CPY((v)+1);CPY((v)+2);CPY((v)+3)
#define COPYBOARD() DCPY(0);DCPY(4);DCPY(8);DCPY(12)
move_t msa[225];
int msp;
std::mutex msl;

bool getmove(int& _x, int& _y){
	msl.lock();
	if (msp == 0){
		msl.unlock();
		return 0;
	}
	_x = msa[--msp].x;
	_y = msa[msp].y;
	msl.unlock();
	return 1;
}
void pushmove(int _x, int _y, int32_t score){
	msl.lock();
	msa[msp].x = _x;
	msa[msp].y = _y;
	msa[msp++].score = score;
	msl.unlock();
}

void thread_body(){
	COPYBOARD();
	int x, y;
	int32_t localm;
	while (getmove(x, y)){
		board[x][y] = 1;
		key = zobrist_key();
		int maxdepth = intelligence & 1;
		int32_t reg;
		while (maxdepth <= intelligence){
			tlock.lock();
			localm = m;
			tlock.unlock();

			if (localm <= SCORE_LOSE + intelligence)
				reg = -alpha_beta(-SCORE_WIN - intelligence, -localm, maxdepth, 1);
			else{
				reg = -alpha_beta(-localm - 1, -localm, maxdepth, 0);
				if (reg > localm && reg < SCORE_WIN)
					reg = -alpha_beta(-SCORE_WIN - intelligence, -localm, maxdepth, 1);
			}

			if (reg <= SCORE_LOSE + intelligence || reg >= SCORE_WIN){
				reg -= maxdepth;
				break;
			}
			maxdepth += 2;
		}
		tlock.lock();
		if (reg > m || mx == 0xfe){
			m = reg;
			mx = x;
			my = y;
		}
		tlock.unlock();
		board[x][y] = 0;
	}
}

void ai_run(){
	int x, y;
	tid = 0;
	mx = 0xfe;
	m = SCORE_LOSE;
	init_table();
	static const int ccpu = count_processor();
	int mvcount = 0;

	int bx = 0xfe;
	int by = 0xfe;
	COPYBOARD();
	uint64_t k = zobrist_key();
	hash_t* h = &hash_table[k & (HASH_SIZE-1)];
	if (h->key == key && h->type){
		bx = h->x;
		by = h->y;
		if (bx >= 15 || bx < 0 || by >= 15 || by < 0){
			bx = 0xfe;
			by = 0xfe;
		}
	}

	for (x = 0; x < 15; x++)
		for (y = 0; y < 15; y++){
			if (mainidle(x, y)) continue;
			if (x == bx&&y == by) continue;
			k ^= zobrist[0][x][y];
			hash_t* p = &hash_table[k & HASH_SIZE];
			pushmove(x, y, -(p->key == k ? p->value : 0) );
			k ^= zobrist[0][x][y];
			mvcount++;
		}
	if (bx != 0xfe){
		mvcount++;
		pushmove(bx, by, INT32_MIN);
	}
	move_sort(msa, 0, mvcount - 1);

	for (x = 0; x < ccpu; x++)
		thm[tid++] = new std::thread(thread_body);
	do{
		--tid;
		if (thm[tid]){
			thm[tid]->join();
			delete thm[tid];
			thm[tid] = NULL;
		}
	} while (tid && tid<256);
	mainboard[mx][my] = 1;
}

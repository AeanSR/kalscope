/*
	KalScope - A Gomoku AI Implement
	AI Core Module
	AeanSR<aeanswiftriver@gmail.com>
*/

#include "stdafx.h"
#include "KalScope.h"

#define intelligence 5
#define HASH_SIZE (0x40000 << intelligence)
#define SCORE_WIN  ((int64_t)(1ULL << 60))
#define SCORE_LOSE (-SCORE_WIN)
#define SCORE_AL4  ( (int64_t)( 1ULL << (intelligence & 1 ? 55 : 50) ))
#define SCORE_EL4  (-(int64_t)( 1ULL << (intelligence & 1 ? 50 : 55) ))
#define SCORE_AL3  ( (int64_t)( 1ULL << (intelligence & 1 ? 45 : 40) ))
#define SCORE_EL3  (-(int64_t)( 1ULL << (intelligence & 1 ? 40 : 45) ))
#define SCORE_AC4  ( (int64_t)( 1ULL << (intelligence & 1 ? 45 : 40) ))
#define SCORE_EC4  (-(int64_t)( 1ULL << (intelligence & 1 ? 40 : 45) ))
#define SCORE_AC3  ( (int64_t)( 1ULL << (intelligence & 1 ? 35 : 30) ))
#define SCORE_EC3  (-(int64_t)( 1ULL << (intelligence & 1 ? 30 : 35) ))
#define SCORE_AM   ( (int64_t)( 1ULL << 15 ))
#define SCORE_EM   (-(int64_t)( 1ULL << 15 ))
#define SCORE_BASE (intelligence & 1 ? 0x7C1F07E0000000LL : 0xF83E0F820000000LL)

char mainboard[16][16] = { 0 };
char __declspec(thread) board[19][32] = { 0 };
static int64_t table_f[4][4][4][4][4][4] = { 0 };
static int init_flag = 0;
int64_t m;
int my, mx = 0xfe;
std::mutex tlock;
std::mutex hlock[1024];
std::thread* thm[225] = { NULL };
size_t tid = 0;
static uint64_t zobrist[2][16][16] = { 0 };
uint64_t __declspec(thread) key = 0;
typedef struct{
	uint64_t key;
	int64_t value;
	int x;
	int y;
	int app_s;
	int app_m;
} hash_t;
hash_t* hash_table;

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

void record_hash(int64_t score){
	hash_t* p = &hash_table[key % HASH_SIZE];
	hlock[key % 1024].lock();
	if (p->key != key) p->app_m = 0;
	p->key = key;
	p->value = score;
	p->app_s = 1;
	hlock[key % 1024].unlock();
}
void record_hash(int x, int y){
	hash_t* p = &hash_table[key % HASH_SIZE];
	hlock[key % 1024].lock();
	if (p->key != key) p->app_s = 0;
	p->key = key;
	p->x = x;
	p->y = y;
	p->app_m = 1;
	hlock[key % 1024].unlock();
}

char __forceinline idle(int x, int y){
	if (board[x][y] > 0) return 1;
	int lx = x > 0 ? x - 1 : x;
	int rx = x + 1;
	int ty = y > 0 ? y - 1 : y;
	int by = y + 1;
	__declspec(align(16)) char cache[8] = {
		board[lx][ty],board[lx][y],board[lx][by],
		board[x][ty] ,             board[x][by] ,
		board[rx][ty],board[rx][y],board[rx][by]
	};
	__m128i xmm1 = _mm_loadl_epi64((__m128i*)cache);
	__m128i xmm0 = _mm_setzero_si128();
	xmm1 = _mm_cmpeq_epi64(xmm1, xmm0);
	return _mm_movemask_epi8(xmm1);
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
				ret |= mainboard[x][y] & mainboard[x][y + 1] & mainboard[x][y + 2] & mainboard[x][y + 3] & mainboard[x][y + 4];
				if (not_le)
					ret |= mainboard[x][y] & mainboard[x - 1][y + 1] & mainboard[x - 2][y + 2] & mainboard[x - 3][y + 3] & mainboard[x - 4][y + 4];
			}
			if (not_re){
				ret |= mainboard[x][y] & mainboard[x + 1][y] & mainboard[x + 2][y] & mainboard[x + 3][y] & mainboard[x + 4][y];
				if (not_be)
					ret |= mainboard[x][y] & mainboard[x + 1][y + 1] & mainboard[x + 2][y + 2] & mainboard[x + 3][y + 3] & mainboard[x + 4][y + 4];
			}
			if (ret) return ret;
		}
	return 0;
}

void init_table(){
	if (init_flag) return;
	init_flag++;
	int a, b, c, d;
	static std::mt19937_64 rng;
	hash_table = new hash_t[HASH_SIZE];
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
	table_f[0][1][1][1][1][0] = SCORE_AL4;
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
	table_f[0][2][2][2][2][0] = SCORE_EL4;
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
}

int64_t eval_s(){
	hash_t* p = &hash_table[key % HASH_SIZE];
	if (p->key == key && p->app_s)
		return p->value;
	switch (eval_w()){
	case 1:
		record_hash(SCORE_WIN);
		return SCORE_WIN;
	case 2:
		record_hash(SCORE_LOSE);
		return SCORE_LOSE;
	default:
		break;
	}
	int x, y;
	int64_t score = SCORE_BASE | (int64_t)rand();
	for (x = 0; x < 15; x++)
		for (y = 0; y < 15; y++){
			if (!board[x][y]) continue;
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
		}
	record_hash(score);
	return score;
}

int64_t __fastcall alpha_beta(int64_t alpha, int64_t beta, int depth){
	int64_t reg;
	int x, y;
	int by, bx = 0xfe;
	int hy = 0xfe;
	int hx = 0xfe;
	hash_t* h = &hash_table[key % HASH_SIZE];
	if (depth<intelligence){
		/* Judge win / lose during recursive. */
		switch (eval_w()){
		case 1:
			return SCORE_WIN + intelligence - depth;
		case 2:
			return SCORE_LOSE + depth;
		default:
			break;
		}
		if (depth & 1){
			/* Depth is even: maximum. */
			if (h->key == key && h->app_m){
				hx = h->x;
				hy = h->y;
				board[hx][hy] = 1;
				key ^= zobrist[0][hx][hy];
				reg = alpha_beta(alpha, beta, depth + 1);
				board[hx][hy] = 0;
				key ^= zobrist[0][hx][hy];
				if (reg >= beta) return beta;
				if (reg > alpha){
					alpha = reg;
					bx = hx;
					by = hy;
				}
			}

			for (x = 0; x < 15; x++)
				for (y = 0; y < 15; y++){
					if (idle(x, y)) continue;
					if (x == hx && y == hy) continue;
					board[x][y] = 1;
					key ^= zobrist[0][x][y];
					reg = alpha_beta(alpha, beta, depth + 1);
					board[x][y] = 0;
					key ^= zobrist[0][x][y];
					if (reg >= beta){
						record_hash(x, y);
						return beta;
					}
					if (reg > alpha){
						alpha = reg;
						bx = x;
						by = y;
					}
				}
			if (bx != 0xfe) record_hash(bx, by);
			return alpha;
		}
		else{
			/* Depth is odd: minimum. */
			if (h->key == key && h->app_m){
				hx = h->x;
				hy = h->y;
				board[hx][hy] = 2;
				key ^= zobrist[1][hx][hy];
				reg = alpha_beta(alpha, beta, depth + 1);
				board[hx][hy] = 0;
				key ^= zobrist[1][hx][hy];
				if (reg <= alpha) return alpha;
				if (reg < beta){
					beta = reg;
					bx = hx;
					by = hy;
				}
			}

			for (x = 0; x < 15; x++)
				for (y = 0; y < 15; y++){
					if (idle(x, y)) continue;
					if (x == hx && y == hy) continue;
					board[x][y] = 2;
					key ^= zobrist[1][x][y];
					reg = alpha_beta(alpha, beta, depth + 1);
					board[x][y] = 0;
					key ^= zobrist[1][x][y];
					if (reg <= alpha){
						record_hash(x, y);
						return alpha;
					}
					if (reg < beta){
						beta = reg;
						bx = x;
						by = y;
					}
				}
			if (bx != 0xfe) record_hash(bx, by);
			return beta;
		}
	}else{
		/* Depth == intelligence: call evaluation function. */
		return eval_s();
	}
}

#define CPY(v) memcpy(board[(v)],mainboard[(v)],16);
#define DCPY(v) CPY(v)CPY((v)+1)CPY((v)+2)CPY((v)+3)

int msx[225];
int msy[225];
int msp;
std::mutex msl;

bool getmove(int& _x, int& _y){
	msl.lock();
	if (msp == 0){
		msl.unlock();
		return 0;
	}
	_x = msx[--msp];
	_y = msy[msp];
	msl.unlock();
	return 1;
}
void pushmove(int _x, int _y){
	msl.lock();
	msx[msp] = _x;
	msy[msp++] = _y;
	msl.unlock();
}


void thread_body(int x, int y){
	DCPY(0)DCPY(4)DCPY(8)DCPY(12);
	board[x][y] = 1;
	key = zobrist_key();
	int64_t reg = alpha_beta(SCORE_LOSE, SCORE_LOSE + intelligence, 0);
	if (reg == SCORE_LOSE + intelligence){
		int64_t reg = alpha_beta(SCORE_LOSE + intelligence, SCORE_WIN + intelligence, 0);
	}
	tlock.lock();
	if (reg > m || mx == 0xfe){
		m = reg;
		mx = x;
		my = y;
	}
	tlock.unlock();
}

int count_processor(){
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}

void ai_run(){
	int x, y;
	tid = 0;
	mx = 0xfe;
	m = INT64_MIN;
	init_table();
	//static const int ccpu = count_processor();
	for (x = 0; x < 15; x++)
		for (y = 0; y < 15; y++){
			if (mainidle(x, y)) continue;
			thm[tid++] = new std::thread(thread_body, x, y);
		}
	/*for (tid = 0; tid < ccpu; tid++){
		pthread_create(&thm[tid], NULL, &thread_body, NULL);
		SetThreadAffinityMask( pthread_getw32threadhandle_np(thm[tid]), 1 << tid);
	}*/
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
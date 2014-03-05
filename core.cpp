/*
	KalScope - A Gomoku AI Implement
	AI Core Module
	Copyright (C) 2014 AeanSR <http://aean.net/>

	Free to use, copy, modify or distribute. No warranty is given.
*/

#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"
#include "KalScope.h"

static const char codename_str[] = "AI Core Module \"Dolanaar\" 2014Mar.";
size_t goffset = 0;
char* init_str = NULL;
int init_finished = 0;

/* Search Depth. This macro is equal to max depth minus 1.  */
#define intelligence 6

//Some branch-less macros.
#define sshr32(v,d) (-(int32_t)((uint32_t)(v) >> d))
#define max32(x,y)  ((x) - (((x) - (y)) & sshr32((x) - (y), 31)))
#define min32(x,y)  ((y) + (((x) - (y)) & sshr32((x) - (y), 31)))
#define abs32(v)    (((v) ^ sshr32((v), 31)) - sshr32((v), 31))

//Evaluate score defination.
#define SCORE_WIN  ((int32_t)(1UL << 30))
#define SCORE_LOSE (-SCORE_WIN)
#define SCORE_MMASK ((int32_t)( 1UL << 10 ) - 1)
#define SCORE_BASE (0UL)

enum{
	TYPE_NON = 0, TYPE_PV = 1, TYPE_A = 2, TYPE_B = 3,
	EVALW_NON = 16, EVALW_WIN = 17, EVALW_LOSE = 18,
};
#define eval_stype(h) (0,((h)->type & 0xf))
#define eval_wtype(h) (0,((h)->type >> 4 ))

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
move_t msa[225];
int msp;
std::mutex msl;

//Structure for board representation.
char mainboard[16][16] = { 0 };
uint16_t __declspec(thread, align(16)) bitboard[2][16] = { 0 };
uint16_t __declspec(thread, align(16)) bitboard_mc[16] = { 0 };
uint16_t __declspec(thread, align(16)) bitboard_h[2][16] = { 0 };
uint16_t __declspec(thread, align(16)) bitboard_d[2][32] = { 0 };
uint16_t __declspec(thread, align(16)) bitboard_ad[2][32] = { 0 };

//Structure for evaluate lookup table.
static int32_t* table_f = NULL;
static int init_flag = 0;
int __declspec(thread, align(16)) subscript[16] = { 0 };
int __declspec(thread, align(16)) subscript_h[16] = { 0 };
int __declspec(thread, align(16)) subscript_d[32] = { 0 };
int __declspec(thread, align(16)) subscript_ad[32] = { 0 };
int __declspec(thread, align(16)) poweru3[16] = {
	1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683, 59049, 177147, 531441, 1594323, 4782969, 14348907
};

//Structure for result reduce.
int32_t m;
int my, mx = 0xfe;

//Structure for thread sync.
std::mutex tlock;
std::mutex hlock[1024];
std::thread* thm[225] = { NULL };
size_t tid = 0;
volatile size_t ltc = 0;
std::mutex ltclock;

//Structure for TT.
#define HASH_SIZE_DEFAULT (0x1000000)
size_t HASH_SIZE = HASH_SIZE_DEFAULT - 1;
static uint64_t zobrist[2][16][16] = { 0 };
uint64_t __declspec(thread) key = 0;
hash_t* hash_table;

// Get the available processor number.
int count_processor(){
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}
static const size_t ccpu = count_processor();

// Determine the size of TT.
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

// Make / Unmake a move on bit board.
void __forceinline bit_makemove(int x, int y, char color){
	bitboard[color - 1][x] |= 1 << y;
	subscript[x] += poweru3[y] << (color - 1);
	bitboard_h[color - 1][y] |= 1 << x;
	subscript_h[y] += poweru3[x] << (color - 1);
	bitboard_d[color - 1][14 - x + y] |= 1 << (x);
	subscript_d[14 - x + y] += poweru3[x] << (color - 1);
	bitboard_ad[color - 1][x + y] |= 1 << (14 - x);	
	subscript_ad[x + y] += poweru3[14 - x] << (color - 1);

}
void __forceinline bit_makemove(move_t& move, char color){
	bit_makemove(move.x, move.y, color);
}
void __forceinline bit_unmakemove(int x, int y, char color){
	bitboard[color - 1][x] &= ~(1 << y);
	subscript[x] -= poweru3[y] << (color - 1);
	bitboard_h[color - 1][y] &= ~(1 << x);
	subscript_h[y] -= poweru3[x] << (color - 1);
	bitboard_d[color - 1][14 - x + y] &= ~(1 << (x));
	subscript_d[14 - x + y] -= poweru3[x] << (color - 1);
	bitboard_ad[color - 1][x + y] &= ~(1 << (14 - x));
	subscript_ad[x + y] -= poweru3[14 - x] << (color - 1);
}
void __forceinline bit_unmakemove(move_t& move, char color){
	bit_unmakemove(move.x, move.y, color);
}

// Copy mainboard to bitboard.
void bit_copyboard(){
	int x, y;
	char b;
	memset(bitboard[0], 0, sizeof(uint16_t)* 16);
	memset(bitboard[1], 0, sizeof(uint16_t)* 16);
	memset(bitboard_h[0], 0, sizeof(uint16_t)* 16);
	memset(bitboard_h[1], 0, sizeof(uint16_t)* 16);
	memset(bitboard_d[0], 0, sizeof(uint16_t)* 16);
	memset(bitboard_d[1], 0, sizeof(uint16_t)* 16);
	memset(bitboard_ad[0], 0, sizeof(uint16_t)* 16);
	memset(bitboard_ad[1], 0, sizeof(uint16_t)* 16);
	memset(subscript, 0, sizeof(int)* 16);
	memset(subscript_h, 0, sizeof(int)* 16);
	memset(subscript_d, 0, sizeof(int)* 32);
	memset(subscript_ad, 0, sizeof(int)* 32);

	for (x = 0; x < 15; x++)
		for (y = 0; y < 15; y++){
			b = mainboard[x][y];
			if (b){
				bit_makemove(x, y, b);
			}
		}
}

// Cut the idle / occupied board out of move generator.
void bit_cutidle(){
	int x;
	memset(bitboard_mc, 0, sizeof(uint16_t)* 16);
	for (x = 1; x < 15; x++){
		bitboard_mc[x] |= 0x7fff & (bitboard[0][x - 1] | (bitboard[0][x - 1] << 1) | (bitboard[0][x - 1] >> 1));
		bitboard_mc[x] |= 0x7fff & (bitboard[1][x - 1] | (bitboard[1][x - 1] << 1) | (bitboard[1][x - 1] >> 1));
	}
	for (x = 0; x < 14; x++){
		bitboard_mc[x] |= 0x7fff & (bitboard[0][x + 1] | (bitboard[0][x + 1] << 1) | (bitboard[0][x + 1] >> 1));
		bitboard_mc[x] |= 0x7fff & (bitboard[1][x + 1] | (bitboard[1][x + 1] << 1) | (bitboard[1][x + 1] >> 1));
	}
	for (x = 0; x < 15; x++){
		bitboard_mc[x] |= 0x7fff & (bitboard[0][x] << 1) | (bitboard[0][x] >> 1);
		bitboard_mc[x] |= 0x7fff & (bitboard[1][x] << 1) | (bitboard[1][x] >> 1);
		bitboard_mc[x] &= ~bitboard[0][x];
		bitboard_mc[x] &= ~bitboard[1][x];
	}
}

// Generate hash key.
uint64_t zobrist_key(){
	int x, y;
	unsigned long c;
	uint64_t z = 0;
	for (x = 0; x < 15; x++){
		unsigned long mask = bitboard[0][x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;
			z ^= zobrist[0][x][y];
			mask &= mask - 1;
		}
		mask = bitboard[1][x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;
			z ^= zobrist[1][x][y];
			mask &= mask - 1;
		}
	}
	return z;
}

// Record a transpose item.
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
	p->type = (p->type & 0xf0) | type;
	p->depth = depth;
	hlock[key % 1024].unlock();
}

// Judge if win /lose.
char eval_w(hash_t* h = NULL){
	if (h){
		char type = eval_wtype(h) & 3;
		switch (type){
		case 0: break;
		default: return type - 1;
		}
	}
	char result = 0;
	int c;
	__m128i xmm1, xmm2, xmm3, xmm4;
	__m128i xmm0 = _mm_set1_epi16(0x1f);
	__m128i xmm7 = _mm_setzero_si128();
	for (c = 0; c < 2; c++){
		xmm1 = _mm_load_si128((__m128i*)&bitboard[c][0]);
		xmm3 = _mm_load_si128((__m128i*)&bitboard[c][8]);
		while (0x7fff & ~_mm_movemask_epi8(_mm_cmpeq_epi16(xmm7, _mm_or_si128(xmm1, xmm3)))){
			xmm2 = _mm_and_si128(xmm0, xmm1);
			xmm4 = _mm_and_si128(xmm0, xmm3);
			if (_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi16(xmm2, xmm0), _mm_cmpeq_epi16(xmm4, xmm0)))){
				result = c + 1;
				goto evalw_return;
			}
			xmm1 = _mm_srli_epi16(xmm1, 1);
			xmm3 = _mm_srli_epi16(xmm3, 1);
		}
		xmm1 = _mm_load_si128((__m128i*)&bitboard_h[c][0]);
		xmm3 = _mm_load_si128((__m128i*)&bitboard_h[c][8]);
		while (0x7fff & ~_mm_movemask_epi8(_mm_cmpeq_epi16(xmm7, _mm_or_si128(xmm1, xmm3)))){
			xmm2 = _mm_and_si128(xmm0, xmm1);
			xmm4 = _mm_and_si128(xmm0, xmm3);
			if (_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi16(xmm2, xmm0), _mm_cmpeq_epi16(xmm4, xmm0)))){
				result = c + 1;
				goto evalw_return;
			}
			xmm1 = _mm_srli_epi16(xmm1, 1);
			xmm3 = _mm_srli_epi16(xmm3, 1);
		}
		xmm1 = _mm_loadu_si128((__m128i*)&bitboard_d[c][4]);
		xmm3 = _mm_loadu_si128((__m128i*)&bitboard_ad[c][4]);
		while (0x7fff & ~_mm_movemask_epi8(_mm_cmpeq_epi16(xmm7, _mm_or_si128(xmm1, xmm3)))){
			xmm2 = _mm_and_si128(xmm0, xmm1);
			xmm4 = _mm_and_si128(xmm0, xmm3);
			if (_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi16(xmm2, xmm0), _mm_cmpeq_epi16(xmm4, xmm0)))){
				result = c + 1;
				goto evalw_return;
			}
			xmm1 = _mm_srli_epi16(xmm1, 1);
			xmm3 = _mm_srli_epi16(xmm3, 1);
		}
		xmm1 = _mm_loadu_si128((__m128i*)&bitboard_d[c][12]);
		xmm3 = _mm_loadu_si128((__m128i*)&bitboard_ad[c][12]);
		while (0x7fff & ~_mm_movemask_epi8(_mm_cmpeq_epi16(xmm7, _mm_or_si128(xmm1, xmm3)))){
			xmm2 = _mm_and_si128(xmm0, xmm1);
			xmm4 = _mm_and_si128(xmm0, xmm3);
			if (_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi16(xmm2, xmm0), _mm_cmpeq_epi16(xmm4, xmm0)))){
				result = c + 1;
				goto evalw_return;
			}
			xmm1 = _mm_srli_epi16(xmm1, 1);
			xmm3 = _mm_srli_epi16(xmm3, 1);
		}
		xmm1 = _mm_loadu_si128((__m128i*)&bitboard_d[c][20]);
		xmm3 = _mm_loadu_si128((__m128i*)&bitboard_ad[c][20]);
		while (0x7fff & ~_mm_movemask_epi8(_mm_cmpeq_epi16(xmm7, _mm_or_si128(xmm1, xmm3)))){
			xmm2 = _mm_and_si128(xmm0, xmm1);
			xmm4 = _mm_and_si128(xmm0, xmm3);
			if (_mm_movemask_epi8(_mm_or_si128(_mm_cmpeq_epi16(xmm2, xmm0), _mm_cmpeq_epi16(xmm4, xmm0)))){
				result = c + 1;
				goto evalw_return;
			}
			xmm1 = _mm_srli_epi16(xmm1, 1);
			xmm3 = _mm_srli_epi16(xmm3, 1);
		}
	}
evalw_return:
	if (h) h->type |= result + 1;
	return result;
}

// Judge if draw.
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

// Judge if game are not started yet.
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

// Another slower win judge, on mainboard.
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

// Initialize whole AI module.
void init_table(){
	if (init_flag) return;
	init_flag++;
	init_str = (char*)calloc(256,1);
	int a, b;
	static std::mt19937_64 rng;
	
	strcpy(init_str, "Constructing evaluation table ...");
	table_f = (int32_t*)_aligned_malloc(14348907 * sizeof(int32_t),16);
	if (table_f == NULL)
		exit(0);

	strcpy(init_str, "Reading evaluation table ...");
	gzFile in = gzopen("eval.ks", "rb");
	//FILE* in = fopen("eval.tbl", "rb");
	if (in == NULL){
		MessageBoxA(0, "Evaluation table \"eval.ks\" not found. Will terminate.\n"
			"(Have you extracted the archive?)", "Fatal error", 0);
		exit(0);
	}

	while (!gzeof(in)){
	//while (!feof(in)){
		goffset += gzread(in, ((char*)table_f) + goffset, 4096);
		//goffset += fread(((char*)table_f) + goffset, 1, 4096, in);
		sprintf(init_str, "Reading evaluation table (%d%%)...", goffset * 100 / (14348907 * sizeof(int32_t)));
	}
	
	gzclose(in);
	//fclose(in);

	strcpy(init_str, "Constructing transpose table ...");
	hash_table = (hash_t*)_aligned_malloc(sizeof(hash_t) * (HASH_SIZE + 1), 16);
	memset(hash_table, 0, sizeof(hash_t)*(HASH_SIZE + 1)); //Occupy memory. Avoid another kalscope process allocate hash table too large.
	for (a = 0; a < 15; a++)
		for (b = 0; b < 15; b++){
			zobrist[0][a][b] = rng();
			zobrist[1][a][b] = rng();
		}
	*init_str = '\0';
	init_finished++;
}

// Evaluation function.
int32_t eval_s(){
	int x;
	static const __m128i xmm0 = _mm_setzero_si128();
	int32_t score = SCORE_BASE;
	for (x = 0; x < 15; x++){
		score += table_f[subscript[x]];
		score += table_f[subscript_h[x]];
		score += table_f[subscript_d[x]];
		score += table_f[subscript_ad[x]];
	}
	for (x = 15; x < 30; x++){
		score += table_f[subscript_d[x]];
		score += table_f[subscript_ad[x]];
	}
	return score;
}

// A quick-sort algorithm.
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

int32_t __fastcall alpha_beta(int32_t alpha, int32_t beta, int depth, int who2move, int is_pv);

// Move generator.
int move_gen(move_t* movelist, hash_t* h, int color, int depth){
	int count = 0;
	int hx = 0xfe;
	int hy = 0xfe;
	int x, y;
	unsigned long c, mask;
	uint64_t k;
	hash_t* p;
	if (h->key == key && eval_stype(h) && h->x != 0xfe){
		hx = h->x;
		hy = h->y;
	}
	bit_cutidle();
	if(depth >= 2){
		for (x = 0; x < 15; x++){
			mask = bitboard_mc[x];
			while (mask){
				_BitScanForward(&c, mask);
				y = c;
				if ((x ^ hx) | (y ^ hy)) {
					k = key ^ zobrist[color - 1][x][y];
					p = &hash_table[k & HASH_SIZE];
					int64_t mkey = p->key;
					movelist[count].x = x;
					movelist[count].y = y;
					count++;
					mkey ^= k;
					mkey |= (-mkey);
					mkey = (uint64_t)mkey >> 63;
					mkey = -mkey;
					movelist[count].score = mkey & p->value;
				}
				else{
					movelist[count].score = INT32_MAX;
					movelist[count].x = x;
					movelist[count].y = y;
					count++;
				}
				mask &= mask - 1;
			}
		}
		move_sort(movelist, 0, count - 1);
	}
	else{
		for (x = 0; x < 15; x++){
			mask = bitboard_mc[x];
			while(mask){
				_BitScanForward(&c, mask);
				y = c;
				movelist[count].x = x;
				movelist[count].y = y;
				if ((x ^ hx) | (y ^ hy))
					movelist[count].score = 0;
				else{
					movelist[count].score = INT32_MAX;
					movelist[0].swap(movelist[count]);
				}
				count++;
				mask &= mask - 1;
			}
		}
	}

	return count;
}

// Fork a child thread if not all processor are busy.
int32_t fork_subthread(bool* ready, move_t move,
	uint16_t bb[][16], uint16_t bbh[][16], uint16_t bbd[][32], uint16_t bbad[][32],
	int ss[], int ssh[], int ssd[], int ssad[],
	uint64_t k, int32_t alpha, int32_t beta, int depth, int who2move)
{
	// Copy board from main.
	memcpy(bitboard[0], bb[0], 32);
	memcpy(bitboard[1], bb[1], 32);
	memcpy(bitboard_h[0], bbh[0], 32);
	memcpy(bitboard_h[1], bbh[1], 32);
	memcpy(bitboard_d[0], bbd[0], 64);
	memcpy(bitboard_d[1], bbd[1], 64);
	memcpy(bitboard_ad[0], bbad[0], 64);
	memcpy(bitboard_ad[1], bbad[1], 64);
	memcpy(subscript, ss, 64);
	memcpy(subscript_h, ssh, 64);
	memcpy(subscript_d, ssd, 128);
	memcpy(subscript_ad, ssad, 128);
	*ready = 1;

	// Set up.
	key = k;
	int32_t reg;
	char color = (who2move > 0 ? 1 : 2);
	int x = move.x;
	int y = move.y;

	// Make move.
	bit_makemove(x, y, color);
	key ^= zobrist[color - 1][x][y];

	// Search.
	reg = -alpha_beta(-alpha - 1, -alpha, depth - 1, -who2move, 0);
	if (reg > alpha && reg < beta)
		reg = -alpha_beta(-beta, -alpha, depth - 1, -who2move, 1);

	// Ready to return, decrease ltc.
	ltclock.lock();
	ltc--;
	ltclock.unlock();

	return reg;
}

// Alpha-beta search frame.
int32_t __fastcall alpha_beta(int32_t alpha, int32_t beta, int depth, int who2move, int is_pv){
	hash_t* h = &hash_table[key & HASH_SIZE];
	int32_t reg;
	int x, y;
	int by, bx = 0xfe;
	int hy = 0xfe;
	int hx = 0xfe;
	char color = (who2move > 0 ? 1 : 2);
	int alpha_raised = 0;
	bool found = 0;// (h->key == key);

	if (depth){
		// If TT returned a deeper history result, use it.
		if (found && h->depth >= depth){
			if (eval_stype(h) == TYPE_PV)
				return h->value;
			else if (eval_stype(h) == TYPE_B && h->value >= beta){
				return beta;
			}else if (eval_stype(h) == TYPE_A && h->value <= alpha)
				return alpha;
		}

		// Call eval_w().
		if (eval_w(found ? h : NULL))
			return alpha;

		// Generate all moves and sort.
		move_t mlist[225];
		int count = move_gen(mlist, h, color, depth);

		// Fork sub threads when appropriate.
		int forked_move = 0;
		std::future<int32_t> forked_return[16];

		// Search all moves.
		for (int i = 0; i < count; i++)
		{
			x = mlist[i].x;
			y = mlist[i].y;

			// If there's an idle CPU, try fork a sub thread.
			// Only fork PV node.
			if (is_pv && ltc < ccpu && depth > 3 && forked_move < 16){
				ltclock.lock();
				if (ltc < ccpu){
					ltc++;
					ltclock.unlock();
					bool ready = 0;
					forked_return[forked_move] = std::async(
						fork_subthread, &ready, mlist[i], bitboard, bitboard_h, bitboard_d, bitboard_ad,
						subscript, subscript_h, subscript_d, subscript_ad,
						key, alpha, beta, depth, who2move);
					forked_move++;

					// Wait for sub thread to make a board copy.
					while (!ready)
						std::this_thread::yield();
					continue;
				}
				else ltclock.unlock();
			}

			// Make move.
			bit_makemove(x, y, color);
			key ^= zobrist[color - 1][x][y];

			// Do principle variation search.
			if (!is_pv || depth < 3)
				reg = -alpha_beta(-beta, -alpha, depth - 1, -who2move, 0);
			else if (!alpha_raised)
				reg = -alpha_beta(-beta, -alpha, depth - 1, -who2move, 1);
			else{
				reg = -alpha_beta(-alpha - 1, -alpha, depth - 1, -who2move, 0);
				if (reg > alpha && reg < beta)
					reg = -alpha_beta(-beta, -alpha, depth - 1, -who2move, 1);
			}

			bit_unmakemove(x, y, color);
			key ^= zobrist[color - 1][x][y];
			if (reg >= beta){
				record_hash(beta, x, y, TYPE_B, depth);
				return beta;
			}
			if (reg > alpha){
				alpha_raised = 1;
				alpha = reg;
				bx = x;
				by = y;
			}
		}

		// At last, check sub threads' result if they could raise alpha.
		while (forked_move--){
			reg = forked_return[forked_move].get();
			if (reg >= beta){
				record_hash(beta, x, y, TYPE_B, depth);
				return beta;
			}
			if (reg > alpha){
				alpha_raised = 1;
				alpha = reg;
				bx = x;
				by = y;
			}
		}

		// Fail low.
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

// Get/put a move for multithread.
bool getmove(int& _x, int& _y, move_t** ptr = NULL){
	msl.lock();
	if (msp == 0){
		msl.unlock();
		return 0;
	}
	_x = msa[--msp].x;
	_y = msa[msp].y;
	if (ptr) *ptr = &msa[msp];
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

// Entry of threads.
void thread_body(int maxdepth){
	bit_copyboard();
	int x, y;
	int32_t localm;
	int32_t reg;
	move_t* mp;
	while (getmove(x, y, &mp)){
		bit_makemove(x, y, 1);
		key = zobrist_key();
		tlock.lock();
		localm = m;
		tlock.unlock();

		if (localm <= SCORE_LOSE)
			reg = -alpha_beta(-SCORE_WIN, -localm, maxdepth, -1, 1);
		else{
			reg = -alpha_beta(-localm - 1, -localm, maxdepth, -1, 0);
			if (reg > localm && reg < SCORE_WIN)
				reg = -alpha_beta(-SCORE_WIN, -localm, maxdepth, -1, 1);
		}

		tlock.lock();
		if (reg > m || mx == 0xfe){
			mp->score = -reg; // move_sort do descending, but we need a ascending sort.
			m = reg;
			mx = x;
			my = y;
		}
		tlock.unlock();
		bit_unmakemove(x, y, 1);
	}

}

// AI Entry, call this function when you want computer make a move.
void ai_run(){
	int x, y;

	// Initialize.
	tid = 0;
	mx = 0xfe;
	init_table();
	int mvcount = 0;
	int bx = 0xfe;
	int by = 0xfe;
	bit_copyboard();

	// Probe TT.
	uint64_t k = zobrist_key();
	hash_t* h = &hash_table[k & (HASH_SIZE - 1)];
	if (h->key == key && eval_stype(h)){
		bx = h->x;
		by = h->y;
		if (bx >= 15 || bx < 0 || by >= 15 || by < 0){
			bx = 0xfe;
			by = 0xfe;
		}
	}

	// See if we are in check.
	bit_cutidle();
	unsigned long c;
	for (x = 0; x < 15; x++){
		unsigned long mask = bitboard_mc[x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;
			bit_makemove(x, y, 1);
			if (eval_w()){
				mainboard[x][y] = 1;
				return;
			}
			bit_unmakemove(x, y, 1);
			mask &= mask - 1;
		}
	}
	for (x = 0; x < 15; x++){
		unsigned long mask = bitboard_mc[x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;
			bit_makemove(x, y, 2);
			if (eval_w()){
				mainboard[x][y] = 1;
				return;
			}
			bit_unmakemove(x, y, 2);
			mask &= mask - 1;
		}
	}
	// Generate all moves.
	for (x = 0; x < 15; x++){
		unsigned long mask = bitboard_mc[x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;

			if (x == bx&&y == by) continue;
			k ^= zobrist[0][x][y];
			hash_t* p = &hash_table[k & HASH_SIZE];
			// move_sort do descending, but we need a ascending sort.
			pushmove(x, y, -(p->key == k ? p->value : 0));
			k ^= zobrist[0][x][y];
			mvcount++;

			mask &= mask - 1;
		}
	}
	if (bx != 0xfe){
		mvcount++;
		pushmove(bx, by, INT32_MIN);
	}

	// Iterative deeping.
	int maxdepth = 0;
	while (maxdepth <= intelligence){
		msp = mvcount;
		move_sort(msa, 0, mvcount - 1);

		// Young Brother Waits.
		if (!getmove(x, y))
			return;
		mx = x;
		my = y;
		bit_makemove(x, y, 1);
		key ^= zobrist[0][x][y];
		ltc++;
		m = -alpha_beta(-SCORE_WIN, -SCORE_LOSE, maxdepth, -1, 1);
		bit_unmakemove(x, y, 1);
		key ^= zobrist[0][x][y];

		// Young brother start.
		ltc = 0;
		for (x = 0; x < ccpu; x++)
			thm[tid++] = new std::thread(thread_body, maxdepth);
		ltc = tid;

		// Reduce.
		do{
			--tid;
			ltclock.lock();
			--ltc;
			ltclock.unlock();
			if (thm[tid]){
				thm[tid]->join();
				delete thm[tid];
				thm[tid] = NULL;
			}
		} while (tid && tid < 256);

		// If win / lose were already determined, we don't need to search more.
		if (m >= SCORE_WIN || m <= SCORE_LOSE)
			break;
		maxdepth += 1;
	}

	// Make the actual move.
	mainboard[mx][my] = 1;
}

/*
	KalScope - A Gomoku AI Implement
	AI Kernel
	Copyright (C) 2014 AeanSR <http://aean.net/>

	KalScope is released under the terms of the MIT License. Free to use
	for any purpose, as long as this copyright notice is preserved.
*/

// Disable MSVC's annoying secure warnings.
#define _CRT_SECURE_NO_WARNINGS

#include "stdafx.h"
#include "KalScope.h"

// Preprocessed macros to control KS ability.
#define USE_TT	// Use transpose table or not
#define  USE_LOG	// Use debug log file or not

// State globals shared to interface.
static const char codename_str[] = "AI Kernel \"Dolanaar\" 2014Mar.";
size_t goffset = 0;
char* init_str = NULL;
int init_finished = 0;
char* debug_str;

// Time limit, in millisecond.
static const int time_limit = 10000;
volatile bool time_out = 0;
uint64_t node = 0;

// Some branch-less macros.
#define sshr32(v,d) (-(int32_t)((uint32_t)(v) >> d))
#define max32(x,y)  ((x) - (((x) - (y)) & sshr32((x) - (y), 31)))
#define min32(x,y)  ((y) + (((x) - (y)) & sshr32((x) - (y), 31)))
#define abs32(v)    (((v) ^ sshr32((v), 31)) - sshr32((v), 31))

// Evaluate score defination.
#define SCORE_WIN  ((int32_t)(1UL << 30))
#define SCORE_LOSE (-SCORE_WIN)
#define SCORE_MMASK ((int32_t)( 1UL << 10 ) - 1)
#define SCORE_BASE (0UL)

#if defined(USE_TT)
// Structure for TT.
enum{
	TYPE_NON = 0, TYPE_PV = 1, TYPE_A = 2, TYPE_B = 3,
};
typedef struct{
	uint64_t key;
	int32_t value;
	char x;
	char y;
	char type;
	char depth;
} hash_t;
#define HASH_SIZE_DEFAULT (0x1000000)
size_t HASH_SIZE = HASH_SIZE_DEFAULT - 1;
static uint64_t zobrist[2][16][16] = { 0 };
__declspec(thread) uint64_t key = 0;
hash_t* hash_table;
#endif

// Structure for movegen / movesort / movestack.
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

// Structure for board representation.
char mainboard[16][16] = { 0 };
__declspec(thread, align(16)) uint16_t bitboard[2][16] = { 0 };
__declspec(thread, align(16)) uint16_t bitboard_mc[16] = { 0 };

// Structure for evaluate lookup table.
static int32_t* table_f = NULL;
static int init_flag = 0;
__declspec(thread) int32_t incremental_eval = SCORE_BASE;
__declspec(thread) int32_t incremental_win = 0;
__declspec(thread) int32_t backup_incwin = 0;
__declspec(thread, align(16)) int subscript[16] = { 0 };
__declspec(thread, align(16)) int subscript_h[16] = { 0 };
__declspec(thread, align(16)) int subscript_d[32] = { 0 };
__declspec(thread, align(16)) int subscript_ad[32] = { 0 };
static int poweru3[16] = {
	1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683, 59049, 177147, 531441, 1594323, 4782969, 14348907
};

// Structure for result reduce.
int32_t m;
int my, mx = 0xfe;
int maxdepth = 0;

// Structure for thread sync.
std::mutex tlock;
std::mutex hlock[1024];
std::thread* thm[225] = { NULL };
size_t tid = 0;
volatile size_t ltc = 0;
std::mutex ltclock;

// Get the available processor number.
int count_processor(){
	SYSTEM_INFO info;
	GetSystemInfo(&info);
	return info.dwNumberOfProcessors;
}
static const size_t ccpu = count_processor();

// Determine the size of TT.
#if defined(USE_TT)
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
#endif

// Make / Unmake a move on bit board, and host the incremental evalutation.
void __forceinline bit_makemove(int x, int y, char color, bool need_b, bool need_e, bool need_w){
	/*
		Since this is a force inlined function, the parameters 'need_x' ( if given constant )
		indicate which 'if' blocks can be ignored at compile time. No actural branches.
		If you give some variant value, the code will come up with branches. You should try
		to rewrite this function as template / override approach.
	*/
	if (need_e){
		incremental_eval -= table_f[subscript[x]];
		incremental_eval -= table_f[subscript_h[y]];
		incremental_eval -= table_f[subscript_d[14 - x + y]];
		incremental_eval -= table_f[subscript_ad[x + y]];
	}
	if (need_e || need_w){
		subscript[x] += poweru3[y] << (color - 1);
		subscript_h[y] += poweru3[x] << (color - 1);
		subscript_d[14 - x + y] += poweru3[x] << (color - 1);
		subscript_ad[x + y] += poweru3[x] << (color - 1);
	}
	if (need_b){
		bitboard[color - 1][x] |= 1 << y;
	}
	if (need_e){
		incremental_eval += table_f[subscript[x]];
		incremental_eval += table_f[subscript_h[y]];
		incremental_eval += table_f[subscript_d[14 - x + y]];
		incremental_eval += table_f[subscript_ad[x + y]];
	}
	if (need_w){
		backup_incwin = incremental_win;
		incremental_win = incremental_win || (table_f[subscript[x]] & 0x7fffffff) == SCORE_WIN || (table_f[subscript_h[y]] & 0x7fffffff) == SCORE_WIN
			|| (table_f[subscript_d[14 - x + y]] & 0x7fffffff) == SCORE_WIN || (table_f[subscript_ad[x + y]] & 0x7fffffff) == SCORE_WIN;

	}
}
void __forceinline bit_makemove(move_t& move, char color, bool need_b, bool need_e, bool need_w){
	bit_makemove(move.x, move.y, color, need_b, need_e, need_w);
}
void __forceinline bit_unmakemove(int x, int y, char color, bool need_b, bool need_e, bool need_w){
	if (need_e){
		incremental_eval -= table_f[subscript[x]];
		incremental_eval -= table_f[subscript_h[y]];
		incremental_eval -= table_f[subscript_d[14 - x + y]];
		incremental_eval -= table_f[subscript_ad[x + y]];
	}
	if (need_e||need_w){
		subscript[x] -= poweru3[y] << (color - 1);
		subscript_h[y] -= poweru3[x] << (color - 1);
		subscript_d[14 - x + y] -= poweru3[x] << (color - 1);
		subscript_ad[x + y] -= poweru3[x] << (color - 1);
	}
	if (need_b){
		bitboard[color - 1][x] &= ~(1 << y);
	}
	if (need_e){
		incremental_eval += table_f[subscript[x]];
		incremental_eval += table_f[subscript_h[y]];
		incremental_eval += table_f[subscript_d[14 - x + y]];
		incremental_eval += table_f[subscript_ad[x + y]];
	}
	if (need_w){
		incremental_win = backup_incwin;
	}
}
void __forceinline bit_unmakemove(move_t& move, char color, bool need_b, bool need_e, bool need_w){
	bit_unmakemove(move.x, move.y, color, need_b, need_e, need_w);
}

// Evaluation function ( Only called on start ).
void eval_s(){
	int x;
	static const __m128i xmm0 = _mm_setzero_si128();
	int32_t score = SCORE_BASE;
	incremental_win = 0;
	for (x = 0; x < 15; x++){
		score += table_f[subscript[x]];
		incremental_win |= abs32(table_f[subscript[x]]) >= SCORE_WIN ;
		score += table_f[subscript_h[x]];
		incremental_win |= abs32(table_f[subscript_h[x]]) >= SCORE_WIN;
		score += table_f[subscript_d[x]];
		incremental_win |= abs32(table_f[subscript_d[x]]) >= SCORE_WIN;
		score += table_f[subscript_ad[x]];
		incremental_win |= abs32(table_f[subscript_ad[x]]) >= SCORE_WIN;
	}
	for (x = 15; x < 30; x++){
		score += table_f[subscript_d[x]];
		incremental_win |= abs32(table_f[subscript_d[x]]) >= SCORE_WIN;
		score += table_f[subscript_ad[x]];
		incremental_win |= abs32(table_f[subscript_ad[x]]) >= SCORE_WIN;
	}
	incremental_eval = score;
}

// Copy mainboard to bitboard.
void bit_copyboard(){
	int x, y;
	char b;
	memset(bitboard[0], 0, sizeof(uint16_t)* 16);
	memset(bitboard[1], 0, sizeof(uint16_t)* 16);
	memset(subscript, 0, sizeof(int)* 16);
	memset(subscript_h, 0, sizeof(int)* 16);
	memset(subscript_d, 0, sizeof(int)* 32);
	memset(subscript_ad, 0, sizeof(int)* 32);
	incremental_eval = SCORE_BASE;
	incremental_win = 0;

	for (x = 0; x < 15; x++)
		for (y = 0; y < 15; y++){
			b = mainboard[x][y];
			if (b){
				bit_makemove(x, y, b, 1, 1, 1);
			}
		}
}

// Cut the idle / occupied board out of move generator.
void bit_cutidle(){

	/*
		To somebody who want to know how KS works:

		SIMD code is hard to read for programmer, but (at most time) significantly faster.
		You don't need to be sure about how the extremely weired code below works, if you aren't.
		Just remember what it does and use it as a library function, since once such a function
		is written and tuned, there is almost no reasons to modify it.
		Same to other SSE2 version functions.

		This will help you better understand:
		_mm_<func>_<domain>
			srli = shift, right, logical, integral
			slli = shift, left , logical, integral
			si128 : signed integral 128 bit, the whole XMM register.
			epi16 : treat whole XMM register as 8 * 16bit signed integral.
	*/
	
	__m128i xmm2, xmm3, xmm4, xmml, xmmr;
	const __m128i xmmmask = _mm_set1_epi16(0x7fff);

	xmml = _mm_or_si128(_mm_load_si128((__m128i*)&bitboard[0][0]), _mm_load_si128((__m128i*)&bitboard[1][0]));
	xmmr = _mm_or_si128(_mm_load_si128((__m128i*)&bitboard[0][8]), _mm_load_si128((__m128i*)&bitboard[1][8]));

	xmm3 = _mm_or_si128(_mm_srli_epi16(xmml, 1), _mm_slli_epi16(xmml, 1));
	xmm2 = _mm_srli_si128(xmml, 2);
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm3 = _mm_or_si128(xmm3, xmm2);
	xmm2 = _mm_slli_si128(xmml, 2);
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm3 = _mm_or_si128(xmm3, xmm2);
	xmm2 = _mm_srli_si128(xmml, 14);
	xmm4 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	
	xmm4 = _mm_or_si128(xmm4, _mm_or_si128(_mm_srli_epi16(xmmr, 1), _mm_slli_epi16(xmmr, 1)));
	xmm2 = _mm_srli_si128(xmmr, 2);
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm4 = _mm_or_si128(xmm4, xmm2);
	xmm2 = _mm_slli_si128(xmmr, 2);
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm4 = _mm_or_si128(xmm4, xmm2);
	xmm2 = _mm_slli_si128(xmmr, 14);
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm3 = _mm_or_si128(xmm3, xmm2);
	
	xmm4 = _mm_and_si128(xmm4, xmmmask);
	xmm3 = _mm_and_si128(xmm3, xmmmask);
	
	/*
	**
		This comment block is an expansion from 3*3 nei-cut to 5*5 nei-cut.
		It is helpful when a threaten is outside KS's 3*3 scope, but it will ruin the start phase.
		So it is disabled yet. If you made a 4-3 threaten and KS failed to answer, it's a known issue,
		which should get fixed in next release.
		This block has bug. Tune it before use it.
	*
	__m128i xmm0;
	xmm2 = _mm_srli_si128(xmm4, 2);
	xmm0 = _mm_slli_si128(xmm4, 2);
	xmm4 = _mm_or_si128(xmm4, _mm_or_si128(_mm_srli_epi16(xmm4, 1), _mm_slli_epi16(xmm4, 1)));
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm0 = _mm_or_si128(xmm0, _mm_or_si128(_mm_srli_epi16(xmm0, 1), _mm_slli_epi16(xmm0, 1)));
	xmm4 = _mm_or_si128(xmm4, _mm_or_si128(xmm0, xmm2));
	xmm2 = _mm_srli_si128(xmm4, 14);
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm3 = _mm_or_si128(xmm3, xmm2);

	xmm2 = _mm_srli_si128(xmm3, 2);
	xmm0 = _mm_slli_si128(xmm3, 2);
	xmm3 = _mm_or_si128(xmm3, _mm_or_si128(_mm_srli_epi16(xmm3, 1), _mm_slli_epi16(xmm3, 1)));
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm0 = _mm_or_si128(xmm0, _mm_or_si128(_mm_srli_epi16(xmm0, 1), _mm_slli_epi16(xmm0, 1)));
	xmm3 = _mm_or_si128(xmm3, _mm_or_si128(xmm0, xmm2));
	xmm2 = _mm_srli_si128(xmm3, 14);
	xmm2 = _mm_or_si128(xmm2, _mm_or_si128(_mm_srli_epi16(xmm2, 1), _mm_slli_epi16(xmm2, 1)));
	xmm4 = _mm_or_si128(xmm4, xmm2);
	
	xmm3 = _mm_and_si128(xmm3, xmmmask);
	xmm4 = _mm_and_si128(xmm4, xmmmask);
	*/

	xmm3 = _mm_andnot_si128(xmml, xmm3);
	xmm4 = _mm_andnot_si128(xmmr, xmm4);

	_mm_store_si128((__m128i*)&bitboard_mc[0], xmm3);
	_mm_store_si128((__m128i*)&bitboard_mc[8], xmm4);
}

// Generate hash key.
#if defined(USE_TT)
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
#endif

// Record a transpose item.
#if defined(USE_TT)
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
#endif

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
	static std::mt19937_64 rng;
	
	strcpy(init_str, "Constructing evaluation table ...");
	table_f = (int32_t*)_aligned_malloc(14348907 * sizeof(int32_t),16);
	if (table_f == NULL)
		exit(0);

	strcpy(init_str, "Reading evaluation table ...");
	gzFile in = gzopen("eval.ks", "rb");
	if (in == NULL){
		MessageBoxA(0, "Evaluation table \"eval.ks\" not found. Will terminate.\n"
			"(Have you extracted the archive?)", "Fatal error", 0);
		exit(0);
	}
	while (!gzeof(in)){
		goffset += gzread(in, ((char*)table_f) + goffset, 4096);
		sprintf(init_str, "Reading evaluation table (%d%%)...", goffset * 100 / (14348907 * sizeof(int32_t)));
	}
	gzclose(in);
#if defined(USE_TT)
	int a, b;
	strcpy(init_str, "Constructing transpose table ...");
	HASH_SIZE = memory_to_use() - 1;
	hash_table = (hash_t*)_aligned_malloc(sizeof(hash_t) * (HASH_SIZE + 1), 16);
	memset(hash_table, 0, sizeof(hash_t)*(HASH_SIZE + 1)); //Occupy memory. Avoid another kalscope process allocate hash table too large.
	for (a = 0; a < 15; a++)
		for (b = 0; b < 15; b++){
			zobrist[0][a][b] = rng();
			zobrist[1][a][b] = rng();
		}
#endif
	*init_str = '\0';
	init_finished++;
}

// A quick-sort algorithm, nothing special.
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
#if defined(USE_TT)
int move_gen(move_t* movelist, hash_t* h, int color, int depth){
	hash_t* p;
	int hx = 0xfe;
	int hy = 0xfe;
	uint64_t k;

	// Probe TT to see if a best move was recorded.
	if (h && h->key == key && h->type && h->x != 0xfe){
		hx = h->x;
		hy = h->y;
		// If the space is occupied, ignore it.
		if ((bitboard[0][hx] | bitboard[1][hx]) & (1 << hy)){
			hx = 0xfe;
			hy = 0xfe;
		}
	}
#else
int move_gen(move_t* movelist, int color, int depth){
#endif
	int count = 0;
	int x, y;
	unsigned long c, mask;
	int32_t score = incremental_eval;
	int32_t val = 0;

	bit_cutidle();

	// Do threat prune and move sort if depth >= 2.
	if(depth >= 2){
		bool lookfor_threat = 1;
		look_again:
		for (x = 0; x < 15; x++){
			mask = bitboard_mc[x];
			while (mask){
				_BitScanForward(&c, mask);
				y = c;
				if (lookfor_threat){
					bit_makemove(x, y, color, 0, 1, 0);
					val = incremental_eval;
					bit_unmakemove(x, y, color, 0, 1, 0);
					// If this move do not make a difference, ignore it.
					if (abs32(val - score) == 0){
						mask &= mask - 1;
						continue;
					}
				}
#if defined(USE_TT)
				if ((x ^ hx) | (y ^ hy)) {
					if (depth < 4){
#endif
						movelist[count].x = x;
						movelist[count].y = y;
						movelist[count].score = val;
						count++;
#if defined(USE_TT)
					}else{
						// Try probe a history evaluation for move sorting.
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
				}
				else{
					// TT gives a best move, it should be first searched.
					movelist[count].score = INT32_MAX;
					movelist[count].x = x;
					movelist[count].y = y;
					count++;
				}
#endif
				mask &= mask - 1;
			}
		}
		// If no move could make a difference, generate all possible moves.
		if (lookfor_threat && count == 0){
			lookfor_threat = 0;
			goto look_again;
		}
		else if (count == 0)
				return 0;
		// Sort.
		move_sort(movelist, 0, count - 1);
	}
	else{
		// For depth = 1, disable some movegen ability so movegen won't consume much CPU time. 
		for (x = 0; x < 15; x++){
			mask = bitboard_mc[x];
			while(mask){
				_BitScanForward(&c, mask);
				y = c;
				movelist[count].x = x;
				movelist[count].y = y;
#if defined(USE_TT)
				if ((x ^ hx) | (y ^ hy))
#endif
					movelist[count].score = 0;
#if defined(USE_TT)
				else{
					movelist[count].score = INT32_MAX;
					movelist[0].swap(movelist[count]);
				}
#endif
				count++;
				mask &= mask - 1;
			}
		}
	}

	return count;
}

// Fork a child thread if not all processor are busy.
int32_t fork_subthread(bool* ready, move_t move,
	uint16_t bb[][16], int32_t incw, int32_t ince, int32_t bkincw,
	int ss[], int ssh[], int ssd[], int ssad[],
	uint64_t k, int32_t alpha, int32_t beta, int depth, int who2move)
{
	// Copy board from main.
	memcpy(bitboard[0], bb[0], 32);
	memcpy(bitboard[1], bb[1], 32);
	memcpy(subscript, ss, 64);
	memcpy(subscript_h, ssh, 64);
	memcpy(subscript_d, ssd, 128);
	memcpy(subscript_ad, ssad, 128);
	*ready = 1;

	// Set up.
	int32_t reg;
	incremental_eval = ince;
	incremental_win = incw;
	backup_incwin = bkincw;
	char color = (who2move > 0 ? 1 : 2);
	int x = move.x;
	int y = move.y;
#if defined(USE_TT)
	key = k ^ zobrist[color - 1][x][y];
#endif

	// Make move.
	bit_makemove(x, y, color, 1, 1, 1);

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
	// If time is up, give up.
	if (time_out)
		return 0;
	
	// Initialize.
	int32_t reg;
	int x, y;
	int by, bx = 0xfe;
	char color = (who2move > 0 ? 1 : 2);
	int alpha_raised = 0;
	bool ready = 0;

	node++;

	if (depth == 0){

		/* Depth == 0: call evaluation function. */
		reg = who2move * incremental_eval;
		return reg;

	}else{
#if defined(USE_TT)
		bool found = 0;
		hash_t* h = NULL;

		if (depth > 1){
			// Probe TT. We don't use stream load since the recently probed TT may hit again soon.
			h = &hash_table[key & HASH_SIZE];
			hash_t __declspec(align(16)) tt;
			register __m128i xmm = _mm_load_si128((__m128i*)h);
			h = &tt;
			_mm_store_si128((__m128i*)h, xmm);
			found = (h->key == key);
		}

		// If TT returned a deeper history result, use it.
		if (found && h->depth >= depth){
			if (h->type == TYPE_PV)
				return h->value;
			else if (h->type == TYPE_B && h->value >= beta){
				return beta;
			}else if (h->type == TYPE_A && h->value <= alpha)
				return alpha;
		}
#endif

		// Call eval_w().
		if (incremental_win)
			return alpha;

		// Generate all moves and sort.
		move_t mlist[225];
#if defined(USE_TT)
		int count = move_gen(mlist, h, color, depth);
#else
		int count = move_gen(mlist, color, depth);
#endif

		// If move generator suggested stop at here, return a evaluation.
		if (count == 0){
			reg = who2move * incremental_eval;
			return reg;
		}

		// Fork sub threads when appropriate.
		int forked_move = 0;
		std::future<int32_t> forked_return[16];

		// Search all moves.
		for (int i = 0; i < count; i++)
		{
			x = mlist[i].x;
			y = mlist[i].y;

			// If there's an idle CPU, try fork a sub thread.
			if (is_pv && i >= 1 && ltc < ccpu && depth > 6 && i < count - ltc && forked_move < 16){
				ltclock.lock();
				if (ltc < ccpu){
					ltc++;
					ltclock.unlock();
					ready = 0;
					forked_return[forked_move] = std::async(
						fork_subthread, &ready, mlist[i], bitboard,
						incremental_win, incremental_eval, backup_incwin,
						subscript, subscript_h, subscript_d, subscript_ad,
#if defined(USE_TT)
						key, alpha, beta, depth, who2move);
#else
						0, alpha, beta, depth, who2move);
#endif
					forked_move++;

					// Wait for sub thread to make a board copy.
					while (!ready)
						std::this_thread::yield();
					continue;
				}
				else ltclock.unlock();
			}

			// Make move.
			bit_makemove(x, y, color, 1, 1, 1);
#if defined(USE_TT)
			key ^= zobrist[color - 1][x][y];
#endif
			// Do principle variation search.
			if (!is_pv || depth < 3)
				reg = -alpha_beta(-beta, -alpha, depth - 1, -who2move, 0);
			else if (!alpha_raised)
				reg = -alpha_beta(-beta, -alpha, depth - 1, -who2move, 1);
			else{
				reg = -alpha_beta(-alpha - 1, -alpha + 1, depth - 1, -who2move, 0);
				if (reg > alpha && reg < beta)
					reg = -alpha_beta(-beta, -alpha, depth - 1, -who2move, 1);
			}

			// Unmake Move.
			bit_unmakemove(x, y, color, 1, 1, 1);
#if defined(USE_TT)
			key ^= zobrist[color - 1][x][y];
#endif
			if (reg >= beta){
#if defined(USE_TT)
				record_hash(beta, x, y, TYPE_B, depth);
#endif
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
#if defined(USE_TT)
				record_hash(beta, x, y, TYPE_B, depth);
#endif
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
#if defined(USE_TT)
		if (bx != 0xfe) record_hash(alpha, bx, by, TYPE_PV, depth);
		else record_hash(alpha, 0xfe, 0xfe, TYPE_A, depth);
#endif
		return alpha;
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
		bit_makemove(x, y, 1, 1, 1, 1);
#if defined(USE_TT)
		key = zobrist_key();
#endif
		tlock.lock();
		localm = m;
		tlock.unlock();

		if (localm <= SCORE_LOSE)
			reg = -alpha_beta(-SCORE_WIN, -localm + 1, maxdepth, -1, 1);
		else{
			reg = -alpha_beta(-localm - 1, -localm + 1, maxdepth, -1, 0);
			if (reg > localm && reg < SCORE_WIN)
				reg = -alpha_beta(-SCORE_WIN, -localm - 1, maxdepth, -1, 1);
		}
		
		if (time_out)
			return;

#if defined(USE_LOG)
		char* app = (char*)alloca(256);
		sprintf(app, "move(%d,%d), maxd %d, score %08X\n", x, y, maxdepth, reg);
		strcat(debug_str, app);
#endif

		mp->score = -reg; // move_sort do descending, but we need a ascending sort.

		tlock.lock();
		if (reg > m){
			m = reg;
			mx = x;
			my = y;
		}
		tlock.unlock();
		bit_unmakemove(x, y, 1, 1, 1, 1);
	}

}

// A stopwatch controls max search time.
void thread_timer(void){
	std::chrono::milliseconds dur(time_limit);
	std::this_thread::sleep_for(dur);
	time_out = 1;
}

// AI Entry, call this function when you want computer make a move.
void ai_run(){
	int x, y;

	// Initialize.
	tid = 0;
	time_out = 0;
	mx = 0xfe;
	msp = 0;
	node = 0;
	init_table();
	int mvcount = 0;
	int bx = 0xfe;
	int by = 0xfe;
	bit_copyboard();

#if defined(USE_LOG)
	debug_str = (char*)calloc(1,534288);
#endif

#if defined(USE_TT)
	// Probe TT.
	uint64_t k = zobrist_key();
	hash_t* h = &hash_table[k & (HASH_SIZE - 1)];
	if (h->key == key && h->type){
		bx = h->x;
		by = h->y;
		if (bx >= 15 || bx < 0 || by >= 15 || by < 0 || mainboard[bx][by] != 0){
			bx = 0xfe;
			by = 0xfe;
		}
	}
#endif
	// See if we are in check.
	bit_cutidle();
	unsigned long c;
	for (x = 0; x < 15; x++){
		unsigned long mask = bitboard_mc[x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;
			bit_makemove(x, y, 1, 0, 0, 1);
			if (incremental_win){
				mainboard[x][y] = 1;
				return;
			}
			bit_unmakemove(x, y, 1, 0, 0, 1);
			mask &= mask - 1;
		}
	}
	for (x = 0; x < 15; x++){
		unsigned long mask = bitboard_mc[x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;
			bit_makemove(x, y, 2, 0, 0, 1);
			if (incremental_win){
				mainboard[x][y] = 1;
				return;
			}
			bit_unmakemove(x, y, 2, 0, 0, 1);
			mask &= mask - 1;
		}
	}
	// Generate all moves.
	for (x = 0; x < 15; x++){
		unsigned long mask = bitboard_mc[x];
		while (mask){
			_BitScanForward(&c, mask);
			y = c;
#if defined(USE_TT)
			if (x == bx&&y == by) continue;
			k ^= zobrist[0][x][y];
			hash_t* p = &hash_table[k & HASH_SIZE];
			// move_sort do descending, but we need a ascending sort.
			pushmove(x, y, -(p->key == k ? p->value : 0));
			k ^= zobrist[0][x][y];
#else
			pushmove(x, y, 0);
#endif
			mvcount++;

			mask &= mask - 1;
		}
	}
	if (bx != 0xfe){
		mvcount++;
		pushmove(bx, by, INT32_MIN);
	}

	// Set up a timer.
	std::thread* timer = new std::thread(thread_timer);
	timer->detach();

	// Iterative deeping.
	maxdepth = 0;
	while (!time_out){
		msp = mvcount;
		move_sort(msa, 0, mvcount - 1);

		// Young Brother Waits.
		move_t* mp;
		if (!getmove(x, y, &mp))
			return;
		mx = x;
		my = y;
		bit_makemove(x, y, 1, 1, 1, 1);
#if defined(USE_TT)
		key ^= zobrist[0][x][y];
#endif
		ltc = 1;
		m = -alpha_beta(-SCORE_WIN, -SCORE_LOSE, maxdepth, -1, 1);

#if defined(USE_LOG)
		char* app = (char*)alloca(256);
		for (int dei = 0; dei < mvcount; dei++){
			sprintf(app, "SORT (%d,%d) - %08X\n", msa[dei].x, msa[dei].y, msa[dei].score);
			strcat(debug_str, app);
		}
		sprintf(app, "OLDBRO move(%d,%d), maxd %d, score %08X\n", x, y, maxdepth, m);
		strcat(debug_str, app);
#endif

		mp->score = -m;
		bit_unmakemove(x, y, 1, 1, 1, 1);
#if defined(USE_TT)
		key ^= zobrist[0][x][y];
#endif
		// Young brother starts.
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

	TerminateThread(timer->native_handle(), 0);
	delete timer;

#if defined(USE_LOG)
	char* app = (char*)alloca(256);
	sprintf(app, "choose move(%d,%d).\n", mx, my);
	strcat(debug_str, app);

	FILE* f = fopen("debuglog.txt", "w");
	fprintf(f, debug_str);
	delete debug_str;
	fclose(f);
#endif

	// Make the actual move.
	mainboard[mx][my] = 1;
}

#pragma once

#include <array>
#include <string>
#include <immintrin.h>

/*
	Typedefs
*/

#define str_lower(x) std::transform(x.begin(), x.end(), x.begin(), [](char c) { return static_cast<char>(std::tolower(c)); });

#define vmirror _byteswap_uint64
#define popcnt _mm_popcnt_u64
#define bsf (U8)std::countr_zero
#define bsr std::countl_zero
#define pext _pext_u64
#define pdep _pdep_u64

typedef uint64_t U64;
typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t U8;

typedef int64_t I64;
typedef int32_t I32;
typedef int16_t I16;
typedef int8_t I8;

typedef std::array<U64, 64> BoardArray;

/*
	Bitboard accessors
*/

enum pieceID : U8 {
	WHITE_PAWNS_ID = 0,
	BLACK_PAWNS_ID = 1,

	WHITE_KNIGHTS_ID = 2,
	BLACK_KNIGHTS_ID = 3,

	WHITE_BISHOPS_ID = 4,
	BLACK_BISHOPS_ID = 5,

	WHITE_ROOKS_ID = 6,
	BLACK_ROOKS_ID = 7,

	WHITE_KING_ID = 8,
	BLACK_KING_ID = 9,

	WHITE_QUEENS_ID = 10,
	BLACK_QUEENS_ID = 11,

	EMPTY_ID = 12,

	WHITE_PIECES_ID = 13,
	BLACK_PIECES_ID = 14,
	ALL_PIECES_ID = 15,
};

/*
	General
*/

constexpr int WHITE = 0;
constexpr int BLACK = 1;

constexpr std::string indexToString(U8 index) {
	char column = (index & 7) + 97;
	char row = (index >> 3) + 49;
	return { column, row };
}

struct Move {
	U16 data;

	Move() : data(0ULL) {};
	Move(U16 passed_data) : data(passed_data) {}
	Move(U8 from, U8 to) : data((from << 6) | to) {}
	Move(U8 from, U8 to, U8 promotion_type) : data((promotion_type << 12) | (from << 6) | to) {}

	constexpr U8 from() { return (data >> 6) & 0b00111111; }
	constexpr U8 to() { return data & 0b00111111; }

	constexpr U8 promotion() { return (data >> 12); }

	bool operator==(Move const& other) const { return data == other.data; }

	constexpr std::string promotionType(U8 type) {
		constexpr char promoTypes[12] = { ' ', ' ', 'N', 'n', 'B', 'b', 'R', 'r', 'K', 'k', 'Q', 'q' };
		return { promoTypes[type] };
	}

	constexpr std::string toString() {
		return indexToString(from()) + indexToString(to()) + promotionType(promotion());
	}

	constexpr U8 promotion_from_string(std::string mv, U8 turn) {
		if (mv.size() == 4 || mv[4] == ' ') return 0;
		constexpr U8 promoID[6] = { WHITE_BISHOPS_ID, WHITE_QUEENS_ID, WHITE_ROOKS_ID, 0, 0, WHITE_KNIGHTS_ID };
		U8 promo_index = (std::tolower(mv[4]) - 98) % 7;
		return promoID[promo_index] + turn;
	}

	Move(std::string mv) {
		U8 from_int = (mv[0] - 97) + (mv[1] - 49) * 8;
		U8 to_int = (mv[2] - 97) + (mv[3] - 49) * 8;
		data = (from_int << 6) | to_int;
	}

	// Move construction with support from promotion
	Move(std::string mv, U8 turn) {
		U8 from_int = (mv[0] - 97) + (mv[1] - 49) * 8;
		U8 to_int = (mv[2] - 97) + (mv[3] - 49) * 8;
		U8 promotion_type = promotion_from_string(mv, turn);
		data = (from_int << 6) | to_int | (promotion_type << 12);
	}
};

std::string bbStr(U64 bb);
std::vector<std::string> splitString(std::string str, char splitter);

/*
	Ranks and Files
*/

constexpr U64 RANK_1 = 0x00000000000000FF;
constexpr U64 RANK_2 = 0x000000000000FF00;
constexpr U64 RANK_3 = 0x0000000000FF0000;
constexpr U64 RANK_4 = 0x00000000FF000000;
constexpr U64 RANK_5 = 0x000000FF00000000;
constexpr U64 RANK_6 = 0x0000FF0000000000;
constexpr U64 RANK_7 = 0x00FF000000000000;
constexpr U64 RANK_8 = 0xFF00000000000000;

constexpr U64 FILE_A = 0x0101010101010101;
constexpr U64 FILE_B = 0x0202020202020202;
constexpr U64 FILE_C = 0x0404040404040404;
constexpr U64 FILE_D = 0x0808080808080808;
constexpr U64 FILE_E = 0x1010101010101010;
constexpr U64 FILE_F = 0x2020202020202020;
constexpr U64 FILE_G = 0x4040404040404040;
constexpr U64 FILE_H = 0x8080808080808080;

constexpr std::array<U64, 8> RANKS = { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8 };
constexpr std::array<U64, 8> FILES = { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };

/*
	Diagonals and Anti-Diagonals
*/

constexpr U64 MAIN_DIAG = 0x8040201008040201;
constexpr U64 ANTI_MAIN_DIAG = 0x102040810204080;

constexpr U64 LU_DIAG_MASK = 0x7f3f1f0f07030100;
constexpr U64 RU_DIAG_MASK = 0xfefcf8f0e0c08000;

constexpr std::array<U64, 15> gen_diags(bool anti_diag) {
	U64 center_diag = (anti_diag) ? ANTI_MAIN_DIAG : MAIN_DIAG;
	U64 left_mask = (anti_diag) ? ~(RU_DIAG_MASK | ANTI_MAIN_DIAG) : LU_DIAG_MASK;
	U64 right_mask = (anti_diag) ? RU_DIAG_MASK : ~(LU_DIAG_MASK | MAIN_DIAG);

	std::array<U64, 15> diags = {0,0,0,0,0,0,0,center_diag};
	for (int i = 6; i >= 0; i--) diags[i] = center_diag >> (7-i) & left_mask;
	for (int i = 8; i < 15; i++) diags[i] = center_diag << (i-7) & right_mask;
	if (!anti_diag) std::reverse(diags.begin(), diags.end());
	return diags;
}

constexpr std::array<U64, 15> DIAGS = gen_diags(false);
constexpr std::array<U64, 15> A_DIAGS = gen_diags(true);

/*
	Combined Lines Array
*/

constexpr U64 FULL_BOARD_INDEX = 46;
constexpr U64 FULL_BOARD = ~(U64)(0);

constexpr std::array<U64, 47> gen_combined_lines() {
	std::array<U64, 47> combined_lines;
	for (int i = 0; i < 8; i++) {
		combined_lines[i+1] = RANKS[i];
		combined_lines[9 + i] = FILES[i];
	}
	for (int i = 0; i < 15; i++) {
		combined_lines[17 + i] = DIAGS[i];
		combined_lines[32 + i] = A_DIAGS[i];
	}
	combined_lines[0] = FULL_BOARD;
	return combined_lines;
}

constexpr std::array<U64, 47> ALL_LINES = gen_combined_lines();

/*
	Starting board constants
*/

constexpr U64 START_WHITE_PAWNS = 0x000000000000FF00;
constexpr U64 START_WHITE_KNIGHTS = 0x0000000000000042;
constexpr U64 START_WHITE_BISHOPS = 0x0000000000000024;
constexpr U64 START_WHITE_ROOKS = 0x0000000000000081;
constexpr U64 START_WHITE_QUEENS = 0x0000000000000008;
constexpr U64 START_WHITE_KING = 0x0000000000000010;
constexpr U64 START_WHITE_PIECES = 0x000000000000FFFF;

constexpr U64 START_BLACK_PAWNS = 0x00FF000000000000;
constexpr U64 START_BLACK_KNIGHTS = 0x4200000000000000;
constexpr U64 START_BLACK_BISHOPS = 0x2400000000000000;
constexpr U64 START_BLACK_ROOKS = 0x8100000000000000;
constexpr U64 START_BLACK_QUEENS = 0x0800000000000000;
constexpr U64 START_BLACK_KING = 0x1000000000000000;
constexpr U64 START_BLACK_PIECES = 0xFFFF000000000000;

constexpr U64 START_ALL_PIECES = 0xFFFF00000000FFFF;
constexpr U64 START_EMPTY_FIELDS = 0x0000FFFFFFFF0000;

/*
	Index to square string
*/

constexpr std::array<std::string_view, 64> SQUARE_MAP = { 
	"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
	"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
	"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
	"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
	"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
	"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
	"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
	"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8"
};

constexpr U64 NOT_FILE_A = ~FILE_A;
constexpr U64 NOT_FILE_H = ~FILE_H;

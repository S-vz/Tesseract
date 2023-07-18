#pragma once

#include "ChessConstants.h"

/*
	Contains data for PeSTo evaluation function according to
	https://www.talkchess.com/forum3/viewtopic.php?f=2&t=68311&sid=0373e1206140d02b377e86dbe98e68d5
*/

constexpr I16 piece_eval[13] = {
	// Pawn
	 82,
	-82,

	// Knight
	 337,
	-337,

	// Bishop
	 365,
	-365,

	// Rook
	 477,
	-477,

	// King
	 0,
	 0,

	// Queen
	 1025,
	-1025,

	0,
};

constexpr I16 piece_eval_eg[13] = {
	// Pawn
	 94,
	-94,

	// Knight
	 281,
	-281,

	// Bishop
	 297,
	-297,

	// Rook
	 512,
	-512,

	// King
	 0,
	 0,

	 // Queen
	  936,
	 -936,

	 0,
};


constexpr I16 piece_eval_abs[13] = { 20, 20, 64, 64, 66, 66, 100, 100, 10000, 10000, 180, 180, 0 };
constexpr I16 piece_eval_mult[13] = { 900, 900, 2880, 2880, 2970, 2970, 4500, 4500, 10000, 10000, 8100, 8100, 0 };
constexpr I16 piece_value_linear[12] = { 0, 0, 1, 1, 2, 2, 3, 3, 5, 5, 4, 4 };
constexpr I16 piece_score_phases[13] = { 0, 0, 10, 10, 10, 10, 22, 22, 0, 0, 44, 44, 0 };

constexpr std::array<I16, 64> vmirror_psqt(std::array<I16, 64> old_psqt) {
	std::array<I16, 64> new_psqt;
	for (U8 i = 0; i < 8; i++)
		for (U8 j = 0; j < 8; j++)
			new_psqt[8 * i + j] = old_psqt[8 * (7 - i) + j];
	return new_psqt;
}

constexpr std::array<I16, 64> flip_sign(std::array<I16, 64> old_psqt) {
	std::array<I16, 64> new_psqt;
	for(U8 i = 0; i < 64; i++) new_psqt[i] = -old_psqt[i];
	return new_psqt;
}

constexpr std::array<I16, 64> pawn_eval_mg = {
	  0,   0,   0,   0,   0,   0,  0,   0,
	 98, 134,  61,  95,  68, 126, 34, -11,
	 -6,   7,  26,  31,  65,  56, 25, -20,
	-14,  13,   6,  21,  23,  12, 17, -23,
	-27,  -2,  -5,  12,  17,   6, 10, -25,
	-26,  -4,  -4, -10,   3,   3, 33, -12,
	-35,  -1, -20, -23, -15,  24, 38, -22,
	  0,   0,   0,   0,   0,   0,  0,   0,
};

constexpr std::array<I16, 64> pawn_eval_eg = {
	  0,   0,   0,   0,   0,   0,   0,   0,
	178, 173, 158, 134, 147, 132, 165, 187,
	 94, 100,  85,  67,  56,  53,  82,  84,
	 32,  24,  13,   5,  -2,   4,  17,  17,
	 13,   9,  -3,  -7,  -7,  -8,   3,  -1,
	  4,   7,  -6,   1,   0,  -5,  -1,  -8,
	 13,   8,   8,  10,  13,   0,   2,  -7,
	  0,   0,   0,   0,   0,   0,   0,   0,
};

constexpr std::array<I16, 64> knight_eval_mg = {
	-167, -89, -34, -49,  61, -97, -15, -107,
	 -73, -41,  72,  36,  23,  62,   7,  -17,
	 -47,  60,  37,  65,  84, 129,  73,   44,
	  -9,  17,  19,  53,  37,  69,  18,   22,
	 -13,   4,  16,  13,  28,  19,  21,   -8,
	 -23,  -9,  12,  10,  19,  17,  25,  -16,
	 -29, -53, -12,  -3,  -1,  18, -14,  -19,
	-105, -21, -58, -33, -17, -28, -19,  -23,
};

constexpr std::array<I16, 64> knight_eval_eg = {
	-58, -38, -13, -28, -31, -27, -63, -99,
	-25,  -8, -25,  -2,  -9, -25, -24, -52,
	-24, -20,  10,   9,  -1,  -9, -19, -41,
	-17,   3,  22,  22,  22,  11,   8, -18,
	-18,  -6,  16,  25,  16,  17,   4, -18,
	-23,  -3,  -1,  15,  10,  -3, -20, -22,
	-42, -20, -10,  -5,  -2, -20, -23, -44,
	-29, -51, -23, -15, -22, -18, -50, -64,
};

constexpr std::array<I16, 64> bishop_eval_mg = {
	-29,   4, -82, -37, -25, -42,   7,  -8,
	-26,  16, -18, -13,  30,  59,  18, -47,
	-16,  37,  43,  40,  35,  50,  37,  -2,
	 -4,   5,  19,  50,  37,  37,   7,  -2,
	 -6,  13,  13,  26,  34,  12,  10,   4,
	  0,  15,  15,  15,  14,  27,  18,  10,
	  4,  15,  16,   0,   7,  21,  33,   1,
	-33,  -3, -14, -21, -13, -12, -39, -21,
};

constexpr std::array<I16, 64> bishop_eval_eg = {
	-14, -21, -11,  -8, -7,  -9, -17, -24,
	 -8,  -4,   7, -12, -3, -13,  -4, -14,
	  2,  -8,   0,  -1, -2,   6,   0,   4,
	 -3,   9,  12,   9, 14,  10,   3,   2,
	 -6,   3,  13,  19,  7,  10,  -3,  -9,
	-12,  -3,   8,  10, 13,   3,  -7, -15,
	-14, -18,  -7,  -1,  4,  -9, -15, -27,
	-23,  -9, -23,  -5, -9, -16,  -5, -17,
};

constexpr std::array<I16, 64> rook_eval_mg = {
	 32,  42,  32,  51, 63,  9,  31,  43,
	 27,  32,  58,  62, 80, 67,  26,  44,
	 -5,  19,  26,  36, 17, 45,  61,  16,
	-24, -11,   7,  26, 24, 35,  -8, -20,
	-36, -26, -12,  -1,  9, -7,   6, -23,
	-45, -25, -16, -17,  3,  0,  -5, -33,
	-44, -16, -20,  -9, -1, 11,  -6, -71,
	-19, -13,   1,  17, 16,  7, -37, -26,
};

constexpr std::array<I16, 64> rook_eval_eg = {
	13, 10, 18, 15, 12,  12,   8,   5,
	11, 13, 13, 11, -3,   3,   8,   3,
	 7,  7,  7,  5,  4,  -3,  -5,  -3,
	 4,  3, 13,  1,  2,   1,  -1,   2,
	 3,  5,  8,  4, -5,  -6,  -8, -11,
	-4,  0, -5, -1, -7, -12,  -8, -16,
	-6, -6,  0,  2, -9,  -9, -11,  -3,
	-9,  2,  3, -1, -5, -13,   4, -20,
};

constexpr std::array<I16, 64> king_eval_mg = {
	-65,  23,  16, -15, -56, -34,   2,  13,
	 29,  -1, -20,  -7,  -8,  -4, -38, -29,
	 -9,  24,   2, -16, -20,   6,  22, -22,
	-17, -20, -12, -27, -30, -25, -14, -36,
	-49,  -1, -27, -39, -46, -44, -33, -51,
	-14, -14, -22, -46, -44, -30, -15, -27,
	  1,   7,  -8, -64, -43, -16,   9,   8,
	-15,  36,  12, -54,   8, -28,  24,  14,
};

constexpr std::array<I16, 64> king_eval_eg = {
	-74, -35, -18, -18, -11,  15,   4, -17,
	-12,  17,  14,  17,  17,  38,  23,  11,
	 10,  17,  23,  15,  20,  45,  44,  13,
	 -8,  22,  24,  27,  26,  33,  26,   3,
	-18,  -4,  21,  24,  27,  23,   9, -11,
	-19,  -3,  11,  21,  23,  16,   7,  -9,
	-27, -11,   4,  13,  14,   4,  -5, -17,
	-53, -34, -21, -11, -28, -14, -24, -43
};

constexpr std::array<I16, 64> queen_eval_mg = {
	-28,   0,  29,  12,  59,  44,  43,  45,
	-24, -39,  -5,   1, -16,  57,  28,  54,
	-13, -17,   7,   8,  29,  56,  47,  57,
	-27, -27, -16, -16,  -1,  17,  -2,   1,
	 -9, -26,  -9, -10,  -2,  -4,   3,  -3,
	-14,   2, -11,  -2,  -5,   2,  14,   5,
	-35,  -8,  11,   2,   8,  15,  -3,   1,
	 -1, -18,  -9,  10, -15, -25, -31, -50,
};

constexpr std::array<I16, 64> queen_eval_eg = {
	 -9,  22,  22,  27,  27,  19,  10,  20,
	-17,  20,  32,  41,  58,  25,  30,   0,
	-20,   6,   9,  49,  47,  35,  19,   9,
	  3,  22,  24,  45,  57,  40,  57,  36,
	-18,  28,  19,  47,  31,  34,  39,  23,
	-16, -27,  15,   6,   9,  17,  10,   5,
	-22, -23, -30, -16, -16, -23, -36, -32,
	-33, -28, -22, -43,  -5, -32, -20, -41,
};

constexpr std::array<I16, 64> square_piece_table[13] = {
	vmirror_psqt(pawn_eval_mg),
	flip_sign(pawn_eval_mg),
	vmirror_psqt(knight_eval_mg),
	flip_sign(knight_eval_mg),
	vmirror_psqt(bishop_eval_mg),
	flip_sign(bishop_eval_mg),
	vmirror_psqt(rook_eval_mg),
	flip_sign(rook_eval_mg),
	vmirror_psqt(king_eval_mg),
	flip_sign(king_eval_mg),
	vmirror_psqt(queen_eval_mg),
	flip_sign(queen_eval_mg),
	{},
};

constexpr std::array<I16, 64> square_piece_table_eg[13] = {
	vmirror_psqt(pawn_eval_eg),
	flip_sign(pawn_eval_eg),
	vmirror_psqt(knight_eval_eg),
	flip_sign(knight_eval_eg),
	vmirror_psqt(bishop_eval_eg),
	flip_sign(bishop_eval_eg),
	vmirror_psqt(rook_eval_eg),
	flip_sign(rook_eval_eg),
	vmirror_psqt(king_eval_eg),
	flip_sign(king_eval_eg),
	vmirror_psqt(queen_eval_eg),
	flip_sign(queen_eval_eg),
	{},
};

constexpr I8 knight_mobility_scores[9] = { -12, -10, -4, 0, 0, 1, 2, 3, 3 };
constexpr I8 bishop_mobility_scores[14] = { -10, -5, 1, 3, 4, 5, 5, 6, 6, 7, 8, 8, 9, 9 };
constexpr I8 rook_mobility_scores[15] = { -12, -5, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 5, 5, 6 };
constexpr I8 queen_mobility_scores[28] = { -8, -5, -3, -3, 2, 2, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 9, 10, 10, 10, 11, 11, 11, 12 };

constexpr I8 knight_mobility_scores_eg[9] = { -18, -14, -8, 0, 0, 1, 2, 3, 3 };
constexpr I8 bishop_mobility_scores_eg[14] = { -15, -15, 1, 3, 4, 5, 5, 6, 6, 7, 8, 8, 9, 9 };
constexpr I8 rook_mobility_scores_eg[15] = { -18, -10, 0, 0, 0, 1, 2, 3, 4, 4, 4, 4, 5, 5, 6 };
constexpr I8 queen_mobility_scores_eg[28] = { -20, -15, -6, -3, 2, 2, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 9, 10, 10, 10, 11, 11, 11, 12 };

class EvalData {
public:
	constexpr I16 get_pos_eval(U8 pieceID, U8 squareIndex) { return square_piece_table[pieceID][squareIndex]; }
	constexpr I16 get_double_pos_eval(U8 fromID, U8 fromIndex, U8 toIndex) { return square_piece_table[fromID][toIndex] - square_piece_table[fromID][fromIndex]; }

	constexpr I16 get_pos_eval_eg(U8 pieceID, U8 squareIndex) { return square_piece_table_eg[pieceID][squareIndex]; }
	constexpr I16 get_double_pos_eval_eg(U8 fromID, U8 fromIndex, U8 toIndex) { return square_piece_table_eg[fromID][toIndex] - square_piece_table_eg[fromID][fromIndex]; }
};

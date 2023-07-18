#pragma once

#include <iostream>
#include <array>

#include "ChessConstants.h"

/*
	King, Knight and Pawn Move Generation
	Generates squareIndex look up tables

	These are jumping pieces, which do not need complicated sliding attack logic. 
	Hence they can be mapped to a simple table with 64 squares denoting each pseudo-legal move.
	Uses an extremely fast method of expanding pre-defined attacks outward instead of calculating for every square.
*/

constexpr U64 notAB12 = ~(FILE_A | FILE_B | RANK_1 | RANK_2);
constexpr U64 notAB78 = ~(FILE_A | FILE_B | RANK_7 | RANK_8);
constexpr U64 notHG12 = ~(FILE_H | FILE_G | RANK_1 | RANK_2);
constexpr U64 notHG78 = ~(FILE_H | FILE_G | RANK_7 | RANK_8);

constexpr U64 KNIGHT_ATTACKS_LD = 0x142200221400;
constexpr U64 KING_ATTACKS_LD = 0x1c141c0000;
constexpr U64 PAWN_ATTACKS_LD = 0x1400000000;

void generate_all_jump_moves(BoardArray* all_moves_p, U64 attacks);

void generate_all_knight_moves(BoardArray* all_moves_p);

void generate_all_king_moves(BoardArray* all_moves_p);

void generate_all_pawn_attacks(BoardArray* all_moves_white_p, BoardArray* all_moves_black_p);

/*
	Rook Move Generation
	Generates PEXT look up tables

	Uses o^(o-2r) to generate moves for both positive and negative rays through
	the use of fast mirroring bitwise actions.
*/

constexpr U64 ROOK_TABLE_SIZE = 102400;

U64 hmirror_rank(U64 bb, U64 rankBB);

U64 o_xor_o_2r(U64 block_mask, U64 lineBB, U64 pieceBBx2);

U64 generate_rook_blocking_mask(U64 pieceIndex, U64 rankBB, U64 fileBB, U64 pieceBB);

U64 generate_rook_moves_for_block_mask(U64 block_mask, U64 fileBB, U64 rankBB, U64 pieceBBx2, U64 pieceBBx2_file_mirror, U64 pieceBBx2_rank_mirror);

U64 generate_rook_moves_for_index(std::array<U64, ROOK_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, U64 pieceIndex, U64 offset);

void generate_all_rook_moves(std::array<U64, ROOK_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, BoardArray* square_offsets);

/*
	Bishop Move Generation
	Generates PEXT look up tables

	Converts bishop diagonal attacks to rook attacks, then re-uses rook functions above
	to calculate the possible moves. This makes it so that we can re-use the o^(o-2r) 
	trick instead of using a new algorithm for diagonal attacks.
*/

constexpr U64 BISHOP_TABLE_SIZE = 5248;

constexpr U64 notAH18 = ~(FILE_A | FILE_H | RANK_1 | RANK_8);

U64 rotate_bishop_to_rook_mask(U64 bishop_mask, U64 diagBB, U64 adiagBB, U64 rankBB, U64 fileBB, U64 diag_index, U64 adiag_index);

U64 rotate_rook_to_bishop_mask(U64 rook_mask, U64 diagBB, U64 adiagBB, U64 rankBB, U64 fileBB, U64 diag_index, U64 adiag_index);

U64 generate_bishop_moves_for_index(std::array<U64, BISHOP_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, U64 pieceIndex, U64 offset);

void generate_all_bishop_moves(std::array<U64, BISHOP_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, BoardArray* square_offsets);

/*
	Lines used for move generation during pinning and checks
*/

struct QueenLine {
	U64 queen;
	U64 rook;
	U64 bishop;
};

struct Line {
	U64 full;
	U64 partial;
	U8 line_index;
};

void generate_all_lines(std::array<Line, 4096>&);

/*
	Misc
*/

constexpr U64 PAWN_SHIELD_LD = 0x1c00000000;

struct KingPawns {
	U64 pawn_shield;
	U64 pawn_storm;
};

struct PawnStructure {
	U64 file;
	U64 partial_file;
	U64 adjecent_files = 0ULL;
	U64 adjecent_forward_files = 0ULL;
	U64 adjecent_back_files = 0ULL;
};

void generate_zobrist_hashes(std::array<U64, 848>& zhash_table);

void generate_king_pawns(std::array<KingPawns, 64>&, std::array<KingPawns, 64>&);

void generate_pawn_structure(std::array<PawnStructure, 64>&, std::array<PawnStructure, 64>&, std::array<Line, 4096>&);

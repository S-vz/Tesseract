#include <intrin.h>
#include <random>

#include "DataGenerator.h"

/*
	King, Knight and Pawn Move Generation
	Generates squareIndex look up tables
*/

void generate_all_jump_moves(BoardArray* all_moves_p, U64 attacks) {
	// Quadrant starting pieces
	U64 start_piece_LU = 0x800000000, start_piece_RU = 0x1000000000;
	U64 start_piece_LD = 0x8000000, start_piece_RD = 0x10000000;

	U64 attacks_LU = attacks << 8, attacks_RU = attacks << 9;
	U64 attacks_LD = attacks << 0, attacks_RD = attacks << 1;

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			(*all_moves_p)[bsf(start_piece_LD >> (i * 8 + j))] = (attacks_LD >> (i * 8 + j))& notHG78;
			(*all_moves_p)[bsf(start_piece_LU >> j << (i * 8))] = (attacks_LU >> j << (i * 8))& notHG12;
			(*all_moves_p)[bsf(start_piece_RD << j >> (i * 8))] = (attacks_RD << j >> (i * 8))& notAB78;
			(*all_moves_p)[bsf(start_piece_RU << (i * 8 + j))] = (attacks_RU << (i * 8 + j)) & notAB12;
		}
	}
}

void generate_all_knight_moves(BoardArray* all_moves_p) {
	return generate_all_jump_moves(all_moves_p, KNIGHT_ATTACKS_LD);
}

void generate_all_king_moves(BoardArray* all_moves_p) {
	return generate_all_jump_moves(all_moves_p, KING_ATTACKS_LD);
}

void generate_all_pawn_attacks(BoardArray* all_moves_white_p, BoardArray* all_moves_black_p) {
	generate_all_jump_moves(all_moves_white_p, PAWN_ATTACKS_LD);
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			(*all_moves_black_p)[63-(i*8 + (7-j))] = vmirror((*all_moves_white_p)[i*8+j]);
		}
	} 
}

/*
	Rook Move Generation
	Generates PEXT look up tables
*/

// Custom horizontal rank mirror utilising PEXT + PEDP + single byte mirror. Only mirrors a certain rank.
U64 hmirror_rank(U64 bb, U64 rankBB) {
	U64 pexted_bb = pext(bb, rankBB);
	U8 mirrored_pexted_bb = (U8)((pexted_bb * 0x0202020202ULL & 0x010884422010ULL) % 0x3ff);
	return pdep(mirrored_pexted_bb, rankBB);
}

U64 o_xor_o_2r(U64 block_mask, U64 lineBB, U64 pieceBBx2) {
	U64 line_block = (block_mask & lineBB);
	return ((line_block - (pieceBBx2)) ^ line_block) & lineBB;
}

U64 generate_rook_blocking_mask(U64 rankBB, U64 fileBB, U64 pieceBB) {
	U64 rankMask = rankBB & ~(FILE_A | FILE_H);
	U64 fileMask = fileBB & ~(RANK_1 | RANK_8);
	return (rankMask | fileMask) & ~pieceBB;
}

// Uses o^(o-2r) to determine sliding attacks
U64 generate_rook_moves_for_block_mask(U64 block_mask, U64 fileBB, U64 rankBB, U64 pieceBBx2, U64 pieceBBx2_file_mirror, U64 pieceBBx2_rank_mirror) {
	// Positive rays
	U64 file_attacks_pos = o_xor_o_2r(block_mask, fileBB, pieceBBx2);
	U64 rank_attacks_pos = o_xor_o_2r(block_mask, rankBB, pieceBBx2);

	// Negative rays
	U64 file_attacks_mirror = o_xor_o_2r(vmirror(block_mask), fileBB, pieceBBx2_file_mirror);
	U64 file_attacks_neg = vmirror(file_attacks_mirror);

	U64 rank_attacks_mirror = o_xor_o_2r(hmirror_rank(block_mask, rankBB), rankBB, pieceBBx2_rank_mirror);
	U64 rank_attacks_neg = hmirror_rank(rank_attacks_mirror, rankBB);

	return (file_attacks_pos | file_attacks_neg | rank_attacks_pos | rank_attacks_neg);
}

// Returns number of possible rook moves
U64 generate_rook_moves_for_index(std::array<U64, ROOK_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, U64 pieceIndex, U64 offset) {
	U64 rankBB = RANKS[pieceIndex >> 3];
	U64 fileBB = FILES[pieceIndex & 7];
	U64 pieceBB = 1ULL << pieceIndex;

	U64 base_block_mask = generate_rook_blocking_mask(rankBB, fileBB, pieceBB);
	U64 num_moves = 1ULL << popcnt(base_block_mask);

	(*blocking_masks)[pieceIndex] = base_block_mask;

	U64 pieceBBx2 = pieceBB << 1;
	U64 pieceBBx2_file_mirror = vmirror(pieceBB) << 1;
	U64 pieceBBx2_rank_mirror = hmirror_rank(pieceBB, rankBB) << 1;

	for (U64 block_pext = 0; block_pext < num_moves; block_pext++) {
		U64 block_mask = pdep(block_pext, base_block_mask) | pieceBB;
		U64 attacks = generate_rook_moves_for_block_mask(block_mask, fileBB, rankBB, pieceBBx2, pieceBBx2_file_mirror, pieceBBx2_rank_mirror);
		(*all_moves_p)[offset + block_pext] = attacks;
	}
	return num_moves;
}

void generate_all_rook_moves(std::array<U64, ROOK_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, BoardArray* square_offsets) {
	U64 total_current_offset = 0;
	for (U64 pieceIndex = 0; pieceIndex < 64; pieceIndex++) {
		(*square_offsets)[pieceIndex] = total_current_offset;
		U64 num_moves = generate_rook_moves_for_index(all_moves_p, blocking_masks, pieceIndex, total_current_offset);
		total_current_offset += num_moves;
	}
}

/*
	Bishop Move Generation
	Generates PEXT look up tables
*/

U64 rotate_bishop_to_rook_mask(U64 bishop_mask, U64 diagBB, U64 adiagBB, U64 rankBB, U64 fileBB, U64 diag_index, U64 adiag_index) {
	U64 pext_diag = pext(bishop_mask, diagBB);
	U64 pext_anti_diag = pext(bishop_mask, adiagBB);
	if (diag_index < 7) pext_diag <<= 8 - popcnt(diagBB);
	if (adiag_index > 7) pext_anti_diag <<= 8 - popcnt(adiagBB);
	U64 pdep_rank = pdep(pext_diag, rankBB);
	U64 pdep_file = pdep(pext_anti_diag, fileBB);
	return (pdep_rank | pdep_file);
}

U64 rotate_rook_to_bishop_mask(U64 rook_mask, U64 diagBB, U64 adiagBB, U64 rankBB, U64 fileBB, U64 diag_index, U64 adiag_index) {
	U64 pext_rank = pext(rook_mask, rankBB);
	U64 pext_file = pext(rook_mask, fileBB);
	if (diag_index < 7) pext_rank >>= 8 - popcnt(diagBB);
	if (adiag_index > 7) pext_file >>= 8 - popcnt(adiagBB);
	U64 pdep_diag = pdep(pext_rank, diagBB);
	U64 pdep_anti_diag = pdep(pext_file, adiagBB);
	return (pdep_diag | pdep_anti_diag);
}

// Returns number of possible bishop moves
U64 generate_bishop_moves_for_index(std::array<U64, BISHOP_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, U64 pieceIndex, U64 offset) {
	U64 row = pieceIndex >> 3;
	U64 column = pieceIndex & 7;

	U64 diag_index = row - column + 7;
	U64 adiag_index = row + column;

	U64 diagBB = DIAGS[diag_index];
	U64 adiagBB = A_DIAGS[adiag_index];
	U64 rankBB = RANKS[row];
	U64 fileBB = FILES[column];

	U64 bishop_block_mask = (diagBB ^ adiagBB) & notAH18;
	U64 num_moves = 1ULL << popcnt(bishop_block_mask);

	(*blocking_masks)[pieceIndex] = bishop_block_mask;

	U64 pieceBB = 1ULL << pieceIndex;
	U64 pieceBBx2 = pieceBB << 1;
	U64 pieceBBx2_file_mirror = vmirror(pieceBB) << 1;
	U64 pieceBBx2_rank_mirror = hmirror_rank(pieceBB, rankBB) << 1;

	for (U64 block_pext = 0; block_pext < num_moves; block_pext++) {
		U64 block_mask = pdep(block_pext, bishop_block_mask) | pieceBB;
		U64 rotated_block_mask = rotate_bishop_to_rook_mask(block_mask, diagBB, adiagBB, rankBB, fileBB, diag_index, adiag_index);
		U64 rook_attacks = generate_rook_moves_for_block_mask(rotated_block_mask, fileBB, rankBB, pieceBBx2, pieceBBx2_file_mirror, pieceBBx2_rank_mirror);
		U64 bishop_attacks = rotate_rook_to_bishop_mask(rook_attacks, diagBB, adiagBB, rankBB, fileBB, diag_index, adiag_index);
		(*all_moves_p)[offset + block_pext] = bishop_attacks;
	}
	return num_moves;
}

void generate_all_bishop_moves(std::array<U64, BISHOP_TABLE_SIZE>* all_moves_p, BoardArray* blocking_masks, BoardArray* square_offsets) {
	U64 total_current_offset = 0;
	for (U64 pieceIndex = 0; pieceIndex < 64; pieceIndex++) {
		(*square_offsets)[pieceIndex] = total_current_offset;
		U64 num_moves = generate_bishop_moves_for_index(all_moves_p, blocking_masks, pieceIndex, total_current_offset);
		total_current_offset += num_moves;
	}
}

std::tuple<U8, U8, U8, U8> extract_indices(U8 squareIndex) {
	U8 row = squareIndex >> 3;
	U8 column = squareIndex & 7;

	U8 diag_index = row - column + 7;
	U8 adiag_index = row + column;
	return std::tuple(row, column, diag_index, adiag_index);
}

U64 get_line_mask(U8 row1, U8 row2, U8 column1, U8 column2) {
	U8 min_row = std::min(row1, row2);
	U8 min_column = std::min(column1, column2);
	U8 row_range = (U8)std::abs(row1 - row2);
	U8 column_range = (U8)std::abs(column1 - column2);
	U64 line_mask = 0ULL;

	if (row_range != 0) for (int i = min_row; i <= min_row+row_range; i++) line_mask |= RANKS[i];
	if (column_range != 0) for (int i = min_column; i <= min_column+column_range; i++) line_mask |= FILES[i];
	return line_mask;
}

void generate_all_lines(std::array<Line, 4096>& all_lines) {
	for (U8 i = 0; i < 64; i++) {
		auto[i_row, i_column, i_diag, i_adiag] = extract_indices(i);
		for (U8 j = 0; j < 64; j++) {
			auto [j_row, j_column, j_diag, j_adiag] = extract_indices(j);

			bool row_equal = (i_row == j_row);
			bool column_equal = (i_column == j_column);
			bool diag_equal = (i_diag == j_diag);
			bool adiag_equal = (i_adiag == j_adiag);

			U8 lines_index = (row_equal * i_row) + (!row_equal * 8);
			lines_index += ((column_equal * i_column) + (!column_equal * 8)) * (lines_index == 8);
			lines_index += ((diag_equal * i_diag) + (!diag_equal * 15)) * (lines_index == 16);
			lines_index += ((adiag_equal * i_adiag) + (!adiag_equal * 15)) * (lines_index == 31);
			lines_index++;

			U64 line_mask = get_line_mask(i_row, j_row, i_column, j_column) & ~(1ULL << i);
			all_lines[(i << 6) + j] = { ALL_LINES[lines_index], ALL_LINES[lines_index] & line_mask, lines_index };
		}
	}
}

/*
	Misc
*/

void generate_zobrist_hashes(std::array<U64, 848>& zhash_table) {
	static std::random_device rd;
	static auto nm = rd();
	//auto nm = 2183994271;
	std::default_random_engine generator;
	generator.seed(nm);
	std::uniform_int_distribution<U64> dist(0, 0xFFFFFFFFFFFFFFFF);

	zhash_table.fill(0ULL);
	for (int i = 0; i < 768; i++) zhash_table[i] = dist(generator);
	for (int i = 832; i < 848; i++) zhash_table[i] = dist(generator);
}

void generate_king_pawns(std::array<KingPawns, 64>& wking_pawns, std::array<KingPawns, 64>& bking_pawns) {
	BoardArray temp_board_array;
	generate_all_jump_moves(&temp_board_array, PAWN_SHIELD_LD);

	for (U8 i = 0; i < 64; i++) {
		U64 pawn_shield = temp_board_array[i];
		U64 pawn_storm = (pawn_shield << 8) | (pawn_shield << 16);
		wking_pawns[i] = { pawn_shield, pawn_storm };
		bking_pawns[i] = { vmirror(pawn_shield), vmirror(pawn_storm) };
	}
}

void generate_pawn_structure(std::array<PawnStructure, 64>& wpawn_structure, std::array<PawnStructure, 64>& bpawn_structure, std::array<Line, 4096>& all_lines) {
	U64 side_ranks_bb = RANK_1 | RANK_8;
	for (U8 i = 0; i < 64; i++) {
		PawnStructure wstruct;
		PawnStructure bstruct;

		U8 file = i & 7;
		U64 fileBB = FILES[file];

		U8 bottom_index = bsf(side_ranks_bb & fileBB);
		unsigned long top_index;
		_BitScanReverse64(&top_index, side_ranks_bb & fileBB);

		U64 upper_line = all_lines[(i << 6) + top_index].partial;
		U64 bottom_line = all_lines[(i << 6) + bottom_index].partial;

		wstruct.file = fileBB;
		bstruct.file = fileBB;

		wstruct.partial_file = upper_line;
		bstruct.partial_file = bottom_line;

		if (file != 0) {
			wstruct.adjecent_files |= fileBB >> 1;
			wstruct.adjecent_forward_files |= upper_line >> 1;
			wstruct.adjecent_back_files |= bottom_line >> 1;

			bstruct.adjecent_files |= fileBB >> 1;
			bstruct.adjecent_forward_files |= bottom_line >> 1;
			bstruct.adjecent_back_files |= upper_line >> 1;
		}
		if (file != 7) {
			wstruct.adjecent_files |= fileBB << 1;
			wstruct.adjecent_forward_files |= upper_line << 1;
			wstruct.adjecent_back_files |= bottom_line << 1;

			bstruct.adjecent_files |= fileBB << 1;
			bstruct.adjecent_forward_files |= bottom_line << 1;
			bstruct.adjecent_back_files |= upper_line << 1;
		}

		wpawn_structure[i] = wstruct;
		bpawn_structure[i] = bstruct;
	}
}
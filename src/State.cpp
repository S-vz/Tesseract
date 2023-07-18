#include "State.h"
#include <iostream>
#include <fstream>

void State::perft_all_moves(U64 depth, U64& total_moves) {
	if (depth == 1) {
		total_moves += (this->move_iter - this->move_arr);
		return;
	}

	for (Move* mv = this->move_arr; mv != this->move_iter; mv++) 
		this->internal_move(*mv).perft_all_moves(depth - 1, total_moves);
}

void State::extract_moves(U64 moves, U8 squareIndex, Move*& mv) {
	while (moves != 0) {
		*(mv++) = Move(squareIndex, bsf(moves));
		moves &= moves - 1;
	}
}

void State::update_moves_start(U64 coverage, U64 check_mask, U64 checkers) {
	if (checkers == 0) {
		this->update_moves(coverage);
	}
	else if (popcnt(checkers) == 1) {
		this->update_moves_check(coverage, check_mask);
	}
	else {
		U64 king = this->piecesBB[WHITE_KING_ID + this->turn];

		U64 friendly_pieces_BB = this->piecesBB[WHITE_PIECES_ID + this->turn];

		Move* mv = this->move_iter;
		if (!this->turn) {
			U8 pieceIndex = bsf(king);
			U64 moveBB = this->update_moves_wking_checked(pieceIndex, friendly_pieces_BB, coverage);
			extract_moves(moveBB, pieceIndex, mv);
		}
		else {
			U8 pieceIndex = bsf(king);
			U64 moveBB = this->update_moves_bking_checked(pieceIndex, friendly_pieces_BB, coverage);
			extract_moves(moveBB, pieceIndex, mv);
		}
		this->move_iter = mv;
	}
}

void State::update_move_template(U64 piecesID, U64 fpieces_BB, Move*& mv, U64(State::*move_func)(U8, U64)) {
	U64 pieces = this->piecesBB[piecesID];
	while (pieces != 0) {
		U8 pieceIndex = bsf(pieces);
		U64 moveBB = (this->*move_func)(pieceIndex, fpieces_BB);
		extract_moves(moveBB, pieceIndex, mv);
		pieces &= pieces - 1;
	}
}

void State::update_move_check_template(U64 piecesID, U64 fpieces_BB, U64 check_mask, Move*& mv, U64(State::* move_func)(U8, U64)) {
	U64 pieces = this->piecesBB[piecesID];
	while (pieces != 0) {
		U8 pieceIndex = bsf(pieces);
		U64 moveBB = (this->*move_func)(pieceIndex, fpieces_BB) & check_mask;
		extract_moves(moveBB, pieceIndex, mv);
		pieces &= pieces - 1;
	}
}

void State::update_moves(U64 coverage) {
	U64 king = this->piecesBB[WHITE_KING_ID + this->turn];

	U64 friendly_pieces_BB = this->piecesBB[WHITE_PIECES_ID + this->turn];
	U64 friendly_pawns_BB = this->piecesBB[WHITE_PAWNS_ID + this->turn];

	Move* mv = this->move_iter;
	if (!this->turn) {
		mv = update_wpawns<PawnTypes::None>(friendly_pawns_BB, FULL_BOARD, mv);
		U8 pieceIndex = bsf(king);
		U64 moveBB = this->update_moves_wking(pieceIndex, friendly_pieces_BB, coverage);
		extract_moves(moveBB, pieceIndex, mv);

	}
	else {
		mv = update_bpawns<PawnTypes::None>(friendly_pawns_BB, FULL_BOARD, mv);
		U8 pieceIndex = bsf(king);
		U64 moveBB = this->update_moves_bking(pieceIndex, friendly_pieces_BB, coverage);
		extract_moves(moveBB, pieceIndex, mv);
	}

	update_move_template(WHITE_KNIGHTS_ID + this->turn, friendly_pieces_BB, mv, &State::update_moves_knight);
	update_move_template(WHITE_QUEENS_ID + this->turn, friendly_pieces_BB, mv, &State::update_moves_queen);
	update_move_template(WHITE_BISHOPS_ID + this->turn, friendly_pieces_BB, mv, &State::update_moves_bishop);
	update_move_template(WHITE_ROOKS_ID + this->turn, friendly_pieces_BB, mv, &State::update_moves_rook);

	this->move_iter = mv;
}

void State::update_moves_check(U64 coverage, U64 check_mask) {
	U64 king = this->piecesBB[WHITE_KING_ID + this->turn];

	U64 friendly_pieces_BB = this->piecesBB[WHITE_PIECES_ID + this->turn];
	U64 friendly_pawns_BB = this->piecesBB[WHITE_PAWNS_ID + this->turn];

	Move* mv = this->move_iter;
	if (!this->turn) {
		mv = update_wpawns<PawnTypes::Checked>(friendly_pawns_BB, check_mask, mv);
		U8 pieceIndex = bsf(king);
		U64 moveBB = this->update_moves_wking_checked(pieceIndex, friendly_pieces_BB, coverage);
		extract_moves(moveBB, pieceIndex, mv);

	}
	else {
		mv = update_bpawns<PawnTypes::Checked>(friendly_pawns_BB, check_mask, mv);
		U8 pieceIndex = bsf(king);
		U64 moveBB = this->update_moves_bking_checked(pieceIndex, friendly_pieces_BB, coverage);
		extract_moves(moveBB, pieceIndex, mv);
	}

	update_move_check_template(WHITE_KNIGHTS_ID + this->turn, friendly_pieces_BB, check_mask, mv, &State::update_moves_knight);
	update_move_check_template(WHITE_QUEENS_ID + this->turn, friendly_pieces_BB, check_mask, mv, &State::update_moves_queen);
	update_move_check_template(WHITE_BISHOPS_ID + this->turn, friendly_pieces_BB, check_mask, mv, &State::update_moves_bishop);
	update_move_check_template(WHITE_ROOKS_ID + this->turn, friendly_pieces_BB, check_mask, mv, &State::update_moves_rook);

	this->move_iter = mv;
}

/* 
	Handles legality through pieces that check, pinned pieces, and attacked squares
	This is partially handled seperately in the split State classes
*/

U64 State::handle_sliding_checkers(U64 queen_checkers, U64 rook_checker, U64 bishop_checker, U64 kingIndexMultiplied, U64& coverage) {
	U64 check_mask = 0ULL;
	while (rook_checker != 0) {
		U8 pieceIndex = bsf(rook_checker);
		coverage |= this->data_table->queen_lines[pieceIndex].rook;
		check_mask |= this->data_table->line_index[kingIndexMultiplied + pieceIndex].partial;

		rook_checker &= rook_checker - 1;
	}

	if (bishop_checker != 0) {
		U8 pieceIndex = bsf(bishop_checker);
		coverage |= this->data_table->queen_lines[pieceIndex].bishop;
		check_mask |= this->data_table->line_index[kingIndexMultiplied + pieceIndex].partial;

		bishop_checker &= bishop_checker - 1;
	}

	while (queen_checkers != 0) {
		U8 pieceIndex = bsf(queen_checkers);
		check_mask |= this->data_table->line_index[kingIndexMultiplied + pieceIndex].partial;
		coverage |= this->data_table->queen_lines[pieceIndex].bishop;
		coverage |= this->get_rook_rays_custom(pieceIndex, this->piecesBB[ALL_PIECES_ID] ^ (1ULL << (kingIndexMultiplied >> 6)));
		queen_checkers &= queen_checkers - 1;
	}

	return check_mask;
}

void State::add_to_coverage(U64 pieces, U64 blocking_pieces, U64& coverage, U64(State::*get_move_func)(U8, U64)) {
	while (pieces != 0) {
		coverage |= (this->*get_move_func)(bsf(pieces), blocking_pieces);
		pieces &= pieces - 1;
	}
}

constexpr U64 State::get_pinned_line(U64 squareIndex) {
	return ALL_LINES[this->pinnedPieceBB[squareIndex]];
}

// Handles the edge case en passant pin using a compact combination of bitwise operations
inline void State::handle_enpassent_pin(U64 pawns, U64 king, U64 coverage, U64 rook_rays) {
	U64 enp_square = this->enpassent_square;
	if (enp_square) {
		U8 enp_pawn_index = bsf(enp_square) + (8 - (16 * !this->turn));
		if ((enp_pawn_index >> 3) == (king >> 3)) {
			U64 enp_mask = ((7ULL << (enp_pawn_index - 1)) & pawns) | (1ULL << enp_pawn_index);
			U64 tricky_pins = ((coverage | rook_rays) & enp_mask);
			tricky_pins *= ((tricky_pins >> 2) & tricky_pins) == 0;
			this->enpassent_square *= __popcnt64(tricky_pins) != 2;
		}
	}
}

U64 State::handle_pinning_and_checks(U64 rook_pinning, U64 bishop_pinning, U64 queen_pinning, U8 enemy_king, U64& coverage, U64& checkers) {
	U16 kingIndexMultiplied = enemy_king << 6;
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];
	U64 rook_rays = this->get_rook_rays_custom(enemy_king, all_pieces);
	U64 bishop_rays = this->get_bishop_rays_custom(enemy_king, all_pieces);

	U64 queen_checker = queen_pinning & (rook_rays | bishop_rays);
	U64 rook_checker = (rook_pinning & rook_rays) & ~queen_pinning;
	U64 bishop_checker = (bishop_pinning & bishop_rays) & ~queen_pinning;

	U64 all_checkers = queen_checker | rook_checker | bishop_checker;
	U64 inv_checkers = ~all_checkers;
	checkers |= all_checkers;

	U64 block_mask = 0ULL;
	if (all_checkers) block_mask = handle_sliding_checkers(queen_checker, rook_checker, bishop_checker, kingIndexMultiplied, coverage);

	U64 pinned_pieces = 0ULL;
	U64 local_coverage = 0ULL;

	rook_pinning &= inv_checkers;
	while (rook_pinning != 0) {
		U64 ray = this->get_rook_rays_custom(bsf(rook_pinning), all_pieces);
		pinned_pieces |= rook_rays & ray;
		local_coverage |= ray;
		rook_pinning &= rook_pinning - 1;
	}

	U64 enemy_pawns = this->piecesBB[WHITE_PAWNS_ID + this->turn];
	handle_enpassent_pin(enemy_pawns, enemy_king, local_coverage, rook_rays);

	bishop_pinning &= inv_checkers;
	while (bishop_pinning != 0) {
		U64 ray = this->get_bishop_rays_custom(bsf(bishop_pinning), all_pieces);
		pinned_pieces |= bishop_rays & ray;
		local_coverage |= ray;
		bishop_pinning &= bishop_pinning - 1;
	}

	queen_pinning &= inv_checkers;
	while (queen_pinning != 0) {
		U8 pieceIndex = bsf(queen_pinning);
		U64 rook_coverage = this->get_rook_rays_custom(pieceIndex, all_pieces);
		U64 bishop_coverage = this->get_bishop_rays_custom(pieceIndex, all_pieces);
		local_coverage |= bishop_coverage | rook_coverage;
		queen_pinning &= queen_pinning - 1;
	}

	this->pinned_pawns = pinned_pieces & enemy_pawns;

	while (pinned_pieces != 0) {
		U8 pieceIndex = bsf(pinned_pieces);
		this->pinnedPieceBB[pieceIndex] = this->data_table->line_index[kingIndexMultiplied + pieceIndex].line_index;
		pinned_pieces &= pinned_pieces - 1;
	}

	coverage |= local_coverage;
	return block_mask;
}

std::tuple<U64, U64, U64> State::update_covered_squares() {
	U64 enemy_king = this->piecesBB[WHITE_KING_ID + this->turn];

	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];

	U64 king = this->piecesBB[WHITE_KING_ID + !this->turn];
	U64 coverage = this->get_moves_king(bsf(king), 0ULL);

	U64 knights = this->piecesBB[WHITE_KNIGHTS_ID + !this->turn];

	U8 enemy_king_index = bsf(enemy_king);
	U64 checkers = (this->get_moves_knight(enemy_king_index, 0ULL) & knights);

	while (knights != 0) {
		coverage |= this->get_moves_knight(bsf(knights), 0ULL);
		knights &= knights - 1;
	}

	QueenLine enemy_king_lines = this->data_table->queen_lines[enemy_king_index];
	U64 queens = this->piecesBB[WHITE_QUEENS_ID + !this->turn];
	U64 queen_pinning = enemy_king_lines.queen & queens;
	queens ^= queen_pinning;

	while (queens != 0) {
		coverage |= this->get_moves_queen(bsf(queens), all_pieces);
		queens &= queens - 1;
	}

	U64 bishops = this->piecesBB[WHITE_BISHOPS_ID + !this->turn];
	U64 bishops_pinning = enemy_king_lines.bishop & (bishops | queen_pinning);
	bishops &= ~bishops_pinning;

	while (bishops != 0) {
		coverage |= this->get_moves_bishop(bsf(bishops), all_pieces);
		bishops &= bishops - 1;
	}

	U64 rooks = this->piecesBB[WHITE_ROOKS_ID + !this->turn];
	U64 rooks_pinning = enemy_king_lines.rook & (rooks | queen_pinning);
	rooks &= ~rooks_pinning;

	while (rooks != 0) {
		coverage |= this->get_moves_rook(bsf(rooks), all_pieces);
		rooks &= rooks - 1;
	}
	
	U64 check_mask = 0ULL;
	if (rooks_pinning | bishops_pinning | queen_pinning) check_mask |= handle_pinning_and_checks(rooks_pinning, bishops_pinning, queen_pinning, enemy_king_index, coverage, checkers);

	U64 pawns_BB = this->piecesBB[WHITE_PAWNS_ID + !this->turn];

	if (this->turn) {
		U64 l_attacks = ((pawns_BB << 7) & NOT_FILE_H);
		U64 r_attacks = ((pawns_BB << 9) & NOT_FILE_A);
		checkers |= ((l_attacks & enemy_king) >> 7) | ((r_attacks & enemy_king) >> 9);
		this->enpassent_square &= (checkers >> 8) | ((checkers == 0) * FULL_BOARD);
		coverage |= l_attacks | r_attacks;
	}
	else {
		U64 l_attacks = ((pawns_BB >> 9) & NOT_FILE_H);
		U64 r_attacks = ((pawns_BB >> 7) & NOT_FILE_A);
		checkers |= ((l_attacks & enemy_king) << 9) | ((r_attacks & enemy_king) << 7);
		this->enpassent_square &= (checkers << 8) | ((checkers == 0) * FULL_BOARD);
		coverage |= l_attacks | r_attacks;
	}

	return std::tuple(coverage, checkers, check_mask);
}

/*
	Move and capture generator used in pseudo-legal generation process
*/

void State::update_wking_pawn_eval(U8 kingIndex) {
	U64 wpawns = this->piecesBB[WHITE_PAWNS_ID];
	U64 bpawns = this->piecesBB[BLACK_PAWNS_ID];

	KingPawns wking_pawns = this->data_table->wking_pawns[kingIndex];
	I16 eval = (I16)popcnt(wking_pawns.pawn_shield & wpawns) * 8;
	eval += (I16)popcnt((wking_pawns.pawn_shield << 8) & wpawns) * 6;
	eval -= (I16)popcnt(wking_pawns.pawn_storm & bpawns) * 8;
	this->mg_eval.extra_eval += eval;
}

void State::update_bking_pawn_eval(U8 kingIndex) {
	U64 wpawns = this->piecesBB[WHITE_PAWNS_ID];
	U64 bpawns = this->piecesBB[BLACK_PAWNS_ID];

	KingPawns bking_pawns = this->data_table->bking_pawns[kingIndex];
	I16 eval = (I16)popcnt(bking_pawns.pawn_shield & bpawns) * 8;
	eval += (I16)popcnt((bking_pawns.pawn_shield >> 8) & bpawns) * 6;
	eval -= (I16)popcnt(bking_pawns.pawn_storm & wpawns) * 8;
	this->mg_eval.extra_eval += eval;
}

U64 State::update_moves_wking(U8 squareIndex, U64 own_pieces, U64 coverage) {
	U64 moves = this->data_table->get_king_move(squareIndex) & ~own_pieces;
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];

	U64 king = (1ULL << squareIndex);
	U64 rooks = this->piecesBB[WHITE_ROOKS_ID];
	U8 allowed_king = (U8)(king >> 4);
	bool wshort = this->events.update_wshort(allowed_king & (rooks >> 7) & 1);
	bool wlong = this->events.update_wlong(allowed_king & rooks & 1);

	U64 rollout_setup = all_pieces | (coverage & WHITE_CASTLE_IGNORE);
	moves |= (((rollout_setup & WHITE_LEFT_CASTLE_PATH) == 0) & wlong) * WHITE_LEFT_CASTLE_MOVE;
	moves |= (((rollout_setup & WHITE_RIGHT_CASTLE_PATH) == 0) & wshort) * WHITE_RIGHT_CASTLE_MOVE;
	update_wking_pawn_eval(squareIndex);

	return moves & ~coverage;
}

U64 State::update_moves_bking(U8 squareIndex, U64 own_pieces, U64 coverage) {
	U64 moves = this->data_table->get_king_move(squareIndex) & ~own_pieces;
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];

	U64 king = (1ULL << squareIndex);
	U64 rooks = this->piecesBB[BLACK_ROOKS_ID];
	U8 allowed_king = (king >> 60);
	bool bshort = this->events.update_bshort(allowed_king & (rooks >> 63) & 1);
	bool blong = this->events.update_blong(allowed_king & (rooks >> 56) & 1);	

	U64 rollout_setup = all_pieces | (coverage & BLACK_CASTLE_IGNORE);
	moves |= (((rollout_setup & BLACK_LEFT_CASTLE_PATH) == 0) & blong) * BLACK_LEFT_CASTLE_MOVE;
	moves |= (((rollout_setup & BLACK_RIGHT_CASTLE_PATH) == 0) & bshort) * BLACK_RIGHT_CASTLE_MOVE;
	update_bking_pawn_eval(squareIndex);

	return moves & ~coverage;
}

U64 State::update_moves_wking_checked(U8 squareIndex, U64 own_pieces, U64 coverage) {
	U64 rooks = this->piecesBB[WHITE_ROOKS_ID];
	U8 allowed_king = (U8)_rotl64(0x1000000000000000, squareIndex);
	this->events.update_wshort(allowed_king & (rooks >> 7) & 1);
	this->events.update_wlong(allowed_king & rooks & 1);
	update_wking_pawn_eval(squareIndex);

	return this->data_table->get_king_move(squareIndex) & ~(own_pieces | coverage);
}

U64 State::update_moves_bking_checked(U8 squareIndex, U64 own_pieces, U64 coverage) {
	U64 rooks = this->piecesBB[BLACK_ROOKS_ID];
	U8 allowed_king = (U8)_rotl64(0x4, squareIndex);
	this->events.update_bshort(allowed_king & (rooks >> 63) & 1);
	this->events.update_blong(allowed_king & (rooks >> 56) & 1);
	update_bking_pawn_eval(squareIndex);

	return this->data_table->get_king_move(squareIndex) & ~(own_pieces | coverage);
}

U64 State::update_moves_knight(U8 squareIndex, U64 own_pieces) {
	U64 moves = this->data_table->get_knight_move(squareIndex) & ~own_pieces;
	U64 legal_moves = moves & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += knight_mobility_scores[popcnt(legal_moves & this->enpassent_square)];
	this->eg_eval.extra_eval += knight_mobility_scores_eg[popcnt(legal_moves & this->enpassent_square)];
	return legal_moves;
}

inline void State::insert_pawn_moves(U64 moves, I8 offset, Move*& mv) {
	while (moves != 0) {
		U8 attackIndex = bsf(moves);
		*(mv++) = Move(attackIndex + offset, attackIndex);
		moves &= moves - 1;
	}
}

inline void State::insert_white_promo_moves(U64 moves, U8 squareIndex, Move*& mv) {
	while (moves != 0) {
		U8 targetIndex = bsf(moves);
		*(mv++) = Move(squareIndex, targetIndex, WHITE_QUEENS_ID);
		*(mv++) = Move(squareIndex, targetIndex, WHITE_ROOKS_ID);
		*(mv++) = Move(squareIndex, targetIndex, WHITE_BISHOPS_ID);
		*(mv++) = Move(squareIndex, targetIndex, WHITE_KNIGHTS_ID);
		moves &= moves - 1;
	}
}

inline void State::insert_black_promo_moves(U64 moves, U8 squareIndex, Move*& mv) {
	while (moves != 0) {
		U8 targetIndex = bsf(moves);
		*(mv++) = Move(squareIndex, targetIndex, BLACK_QUEENS_ID);
		*(mv++) = Move(squareIndex, targetIndex, BLACK_ROOKS_ID);
		*(mv++) = Move(squareIndex, targetIndex, BLACK_BISHOPS_ID);
		*(mv++) = Move(squareIndex, targetIndex, BLACK_KNIGHTS_ID);
		moves &= moves - 1;
	}
}

template<PawnTypes T, PawnTypes U, PawnTypes V>
void State::insert_wpawn_template(U64 pinned_wpawns, U64 empty_squares, U64 captures, U64 check_mask, Move*& mv) {
	while (pinned_wpawns != 0) {
		U8 pawn_index = bsf(pinned_wpawns);

		U64 moves = BIT9 << pawn_index & empty_squares;
		if constexpr (!IS_PROMO) moves |= (moves << 8) & RANK_4 & empty_squares;
		moves |= this->data_table->get_wpawn_move(pawn_index) & captures;

		if constexpr (IS_PINNED) moves &= get_pinned_line(pawn_index);
		if constexpr (IS_CHECKED) moves &= check_mask;

		if constexpr (IS_PROMO) insert_white_promo_moves(moves, pawn_index, mv);
		else extract_moves(moves, pawn_index, mv);

		pinned_wpawns &= pinned_wpawns - 1;
	}
}

template<PawnTypes T, PawnTypes U, PawnTypes V>
void State::insert_bpawn_template(U64 pinned_wpawns, U64 empty_squares, U64 captures, U64 check_mask, Move*& mv) {
	while (pinned_wpawns != 0) {
		U8 pawn_index = bsf(pinned_wpawns);

		U64 moves = _rotl64(BIT56, pawn_index) & empty_squares;
		if constexpr (!IS_PROMO) moves |= moves >> 8 & RANK_5 & empty_squares;
		moves |= this->data_table->get_bpawn_move(pawn_index) & captures;

		if constexpr (IS_PINNED) moves &= get_pinned_line(pawn_index);
		if constexpr (IS_CHECKED) moves &= check_mask;

		if constexpr (IS_PROMO) insert_black_promo_moves(moves, pawn_index, mv);
		else extract_moves(moves, pawn_index, mv);

		pinned_wpawns &= pinned_wpawns - 1;
	}
}

template<PawnTypes U>
Move* State::update_wpawns(U64 pawns, U64 check_mask, Move* mv) {
	U64 captures;
	if constexpr (IS_CHECKED) captures = (this->piecesBB[BLACK_PIECES_ID] & check_mask) | this->enpassent_square;
	else captures = this->piecesBB[BLACK_PIECES_ID] | this->enpassent_square;
	U64 empty_squares = this->piecesBB[EMPTY_ID];

	U64 promotion_pawns = pawns & RANK_7;
	U64 pinned_wpawns = this->pinned_pawns & ~promotion_pawns;
	pawns ^= pinned_wpawns | promotion_pawns;

	U64 one_up = (pawns << 8) & empty_squares;
	U64 two_up = one_up << 8 & RANK_4 & empty_squares;
	U64 left_attack = (pawns << 7) & NOT_FILE_H & captures;
	U64 right_attack = (pawns << 9) & NOT_FILE_A & captures;
	
	if constexpr (IS_CHECKED) {
		one_up &= check_mask;
		two_up &= check_mask;
	}

	insert_pawn_moves(one_up, -8, mv);
	insert_pawn_moves(two_up, -16, mv);
	insert_pawn_moves(left_attack, -7, mv);
	insert_pawn_moves(right_attack, -9, mv);

	if constexpr (IS_CHECKED) {
		insert_wpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::None>(pinned_wpawns, empty_squares, captures, check_mask, mv);
		insert_wpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::Promo>(promotion_pawns, empty_squares, captures, check_mask, mv);
	}
	else {
		insert_wpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::None>(pinned_wpawns, empty_squares, captures, check_mask, mv);
		insert_wpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::Promo>(promotion_pawns, empty_squares, captures, check_mask, mv);
	}

	return mv;
}

template<PawnTypes U>
Move* State::update_bpawns(U64 pawns, U64 check_mask, Move* mv) {
	U64 captures;
	if constexpr (IS_CHECKED) captures = (this->piecesBB[WHITE_PIECES_ID] & check_mask) | this->enpassent_square;
	else captures = this->piecesBB[WHITE_PIECES_ID] | this->enpassent_square;

	U64 empty_squares = this->piecesBB[EMPTY_ID];

	U64 promotion_pawns = pawns & RANK_2;
	U64 pinned_bpawns = this->pinned_pawns & ~promotion_pawns;
	pawns ^= pinned_bpawns | promotion_pawns;

	U64 one_up = (pawns >> 8) & empty_squares;
	U64 two_up = one_up >> 8 & RANK_5 & empty_squares;
	U64 left_attack = (pawns >> 9) & NOT_FILE_H & captures;
	U64 right_attack = (pawns >> 7) & NOT_FILE_A & captures;

	if constexpr (IS_CHECKED) {
		one_up &= check_mask;
		two_up &= check_mask;
	}

	insert_pawn_moves(one_up, 8, mv);
	insert_pawn_moves(two_up, 16, mv);
	insert_pawn_moves(left_attack, 9, mv);
	insert_pawn_moves(right_attack, 7, mv);

	if constexpr (IS_CHECKED) {
		insert_bpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::None>(pinned_bpawns, empty_squares, captures, check_mask, mv);
		insert_bpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::Promo>(promotion_pawns, empty_squares, captures, check_mask, mv);
	}		   
	else {	   
		insert_bpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::None>(pinned_bpawns, empty_squares, captures, check_mask, mv);
		insert_bpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::Promo>(promotion_pawns, empty_squares, captures, check_mask, mv);
	}

	return mv;
}

U64 State::update_moves_rook(U8 squareIndex, U64 own_pieces) {
	U64 moves = this->data_table->get_rook_move(squareIndex, this->piecesBB[ALL_PIECES_ID]) & ~own_pieces;
	U64 legal_moves = moves & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += rook_mobility_scores[popcnt(legal_moves & this->zobrist_hash)];
	this->eg_eval.extra_eval += rook_mobility_scores_eg[popcnt(legal_moves & this->zobrist_hash)];
	return legal_moves;
}

U64 State::update_moves_bishop(U8 squareIndex, U64 own_pieces) {
	U64 moves = this->data_table->get_bishop_move(squareIndex, this->piecesBB[ALL_PIECES_ID]) & ~own_pieces;
	U64 legal_moves = moves & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += bishop_mobility_scores[popcnt(legal_moves & this->zobrist_hash)];
	this->eg_eval.extra_eval += bishop_mobility_scores_eg[popcnt(legal_moves & this->zobrist_hash)];
	return legal_moves;
}

U64 State::update_moves_queen(U8 squareIndex, U64 own_pieces) {
	U64 moves = this->data_table->get_queen_move(squareIndex, this->piecesBB[ALL_PIECES_ID]) & ~own_pieces;
	U64 legal_moves = moves & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += queen_mobility_scores[popcnt(legal_moves & this->zobrist_hash)];
	this->eg_eval.extra_eval += queen_mobility_scores_eg[popcnt(legal_moves & this->zobrist_hash)];
	return legal_moves;
}

U64 State::get_captures_rook(U8 squareIndex, U64 enemy_pieces) {
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];
	U64 moves = this->data_table->get_rook_move(squareIndex, all_pieces) & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += rook_mobility_scores[popcnt(moves & this->zobrist_hash)];
	this->eg_eval.extra_eval += rook_mobility_scores_eg[popcnt(moves & this->zobrist_hash)];
	return moves & enemy_pieces;
}

U64 State::get_captures_bishop(U8 squareIndex, U64 enemy_pieces) {
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];
	U64 moves = this->data_table->get_bishop_move(squareIndex, all_pieces) & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += bishop_mobility_scores[popcnt(moves & this->zobrist_hash)];
	this->eg_eval.extra_eval += bishop_mobility_scores_eg[popcnt(moves & this->zobrist_hash)];
	return moves & enemy_pieces;
}

U64 State::get_captures_queen(U8 squareIndex, U64 enemy_pieces) {
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];
	U64 moves = this->data_table->get_queen_move(squareIndex, all_pieces) & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += queen_mobility_scores[popcnt(moves & this->zobrist_hash)];
	this->eg_eval.extra_eval += queen_mobility_scores_eg[popcnt(moves & this->zobrist_hash)];
	return moves & enemy_pieces;
}

U64 State::get_captures_wking(U8 squareIndex, U64 enemy_pieces, U64 coverage) {
	U64 rooks = this->piecesBB[WHITE_ROOKS_ID];
	U8 allowed_king = (U8)_rotl64(0x1000000000000000, squareIndex);
	this->events.update_wshort(allowed_king & (rooks >> 7) & 1);
	this->events.update_wlong(allowed_king & rooks & 1);
	update_wking_pawn_eval(squareIndex);

	return this->data_table->get_king_move(squareIndex) & enemy_pieces & ~coverage;
}

U64 State::get_captures_bking(U8 squareIndex, U64 enemy_pieces, U64 coverage) {
	U64 rooks = this->piecesBB[BLACK_ROOKS_ID];
	U8 allowed_king = (U8)_rotl64(0x4, squareIndex);
	this->events.update_bshort(allowed_king & (rooks >> 63) & 1);
	this->events.update_blong(allowed_king & (rooks >> 56) & 1);
	update_bking_pawn_eval(squareIndex);

	return this->data_table->get_king_move(squareIndex) & enemy_pieces & ~coverage;
}

U64 State::get_captures_knight(U8 squareIndex, U64 enemy_pieces) {
	U64 moves = this->data_table->get_knight_move(squareIndex) & this->get_pinned_line(squareIndex);
	this->mg_eval.extra_eval += knight_mobility_scores[popcnt(moves & this->zobrist_hash)];
	this->eg_eval.extra_eval += knight_mobility_scores_eg[popcnt(moves & this->zobrist_hash)];
	return moves & enemy_pieces;
}

U64 State::get_moves_king(U8 squareIndex, U64) { return this->data_table->get_king_move(squareIndex); }
U64 State::get_moves_knight(U8 squareIndex, U64) { return this->data_table->get_knight_move(squareIndex); }
U64 State::get_moves_rook(U8 squareIndex, U64 all_pieces) { return this->data_table->get_rook_move(squareIndex, all_pieces); }
U64 State::get_moves_bishop(U8 squareIndex, U64 all_pieces) { return this->data_table->get_bishop_move(squareIndex, all_pieces); }
U64 State::get_moves_queen(U8 squareIndex, U64 all_pieces) { return this->data_table->get_queen_move(squareIndex, all_pieces); }

U64 State::get_rook_rays_custom(U64 squareIndex, U64 pieces) { return this->data_table->get_rook_move(squareIndex, pieces); }
U64 State::get_bishop_rays_custom(U64 squareIndex, U64 pieces) { return this->data_table->get_bishop_move(squareIndex, pieces); }


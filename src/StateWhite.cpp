#include "State.h"

constexpr U64(State::* board_move_functions[18])(U64, U64, U8, U8) = {
	&State::moveNull,
	&State::moveNull,
	&State::moveWhitePawnsDouble,
	&State::moveBlackPawnsDouble,
	&State::moveWhitePawnsEnpassent,
	&State::moveBlackPawnsEnpassent,
	&State::moveNull,
	&State::moveNull,
	&State::moveWhitePawnsPromo,
	&State::moveBlackPawnsPromo,
	&State::moveNull,
	&State::moveNull,
	&State::moveNull,
	&State::moveNull,
	&State::moveNull,
	&State::moveNull,
	&State::moveWhiteKingCastle,
	&State::moveBlackKingCastle,
};

StateWhite::StateWhite(StateBlack& s) : State(static_cast<State&>(s)) {}

StateBlack* StateWhite::internal_move_aligned(Move& mv, AlignedState* aligned_state) {
	StateBlack* new_state = new (aligned_state) StateBlack(*this);

	new_state->internal_move_inplace(mv);
	new_state->update_moves_and_squares();

	return new_state;
}

StateBlack* StateWhite::null_move_aligned(AlignedState* aligned_state) {
	StateBlack* new_state = new (aligned_state) StateBlack(*this);
	new_state->zobrist_hash ^= this->data_table->get_zhash_turn() ^ this->data_table->get_zobrist_hash_index(832 + this->events.get_data()) ^ this->data_table->get_zobrist_hash_index(bsf(enpassent_square >> 40));
	new_state->null_move = true;
	new_state->enpassent_square = 0ULL;
	new_state->mg_eval.base_eval *= -1;
	new_state->eg_eval.base_eval *= -1;
	new_state->update_moves_and_squares();
	return new_state;
}

StateBlack* StateWhite::move_board_update(Move& mv, AlignedState* aligned_state) {
	StateBlack* new_state = new (aligned_state) StateBlack(*this);
	new_state->internal_move_inplace(mv);
	return new_state;
}

void StateWhite::update_moves_start(U64 coverage, U64 check_mask, U64 checkers) {
	if (checkers == 0) { this->update_moves(coverage); }
	else if (__popcnt64(checkers) == 1) { in_check = true; this->update_moves_check(coverage, check_mask); }
	else {
		in_check = true;
		U64 king = this->piecesBB[WHITE_KING_ID];
		U64 friendly_pieces_BB = this->piecesBB[WHITE_PIECES_ID];

		Move* mv = this->move_iter;
		U8 pieceIndex = bsf(king);
		U64 moveBB = this->update_moves_wking_checked(pieceIndex, friendly_pieces_BB, coverage);
		extract_moves(moveBB, pieceIndex, mv);
		this->move_iter = mv;
	}
}

void StateWhite::update_moves(U64 coverage) {
	Move* mv = this->move_iter;
	mv = update_wpawns<PawnTypes::None>(this->piecesBB[WHITE_PAWNS_ID], FULL_BOARD, mv);

	U64 friendly_pieces_BB = this->piecesBB[WHITE_PIECES_ID];
	update_move_template(WHITE_KNIGHTS_ID, friendly_pieces_BB, mv, &State::update_moves_knight);
	update_move_template(WHITE_BISHOPS_ID, friendly_pieces_BB, mv, &State::update_moves_bishop);
	update_move_template(WHITE_QUEENS_ID, friendly_pieces_BB, mv, &State::update_moves_queen);
	update_move_template(WHITE_ROOKS_ID, friendly_pieces_BB, mv, &State::update_moves_rook);

	U8 pieceIndex = bsf(this->piecesBB[WHITE_KING_ID]);
	U64 moveBB = this->update_moves_wking(pieceIndex, friendly_pieces_BB, coverage);
	extract_moves(moveBB, pieceIndex, mv);

	this->move_iter = mv;
}

void StateWhite::update_moves_check(U64 coverage, U64 check_mask) {
	Move* mv = this->move_iter;
	mv = update_wpawns<PawnTypes::Checked>(this->piecesBB[WHITE_PAWNS_ID], check_mask, mv);

	U64 friendly_pieces_BB = this->piecesBB[WHITE_PIECES_ID];
	update_move_check_template(WHITE_KNIGHTS_ID, friendly_pieces_BB, check_mask, mv, &State::update_moves_knight);
	update_move_check_template(WHITE_BISHOPS_ID, friendly_pieces_BB, check_mask, mv, &State::update_moves_bishop);
	update_move_check_template(WHITE_QUEENS_ID, friendly_pieces_BB, check_mask, mv, &State::update_moves_queen);
	update_move_check_template(WHITE_ROOKS_ID, friendly_pieces_BB, check_mask, mv, &State::update_moves_rook);

	U8 pieceIndex = bsf(this->piecesBB[WHITE_KING_ID]);
	U64 moveBB = this->update_moves_wking_checked(pieceIndex, friendly_pieces_BB, coverage);
	extract_moves(moveBB, pieceIndex, mv);

	this->move_iter = mv;
}

void StateWhite::update_captures_start(U64 coverage, U64 check_mask, U64 checkers) {
	if (checkers == 0) { this->update_captures(coverage); }
	else if (__popcnt64(checkers) == 1) { in_check = true; this->update_captures_check(coverage, check_mask); }
	else {
		in_check = true;

		Move* mv = this->move_iter;
		U8 pieceIndex = bsf(this->piecesBB[WHITE_KING_ID]);
		U64 moveBB = this->get_captures_wking(pieceIndex, this->piecesBB[BLACK_PIECES_ID], coverage);
		extract_moves(moveBB, pieceIndex, mv);
		this->move_iter = mv;
	}
}

void StateWhite::update_captures(U64 coverage) {
	Move* mv = this->move_iter;
	mv = get_captures_wpawn<PawnTypes::None>(this->piecesBB[WHITE_PAWNS_ID], FULL_BOARD, mv);

	U8 enemy_king_index = bsf(this->piecesBB[BLACK_KING_ID]);
	U64 enemy_pieces_BB = this->piecesBB[BLACK_PIECES_ID];
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];
	U64 all_pieces_inv = ~all_pieces;

	U64 knight_checks = this->get_moves_knight(enemy_king_index, 0ULL) & all_pieces_inv;
	update_move_template(WHITE_KNIGHTS_ID, enemy_pieces_BB | knight_checks, mv, &State::get_captures_knight);

	U64 bishop_checks = this->get_moves_bishop(enemy_king_index, all_pieces) & all_pieces_inv;
	update_move_template(WHITE_BISHOPS_ID, enemy_pieces_BB | bishop_checks, mv, &State::get_captures_bishop);

	U64 rook_checks = this->get_moves_rook(enemy_king_index, all_pieces) & all_pieces_inv;
	update_move_template(WHITE_QUEENS_ID, enemy_pieces_BB | rook_checks, mv, &State::get_captures_queen);

	update_move_template(WHITE_ROOKS_ID, enemy_pieces_BB | bishop_checks | rook_checks, mv, &State::get_captures_rook);

	U8 kingIndex = bsf(this->piecesBB[WHITE_KING_ID]);
	U64 moveBB = this->get_captures_wking(kingIndex, enemy_pieces_BB, coverage);
	extract_moves(moveBB, kingIndex, mv);

	this->move_iter = mv;
}

void StateWhite::update_captures_check(U64 coverage, U64 check_mask) {
	Move* mv = this->move_iter;
	mv = get_captures_wpawn<PawnTypes::Checked>(this->piecesBB[WHITE_PAWNS_ID], check_mask, mv);

	U64 enemy_pieces_BB = this->piecesBB[BLACK_PIECES_ID];
	update_move_check_template(WHITE_KNIGHTS_ID, enemy_pieces_BB, check_mask, mv, &State::get_captures_knight);
	update_move_check_template(WHITE_BISHOPS_ID, enemy_pieces_BB, check_mask, mv, &State::get_captures_bishop);
	update_move_check_template(WHITE_QUEENS_ID, enemy_pieces_BB, check_mask, mv, &State::get_captures_queen);
	update_move_check_template(WHITE_ROOKS_ID, enemy_pieces_BB, check_mask, mv, &State::get_captures_rook);

	U8 kingIndex = bsf(this->piecesBB[WHITE_KING_ID]);
	U64 moveBB = this->get_captures_wking(kingIndex, enemy_pieces_BB, coverage);
	extract_moves(moveBB, kingIndex, mv);

	this->move_iter = mv;
}

void StateWhite::update_moves_and_squares() {
	U64 og_zhash = zobrist_hash;
	auto [coverage, checkers, check_mask] = update_covered_squares();

	update_moves_start(coverage, check_mask, checkers);
	zobrist_hash = og_zhash ^ this->data_table->get_zobrist_hash_index(832 + events.get_data()) ^ this->data_table->get_zobrist_hash_index(bsf(enpassent_square >> 40));
}

void StateWhite::update_captures_and_squares() {
	U64 og_zhash = zobrist_hash;
	auto [coverage, checkers, check_mask] = update_covered_squares();

	update_captures_start(coverage, check_mask, checkers);
	zobrist_hash = og_zhash ^ this->data_table->get_zobrist_hash_index(832 + events.get_data()) ^ this->data_table->get_zobrist_hash_index(bsf(enpassent_square >> 40));
}

void StateWhite::update_pawn_structure_eval() {
	PTableEntry& pEntry = this->data_table->get_ptable_entry(this->pawn_zhash);
	if (pEntry.softEquals(this->pawn_zhash)) {
		I16 pawn_score = pEntry.score;
		this->mg_eval.base_eval += pawn_score;
		this->eg_eval.base_eval += pawn_score;
	}
	else {
		I16 pawn_score = get_pawn_structure_eval();
		pEntry.setHash(this->pawn_zhash);
		pEntry.score = pawn_score;
		this->mg_eval.base_eval += pawn_score;
		this->eg_eval.base_eval += pawn_score;
	}
}

void StateWhite::internal_move_inplace(Move& mv) {
	U8 fromIndex = mv.from();
	U8 toIndex = mv.to();

	U8 fromPieceID = this->squareOcc[fromIndex];
	U8 toPieceID = this->squareOcc[toIndex];
	
	this->mg_eval.base_eval *= -1;
	this->eg_eval.base_eval *= -1;
	this->update_move_eval(fromIndex, toIndex, fromPieceID, toPieceID, this->events.wscore());

	this->squareOcc[fromIndex] = EMPTY_ID;
	this->squareOcc[toIndex] = fromPieceID;

	U64 fromPieceBB = (1ULL << fromIndex);
	U64 toPieceBB = (1ULL << toIndex);

	this->piecesBB[fromPieceID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[toPieceID] ^= toPieceBB;

	U64 enp_square = this->enpassent_square;
	zobrist_hash ^= this->data_table->get_zobrist_hash_index(832 + this->events.get_data())
		^ this->data_table->get_zobrist_hash(fromIndex, fromPieceID)
		^ this->data_table->get_zobrist_hash(toIndex, fromPieceID)
		^ this->data_table->get_zobrist_hash(toIndex, toPieceID)
		^ this->data_table->get_zobrist_hash_index(bsf(enp_square >> 16))
		^ this->data_table->get_zhash_turn();

	U8 move_index = 1, promoID = 0;
	if (fromPieceID == BLACK_PAWNS_ID) {
		promoID = mv.promotion();
		U8 pawn_enpassent = (toPieceBB == enp_square) << 2;
		U8 pawn_promotion = (promoID != 0) << 3;
		move_index |= pawn_enpassent | pawn_promotion;
		this->enpassent_square = (fromPieceBB >> 8) & (toPieceBB << 8);
		this->pawn_zhash ^= this->data_table->get_zhash_bpawn_table(fromIndex) ^ this->data_table->get_zhash_bpawn_table(toIndex);;
	}
	else {
		move_index |= ((abs((int)(fromIndex - toIndex)) == 2) & (fromPieceID == BLACK_KING_ID)) << 4;
		this->enpassent_square = 0ULL;
	}

	if (toPieceID == WHITE_PAWNS_ID) this->pawn_zhash ^= this->data_table->get_zhash_wpawn_table(toIndex);

	if (move_index == 1) this->moveBlackPiece(fromPieceBB, toPieceBB);
	else (this->*board_move_functions[move_index])(fromPieceBB, toPieceBB, promoID, toIndex);
}

std::tuple<U64, U64, U64> StateWhite::update_covered_squares() {
	U64 pawns_BB = this->piecesBB[BLACK_PAWNS_ID];
	U64 enemy_king = this->piecesBB[WHITE_KING_ID];

	U64 l_attacks = ((pawns_BB >> 9) & NOT_FILE_H);
	U64 r_attacks = ((pawns_BB >> 7) & NOT_FILE_A);
	U64 checkers = ((l_attacks & enemy_king) << 9) | ((r_attacks & enemy_king) << 7);
	U64 coverage = l_attacks | r_attacks;
	zobrist_hash = ~coverage;

	coverage |= this->get_moves_king(bsf(this->piecesBB[BLACK_KING_ID]), 0ULL);

	U8 enemy_king_index = bsf(enemy_king);
	U64 knights = this->piecesBB[BLACK_KNIGHTS_ID];
	checkers |= (this->get_moves_knight(enemy_king_index, 0ULL) & knights);
	add_to_coverage(knights, 0ULL, coverage, &State::get_moves_knight);

	QueenLine enemy_king_lines = this->data_table->queen_lines[enemy_king_index];
	U64 all_pieces = this->piecesBB[ALL_PIECES_ID];

	U64 queens = this->piecesBB[BLACK_QUEENS_ID];
	U64 queen_pinning = enemy_king_lines.queen & queens;
	queens ^= queen_pinning;
	add_to_coverage(queens, all_pieces, coverage, &State::get_moves_queen);

	U64 bishops = this->piecesBB[BLACK_BISHOPS_ID];
	U64 bishops_pinning = enemy_king_lines.bishop & (bishops | queen_pinning);
	bishops &= ~bishops_pinning;
	add_to_coverage(bishops, all_pieces, coverage, &State::get_moves_bishop);

	U64 rooks = this->piecesBB[BLACK_ROOKS_ID];
	U64 rooks_pinning = enemy_king_lines.rook & (rooks | queen_pinning);
	rooks &= ~rooks_pinning;
	add_to_coverage(rooks, all_pieces, coverage, &State::get_moves_rook);

	U64 check_mask = checkers;
	if (rooks_pinning | bishops_pinning | queen_pinning) check_mask |= handle_pinning_and_checks(rooks_pinning, bishops_pinning, queen_pinning, enemy_king_index, coverage, checkers);
	check_mask += (check_mask == 0) * FULL_BOARD;

	this->enpassent_square &= (check_mask << 8);
	return std::tuple(coverage, checkers, check_mask);
}

void StateWhite::perft_all_moves(U8 depth, U64& total_moves) {
	if (depth == 1) { total_moves += (this->move_iter - this->move_arr); return; }

	HTableEntryPerft& zentry = this->data_table->get_perft_entry(this->zobrist_hash);
	if (zentry.softEquals(zobrist_hash) && zentry.depth == depth) { total_moves += zentry.perft_moves; return; }

	U64 start_moves = total_moves;
	AlignedState aligned_st;
	for (Move* mv = this->move_arr; mv != this->move_iter; mv++) this->internal_move_aligned(*mv, &aligned_st)->perft_all_moves(depth - 1, total_moves);

	zentry.setHash(zobrist_hash);
	zentry.depth = depth;
	zentry.perft_moves = (U32)(total_moves - start_moves);
}

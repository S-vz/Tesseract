#include "State.h"

#include <iostream>
#include <string>
#include <cstring>

#include "ChessConstants.h"

#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <map>
#include <ctype.h>

constexpr U64 (State::*board_move_functions[18])(U64, U64, U8, U8) = {
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

U8 State::pawn_implicit_promo_check(Move& mv) {
	if ((1ULL << mv.to()) & (RANK_1 | RANK_8)) 
		return (this->turn) ? BLACK_QUEENS_ID : WHITE_QUEENS_ID;
	return 0;
}

bool State::is_irreversible_move(Move& mv) {
	if (this->squareOcc[mv.to()] != EMPTY_ID) return true; // Capture

	U8 fromIndex = mv.from();
	U8 from_ID = this->squareOcc[fromIndex];
	if (from_ID <= 1) return true; // Pawn move

	// Castle right losses
	if (from_ID == WHITE_KING_ID) return this->events.wshort() || this->events.wlong(); 
	if (from_ID == BLACK_KING_ID) return this->events.bshort() || this->events.blong();

	if (from_ID == WHITE_ROOKS_ID) return ((fromIndex == 0) && this->events.wlong()) || ((fromIndex == 7) && this->events.wshort());
	if (from_ID == BLACK_ROOKS_ID) return ((fromIndex == 56) && this->events.blong()) || ((fromIndex == 63) && this->events.bshort());

	return false;

}

void State::deriveSquareOcc() {
	this->squareOcc.fill(EMPTY_ID);
	this->zobrist_hash = 0ULL;
	for (U8 pieceType = 0; pieceType < 12; pieceType++) {
		U64 pieceBB = this->piecesBB[pieceType];
		while (pieceBB != 0) {
			U8 pieceIndex = (U8)bsf(pieceBB);
			this->squareOcc[pieceIndex] = pieceType;
			this->zobrist_hash ^= this->data_table->get_zobrist_hash(pieceIndex, pieceType);
			pieceBB ^= 1ULL << pieceIndex;
		}
	}
}

void State::derivePiecesBB() {
	this->piecesBB.fill(0);
	for (int square = 0; square < 64; square++) {
		U8 pieceID = this->squareOcc[square];
		this->piecesBB[pieceID] |= 1ULL << square;
		if (pieceID == EMPTY_ID) continue;
		this->piecesBB[WHITE_PIECES_ID + (pieceID % 2)] |= 1ULL << square;
		this->piecesBB[ALL_PIECES_ID] |= 1ULL << square;
	}
	this->piecesBB[EMPTY_ID] = ~this->piecesBB[ALL_PIECES_ID];
}

void State::full_eval() {
	this->mg_eval.base_eval = 0;
	this->eg_eval.base_eval = 0;
	this->total_material = 0;
	for (U8 i = 0; i < 64; i++) {
		U8 pieceType = squareOcc[i];
		this->mg_eval.base_eval += this->data_table->eval.get_pos_eval(pieceType, i) + piece_eval[pieceType];
		this->eg_eval.base_eval += this->data_table->eval.get_pos_eval_eg(pieceType, i) + piece_eval_eg[pieceType];
		this->total_material += piece_score_phases[pieceType];
	}
	if (this->turn) {
		this->mg_eval.base_eval *= -1;
		this->eg_eval.base_eval *= -1;
	}
}

void State::update_move_eval(U8 fromIndex, U8 toIndex, U8 fromPieceID, U8 toPieceID, I16 castle_score) {
	this->mg_eval.base_eval += this->data_table->eval.get_double_pos_eval(fromPieceID, fromIndex, toIndex) - this->data_table->eval.get_pos_eval(toPieceID, toIndex) - piece_eval[toPieceID];
	this->eg_eval.base_eval += this->data_table->eval.get_double_pos_eval_eg(fromPieceID, fromIndex, toIndex) - this->data_table->eval.get_pos_eval_eg(toPieceID, toIndex) - piece_eval_eg[toPieceID];

	this->mg_eval.extra_eval += castle_score;
	this->eg_eval.extra_eval += castle_score;

	this->total_material -= piece_score_phases[toPieceID];
}

I16 State::get_pawn_structure_eval() {
	U64 wpawns = this->piecesBB[WHITE_PAWNS_ID];
	U64 bpawns = this->piecesBB[BLACK_PAWNS_ID];

	U64 wpawn_attacks = (((wpawns << 7) & NOT_FILE_H) | ((wpawns << 9) & NOT_FILE_A)) << 8;
	U64 bpawn_attacks = (((bpawns >> 9) & NOT_FILE_H) | ((bpawns >> 7) & NOT_FILE_A)) >> 8;

	auto process_pawn_structure = [&](U64 friendly_pawnsBB, U64 enemy_pawnsBB, U64 attacks, std::array<PawnStructure, 64>& pawn_structure_arr) {
		U64 pawns_copy = friendly_pawnsBB;
		I16 score = 0;
		while (pawns_copy != 0) {
			U8 pIndex = bsf(pawns_copy);
			PawnStructure& pStruct = pawn_structure_arr[pIndex];

			if (friendly_pawnsBB & pStruct.partial_file) { // Doubled pawn
				U8 adjecented_enemy_pawns = (U8)popcnt(enemy_pawnsBB & pStruct.adjecent_forward_files);
				if (adjecented_enemy_pawns >= 2) score -= 5;
				else if (adjecented_enemy_pawns == 1) score -= 10;
				else score -= 15;
			}
			else if ((enemy_pawnsBB & (pStruct.partial_file | pStruct.adjecent_forward_files)) == 0) score += 10; // Passed pawn
			else if ((friendly_pawnsBB & pStruct.adjecent_back_files) == 0) {
				if ((friendly_pawnsBB & pStruct.adjecent_files) == 0) score -= 3; // Isolated pawn
				else if (attacks & (1ULL << pIndex)) score -= 2; // Backward pawn
			}
			pawns_copy &= pawns_copy - 1;
		}
		return score;
	};

	I16 wpawn_score = process_pawn_structure(wpawns, bpawns, bpawn_attacks, this->data_table->wpawn_structure);
	I16 bpawn_score = process_pawn_structure(bpawns, wpawns, wpawn_attacks, this->data_table->bpawn_structure);
	return wpawn_score - bpawn_score;
}


// Tapered eval, uses a weighted average between the middlegame and endgame score. Same trick as MadChess and Fruit to use bitshifts instead of divide
I16 State::get_eval() {
	I32 material_score = std::min(total_material, (U16)256);
	I32 mg_score = (this->mg_eval.base_eval + this->mg_eval.extra_eval) * material_score;
	I32 eg_score = (this->eg_eval.base_eval + this->eg_eval.extra_eval) * (256 - material_score);
	return (I16)((mg_score + eg_score) >> 8);
}

void State::recalc_zobrist() {
	this->zobrist_hash = 0ULL;
	this->pawn_zhash = 0ULL;
		for (U16 i = 0; i < 64; i++) {
		U8 pieceType = this->squareOcc[i];
		this->zobrist_hash ^= this->data_table->get_zobrist_hash(i, pieceType);
		if (pieceType == WHITE_PAWNS_ID) this->pawn_zhash ^= this->data_table->get_zhash_wpawn_table(i);
		else if (pieceType == BLACK_PAWNS_ID) this->pawn_zhash ^= this->data_table->get_zhash_bpawn_table(i);
	}
	if (this->turn) {
		this->zobrist_hash ^= this->data_table->get_zhash_turn();
		this->zobrist_hash ^= this->data_table->get_zobrist_hash_index(bsf(enpassent_square >> 16));
	}
	else this->zobrist_hash ^= this->data_table->get_zobrist_hash_index(bsf(enpassent_square >> 40));
	this->zobrist_hash ^= this->data_table->get_zobrist_hash_index(832 + this->events.get_data());
}

void State::printUnicodeBoardStringSimple() {
	const char* piecesUnicodeInverse[13] = { "P", "p", "N", "n", "B", "b", "R", "r", "K", "k", "Q", "q", "." };

	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			std::cout << *piecesUnicodeInverse[this->squareOcc[i * 8 + j]];
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

void State::printUnicodeBoardString() {
	_setmode(_fileno(stdout), _O_U16TEXT);

	const wchar_t* piecesUnicode[13] = { L"♙", L"♟︎", L"♘", L"♞", L"♗", L"♝", L"♖", L"♜", L"♔", L"♚", L"♕", L"♛",  L"." };
	const wchar_t* piecesUnicodeInverse[13] = { L"♟︎", L"♙", L"♞", L"♘", L"♝", L"♗", L"♜", L"♖", L"♚", L"♔", L"♛", L"♕", L"." };

	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			std::wcout << *piecesUnicodeInverse[this->squareOcc[i * 8 + j]];
		}
		std::wcout << std::endl;
	}
	std::wcout << std::endl;

	_setmode(_fileno(stdout), _O_TEXT);
}

void State::loadFenString(std::string fen) {
	this->move_iter = move_arr;
	std::map<char, U8> pieceType{ 
		{'P', (U8)0},
		{'p', (U8)1},
		{'N', (U8)2},
		{'n', (U8)3},
		{'B', (U8)4},
		{'b', (U8)5},
		{'R', (U8)6},
		{'r', (U8)7},
		{'K', (U8)8},
		{'k', (U8)9},
		{'Q', (U8)10},
		{'q', (U8)11},
	};
	auto fen_split = splitString(fen, ' ');

	U8 index = 0;
	std::string piece_data = fen_split[0];
	std::reverse(piece_data.begin(), piece_data.end());
	for (char ch : piece_data) {
		if (isdigit(ch)) {
			U8 target = index + (ch - 48);
			for (; index < target; index++) this->squareOcc[index] = EMPTY_ID;
			continue;
		}
		else if (ch == '/') continue;
		this->squareOcc[index] = pieceType[ch];
		index++;
	}
	for (int i = 0; i < 9; i++)	std::reverse(this->squareOcc.begin() + (i - 1) * 8, this->squareOcc.begin() + i * 8);
	this->derivePiecesBB();

	this->turn = fen_split[1] == "b";

	if (fen_split[2] == "-") this->events = { 0b0000 };
	else {
		bool wshort = fen_split[2].find('K') != std::string::npos;
		bool wlong = fen_split[2].find('Q') != std::string::npos;
		bool bshort = fen_split[2].find('k') != std::string::npos;
		bool blong = fen_split[2].find('q') != std::string::npos;
		this->events = { wshort, wlong, bshort, blong };
	}

	if (fen_split[3] == "-") this->enpassent_square = 0ULL;
	else this->enpassent_square = 1ULL << ((fen_split[3][0] - 97) + (fen_split[3][1] - 49) * 8);

	this->pinned_pawns = 0ULL;
	this->pinnedPieceBB = {};
	update_moves_and_squares();
	recalc_zobrist();
	full_eval();
}

std::string State::toFenString() {
	constexpr char fenDict[12] = { 'P', 'p', 'N', 'n', 'B', 'b', 'R', 'r', 'K', 'k', 'Q', 'q' };

	U64 empty_counter = 0;
	std::string fen = "";

	for (int i = 7; i >= 0; i--) {
		for (int j = 0; j < 8; j++) {
			int index = i * 8 + j;
			if (this->squareOcc[index] == EMPTY_ID) {
				empty_counter++;
			}
			else if (empty_counter != 0) {
				fen.append(std::to_string(empty_counter));
				empty_counter = 0;
				fen.push_back(fenDict[this->squareOcc[index]]);
			}
			else fen.push_back(fenDict[this->squareOcc[index]]);
		}
		if (i != 0) {
			if (empty_counter != 0) {
				fen.append(std::to_string(empty_counter));
				empty_counter = 0;
			}
			fen.append("/");
		}
	}
	if (empty_counter != 0) fen.append(std::to_string(empty_counter));
	(this->turn) ? fen.append(" b ") : fen.append(" w ");
	fen.append(this->events.toFen());
	(this->enpassent_square) ? fen.append(" " + indexToString(bsf(enpassent_square))) : fen.append(" -");
	return fen;
}

void State::construct_startpos(DataTable* mtable) {
	this->piecesBB = {
		START_WHITE_PAWNS,
		START_BLACK_PAWNS,
		START_WHITE_KNIGHTS,
		START_BLACK_KNIGHTS,
		START_WHITE_BISHOPS,
		START_BLACK_BISHOPS,
		START_WHITE_ROOKS,
		START_BLACK_ROOKS,
		START_WHITE_KING,
		START_BLACK_KING,
		START_WHITE_QUEENS,
		START_BLACK_QUEENS,
		START_EMPTY_FIELDS,
		START_WHITE_PIECES,
		START_BLACK_PIECES,
		START_ALL_PIECES,
	};

	this->pinnedPieceBB = {};
	this->events = CastleEvents();
	this->data_table = mtable;
	this->turn = WHITE;

	this->enpassent_square = 0ULL;
	this->pinned_pawns = 0ULL;
	this->move_iter = move_arr;

	deriveSquareOcc();
	update_moves(0ULL);
	recalc_zobrist();
	full_eval();
}

State State::internal_move(Move& mv) {
	State new_state = State(*this);
	
	new_state.internal_move_inplace(mv);
	new_state.update_moves_and_squares();

	return new_state;
}

void State::update_moves_and_squares() {
	auto [coverage, checkers, check_mask] = update_covered_squares();

	check_mask |= checkers;
	check_mask += (check_mask == 0) * FULL_BOARD; // If there are no checkers use the full board

	update_moves_start(coverage, check_mask, checkers);
}

void State::internal_move_inplace(Move& mv) {
	U8 fromIndex = mv.from();
	U8 toIndex = mv.to();
	U8 promoID = 0;

	U8 fromPieceID = this->squareOcc[fromIndex];
	U8 toPieceID = this->squareOcc[toIndex];

	this->squareOcc[fromIndex] = EMPTY_ID;
	this->squareOcc[toIndex] = fromPieceID;

	U64 fromPieceBB = (1ULL << fromIndex);
	U64 toPieceBB = (1ULL << toIndex);

	this->piecesBB[fromPieceID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[toPieceID] ^= toPieceBB;

	U8 move_index = fromPieceID & 1;
	U8 pawn_identifier = (fromPieceID <= 1);

	// Skip double pawn function when seperating State
	if (pawn_identifier) {
		promoID = mv.promotion();
		U8 pawn_double = (std::abs(((int)toIndex - (int)fromIndex)) == 16) << 1;
		U8 pawn_enpassent = (toPieceBB == this->enpassent_square) << 2;
		U8 pawn_promotion = (promoID != 0) << 3;
		move_index |= pawn_double | pawn_enpassent | pawn_promotion;
	}
	else {
		U8 castle_identifier = (((((toPieceBB >> 2) | (toPieceBB << 2)) & fromPieceBB) != 0) & ((fromPieceID & 7) <= 1)) << 4;
		move_index |= castle_identifier;
	}

	U64 old_enp = enpassent_square;

	if (move_index <= 1) this->movePiece(fromPieceBB, toPieceBB);
	else (this->*board_move_functions[move_index])(fromPieceBB, toPieceBB, promoID, toIndex);

	this->zobrist_hash ^= this->data_table->get_zobrist_hash(fromIndex, fromPieceID)
		^ this->data_table->get_zobrist_hash(toIndex, fromPieceID)
		^ this->data_table->get_zobrist_hash(toIndex, toPieceID)
		^ this->data_table->get_zobrist_hash_index(56 + (bsf(old_enp) & 7))
		^ this->data_table->get_zobrist_hash_index(56 + (bsf(enpassent_square) & 7))
		^ this->data_table->get_zhash_turn();
}

void State::movePiece(U64 fromPieceBB, U64 toPieceBB) {
	U64 empty_squares = this->piecesBB[EMPTY_ID] | fromPieceBB;
	this->piecesBB[WHITE_PIECES_ID + !this->turn] ^= fromPieceBB | toPieceBB;
	this->piecesBB[WHITE_PIECES_ID + this->turn] &= ~toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
	this->enpassent_square = 0ULL;
}

void State::moveWhitePiece(U64 fromPieceBB, U64 toPieceBB) {
	U64 empty_squares = this->piecesBB[EMPTY_ID] | fromPieceBB;
	this->piecesBB[WHITE_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[BLACK_PIECES_ID] &= ~toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
}

void State::moveBlackPiece(U64 fromPieceBB, U64 toPieceBB) {
	U64 empty_squares = this->piecesBB[EMPTY_ID] | fromPieceBB;
	this->piecesBB[WHITE_PIECES_ID] &= ~toPieceBB;
	this->piecesBB[BLACK_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
}

U64 State::moveWhitePawnsDouble(U64 fromPieceBB, U64 toPieceBB, U8, U8) {
	U64 empty_squares = this->piecesBB[EMPTY_ID] | fromPieceBB;
	this->piecesBB[WHITE_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
	this->enpassent_square = fromPieceBB << 8;
	return 0ULL;
}

U64 State::moveBlackPawnsDouble(U64 fromPieceBB, U64 toPieceBB, U8, U8) {
	U64 empty_squares = this->piecesBB[EMPTY_ID] | fromPieceBB;
	this->piecesBB[BLACK_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
	this->enpassent_square = fromPieceBB >> 8;
	return 0ULL;
}

U64 State::moveWhitePawnsEnpassent(U64 fromPieceBB, U64 toPieceBB, U8, U8 toIndex) {
	U64 enpassent_pawn = toPieceBB >> 8;
	U8 enpassent_index = toIndex - 8;

	this->squareOcc[enpassent_index] = EMPTY_ID;

	U64 empty_pieces = this->piecesBB[EMPTY_ID] | enpassent_pawn | fromPieceBB;

	this->piecesBB[BLACK_PAWNS_ID] ^= enpassent_pawn;
	this->piecesBB[WHITE_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[BLACK_PIECES_ID] &= ~(enpassent_pawn | toPieceBB);
	this->piecesBB[EMPTY_ID] = empty_pieces;

	this->piecesBB[ALL_PIECES_ID] = ~empty_pieces;
	this->enpassent_square = 0ULL;

	this->pawn_zhash ^= this->data_table->get_zhash_bpawn_table(enpassent_index);
	this->zobrist_hash ^= this->data_table->get_zobrist_hash(enpassent_index, BLACK_PAWNS_ID);
	this->mg_eval.base_eval -= this->data_table->eval.get_pos_eval(BLACK_PAWNS_ID, toIndex) + piece_eval[BLACK_PAWNS_ID];
	this->eg_eval.base_eval -= this->data_table->eval.get_pos_eval_eg(BLACK_PAWNS_ID, toIndex) + piece_eval_eg[BLACK_PAWNS_ID];

	return this->data_table->queen_lines[enpassent_index].queen;
}

U64 State::moveBlackPawnsEnpassent(U64 fromPieceBB, U64 toPieceBB, U8, U8 toIndex) {
	U64 enpassent_pawn = toPieceBB << 8;
	U8 enpassent_index = toIndex + 8;

	this->squareOcc[enpassent_index] = EMPTY_ID;

	U64 empty_pieces = this->piecesBB[EMPTY_ID] | enpassent_pawn | fromPieceBB;

	this->piecesBB[WHITE_PAWNS_ID] ^= enpassent_pawn;
	this->piecesBB[WHITE_PIECES_ID] &= ~(enpassent_pawn | toPieceBB);
	this->piecesBB[BLACK_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_pieces;

	this->piecesBB[ALL_PIECES_ID] = ~empty_pieces;
	this->enpassent_square = 0ULL;

	this->pawn_zhash ^= this->data_table->get_zhash_wpawn_table(enpassent_index);
	this->zobrist_hash ^= this->data_table->get_zobrist_hash(enpassent_index, WHITE_PAWNS_ID);
	this->mg_eval.base_eval -= this->data_table->eval.get_pos_eval(WHITE_PAWNS_ID, toIndex) + piece_eval[WHITE_PAWNS_ID];
	this->eg_eval.base_eval -= this->data_table->eval.get_pos_eval_eg(WHITE_PAWNS_ID, toIndex) + piece_eval_eg[WHITE_PAWNS_ID];

	return this->data_table->queen_lines[enpassent_index].queen;
}

U64 State::moveWhitePawnsPromo(U64 fromPieceBB, U64 toPieceBB, U8 promoID, U8 toIndex) {
	U64 empty_squares = this->piecesBB[EMPTY_ID] | fromPieceBB;
	this->piecesBB[WHITE_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[BLACK_PIECES_ID] &= ~toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
	this->enpassent_square = 0ULL;

	this->piecesBB[WHITE_PAWNS_ID] ^= toPieceBB;
	this->piecesBB[promoID] |= toPieceBB;
	this->squareOcc[toIndex] = promoID;

	this->pawn_zhash ^= this->data_table->get_zhash_wpawn_table(toIndex);
	this->zobrist_hash ^= this->data_table->get_zobrist_hash(bsf(toPieceBB), WHITE_PAWNS_ID) ^ this->data_table->get_zobrist_hash(toIndex, promoID);
	this->mg_eval.base_eval += this->data_table->eval.get_pos_eval(promoID, toIndex) + piece_eval[promoID] - piece_eval[WHITE_PAWNS_ID];
	this->eg_eval.base_eval += this->data_table->eval.get_pos_eval_eg(promoID, toIndex) + piece_eval[promoID] - piece_eval_eg[WHITE_PAWNS_ID];
	this->total_material += piece_score_phases[promoID];

	return 0ULL;
}

U64 State::moveBlackPawnsPromo(U64 fromPieceBB, U64 toPieceBB, U8 promoID, U8 toIndex) {
	U64 empty_squares = this->piecesBB[EMPTY_ID] | fromPieceBB;
	this->piecesBB[WHITE_PIECES_ID] &= ~toPieceBB;
	this->piecesBB[BLACK_PIECES_ID] ^= fromPieceBB | toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
	this->enpassent_square = 0ULL;

	this->piecesBB[BLACK_PAWNS_ID] ^= toPieceBB;
	this->piecesBB[promoID] |= toPieceBB;
	this->squareOcc[toIndex] = promoID;

	this->pawn_zhash ^= this->data_table->get_zhash_bpawn_table(toIndex);
	this->zobrist_hash ^= this->data_table->get_zobrist_hash(bsf(toPieceBB), BLACK_PAWNS_ID) ^ this->data_table->get_zobrist_hash(toIndex, promoID);
	this->mg_eval.base_eval += this->data_table->eval.get_pos_eval(promoID, toIndex) + piece_eval[promoID] - piece_eval[BLACK_PAWNS_ID];
	this->eg_eval.base_eval += this->data_table->eval.get_pos_eval_eg(promoID, toIndex) + piece_eval[promoID] - piece_eval_eg[BLACK_PAWNS_ID];
	this->total_material += piece_score_phases[promoID];

	return 0ULL;
}

U64 State::moveWhiteKingCastle(U64 fromPieceBB, U64 toPieceBB, U8, U8 toIndex) {
	U8 right_castle = toIndex == 6;
	U8 from_rook_index = 0 + (right_castle * 7);
	U8 to_rook_index = 3 + (right_castle << 1);
	this->events.wcastled();

	this->squareOcc[from_rook_index] = EMPTY_ID;
	this->squareOcc[to_rook_index] = WHITE_ROOKS_ID;

	U64 from_rook_BB = (1ULL << from_rook_index);
	U64 rook_mask = from_rook_BB | (1ULL << to_rook_index);

	U64 empty_squares = (this->piecesBB[EMPTY_ID] | fromPieceBB) ^ rook_mask;
	this->piecesBB[WHITE_ROOKS_ID] ^= rook_mask;
	this->piecesBB[WHITE_PIECES_ID] ^= rook_mask | fromPieceBB | toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
	this->enpassent_square = 0ULL;

	this->zobrist_hash ^= this->data_table->get_zobrist_hash(from_rook_index, WHITE_ROOKS_ID) ^ this->data_table->get_zobrist_hash(to_rook_index, WHITE_ROOKS_ID);
	this->mg_eval.base_eval += this->data_table->eval.get_double_pos_eval(WHITE_ROOKS_ID, from_rook_index, to_rook_index);
	this->eg_eval.base_eval += this->data_table->eval.get_double_pos_eval_eg(WHITE_ROOKS_ID, from_rook_index, to_rook_index);

	return this->data_table->queen_lines[from_rook_index].queen | this->data_table->queen_lines[to_rook_index].queen;
}

U64 State::moveBlackKingCastle(U64 fromPieceBB, U64 toPieceBB, U8, U8 toIndex) {
	U8 right_castle = toIndex == 62;
	U8 from_rook_index = 56 + (right_castle * 7);
	U8 to_rook_index = 59 + (right_castle << 1);
	this->events.bcastled();

	this->squareOcc[from_rook_index] = EMPTY_ID;
	this->squareOcc[to_rook_index] = BLACK_ROOKS_ID;

	U64 from_rook_BB = (1ULL << from_rook_index);
	U64 rook_mask = from_rook_BB | (1ULL << to_rook_index);

	U64 empty_squares = (this->piecesBB[EMPTY_ID] | fromPieceBB) ^ rook_mask;
	this->piecesBB[BLACK_ROOKS_ID] ^= rook_mask;
	this->piecesBB[BLACK_PIECES_ID] ^= rook_mask | fromPieceBB | toPieceBB;
	this->piecesBB[EMPTY_ID] = empty_squares;
	this->piecesBB[ALL_PIECES_ID] = ~empty_squares;
	this->enpassent_square = 0ULL;

	this->zobrist_hash ^= this->data_table->get_zobrist_hash(from_rook_index, BLACK_ROOKS_ID) ^ this->data_table->get_zobrist_hash(to_rook_index, BLACK_ROOKS_ID);
	this->mg_eval.base_eval += this->data_table->eval.get_double_pos_eval(BLACK_ROOKS_ID, from_rook_index, to_rook_index);
	this->eg_eval.base_eval += this->data_table->eval.get_double_pos_eval_eg(BLACK_ROOKS_ID, from_rook_index, to_rook_index);

	return this->data_table->queen_lines[from_rook_index].queen | this->data_table->queen_lines[to_rook_index].queen;
}


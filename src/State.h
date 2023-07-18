#pragma once

#include <bitset>
#include <vector>
#include <variant>
#include "DataGenerator.h"
#include "ChessConstants.h"
#include "DataTable.h"

constexpr U64 BIT9 = 0x100;
constexpr U64 BIT56 = 0x100000000000000;

constexpr U8 NO_PIN_INDEX = 64;

/*
	Castling 
*/

constexpr U64 WHITE_CASTLE_IGNORE = 0xffffffffffffff6c;
constexpr U64 WHITE_LEFT_CASTLE_PATH = 0xe;
constexpr U64 WHITE_RIGHT_CASTLE_PATH = 0x60;
constexpr U64 WHITE_LEFT_CASTLE_MOVE = 0x4;
constexpr U64 WHITE_RIGHT_CASTLE_MOVE = 0x40;

constexpr U64 BLACK_CASTLE_IGNORE = 0x6cffffffffffffff;
constexpr U64 BLACK_LEFT_CASTLE_PATH = 0xe00000000000000;
constexpr U64 BLACK_RIGHT_CASTLE_PATH = 0x6000000000000000;
constexpr U64 BLACK_LEFT_CASTLE_MOVE = 0x400000000000000;
constexpr U64 BLACK_RIGHT_CASTLE_MOVE = 0x4000000000000000;

struct CastleEvents {
	U8 data;

	CastleEvents() : data(0b00001111) {}
	CastleEvents(U8 o_data) : data(o_data) {}
	CastleEvents(U8 wShortCastle, U8 wLongCastle, U8 bShortCastle, U8 bLongCastle) : data(wShortCastle | (wLongCastle << 1) | (bShortCastle << 2) | (bLongCastle << 3)) {}

	U8 get_data() { return data & 0b1111; }

	bool wshort() { return (data & 0b00000001); }
	bool wlong() { return (data & 0b00000010); }
	bool bshort() { return (data & 0b00000100); }
	bool blong() { return (data & 0b00001000); }

	bool update_wshort(bool allowed) { 
		data &= ~(!allowed << 0);
		return wshort();
	}

	bool update_wlong(bool allowed) {
		data &= ~(!allowed << 1);
		return wlong();
	}

	bool update_bshort(bool allowed) {
		data &= ~(!allowed << 2);
		return bshort();
	}

	bool update_blong(bool allowed) {
		data &= ~(!allowed << 3);
		return blong();
	}

	void wcastled() { data |= 0b00010000; }
	void bcastled() { data |= 0b00100000; }

	I16 wscore() {
		I16 castle_score = ((data & 0b00010000) >> 4) * 40;
		castle_score -= (data & 0b00000001) * 7 + ((data & 0b00000010) << 2);
		return castle_score;
	}

	I16 bscore() {
		I16 castle_score = ((data & 0b00100000) >> 5) * 40;
		castle_score -= ((data & 0b00000100) >> 2) * 7 + ((data & 0b00001000));
		return castle_score;
	}

	std::string toFen() {
		std::string fen_str = "";
		if (wshort()) fen_str.append("K");
		if (wlong()) fen_str.append("Q");
		if (bshort()) fen_str.append("k");
		if (blong()) fen_str.append("q");
		return (fen_str == "") ? "-" : fen_str;
	}
};

enum class PawnTypes {
	None,
	Pinned,
	Checked,
	Promo,
};

struct EvalPackage {
	I16 base_eval;
	I16 extra_eval = 0;

	EvalPackage() : base_eval(0) {}
	EvalPackage(EvalPackage& oth) : base_eval(oth.base_eval) {}
};

#define IS_PINNED (T == PawnTypes::Pinned)
#define IS_CHECKED (U == PawnTypes::Checked)
#define IS_PROMO (V == PawnTypes::Promo)

/*
	 Snapshot of all data at an arbitrary moment in time
	 Utilizes mostly branchless code.
	 
	 Has a Bitboard class built inside of it instead having one externally.
	 This is for effiency reasons, and comes at the cost of readability.
*/

class State {
public:
	// Bitboard data
	std::array<U8, 64> pinnedPieceBB = {};
	std::array<U8, 64> squareOcc;
	std::array<U64, 16> piecesBB;

	// State data
	U8 turn;
	CastleEvents events;
	DataTable* data_table;

	U64 zobrist_hash;
	U64 pawn_zhash;
	U64 enpassent_square;
	U64 pinned_pawns = 0ULL;

	Move move_arr[218];
	Move* move_iter = move_arr;

	U16 total_material = 0;

	EvalPackage mg_eval;
	EvalPackage eg_eval;

	bool in_check = false;
	bool null_move = false;

	//std::vector<Move> past_moves;

	State(DataTable* mtable) { construct_startpos(mtable); }

	State(State& s) :
		piecesBB(s.piecesBB),
		squareOcc(s.squareOcc),
		//past_moves(s.past_moves),

		events(s.events),
		data_table(s.data_table),

		pawn_zhash(s.pawn_zhash),
		zobrist_hash(s.zobrist_hash),
		enpassent_square(s.enpassent_square),

		total_material(s.total_material),
		mg_eval(s.mg_eval),
		eg_eval(s.eg_eval),	

		turn(s.turn ^ 1)
	{}

	/*
		Bitboard related functions
	*/

	U8 pawn_implicit_promo_check(Move& mv);
	bool is_irreversible_move(Move& mv);
	void construct_startpos(DataTable* mtable);

	I16 get_eval();
	I16 get_pawn_structure_eval();
	void update_move_eval(U8, U8, U8, U8, I16);
	void full_eval();

	void deriveSquareOcc();
	void derivePiecesBB();
	void printUnicodeBoardString();
	void printUnicodeBoardStringSimple();
	
	State fromFen(std::string fen);
	void loadFenString(std::string fen);
	std::string toFenString();
	void recalc_zobrist();
	void movePiece(U64 fromPieceBB, U64 toPieceBB);
	constexpr U64 moveNull(U64, U64, U8, U8) { return 0ULL; }

	void moveWhitePiece(U64, U64);
	U64 moveWhitePawnsDouble(U64, U64, U8, U8);
	U64 moveWhitePawnsEnpassent(U64, U64, U8, U8);
	U64 moveWhitePawnsPromo(U64 fromPieceBB, U64 toPieceBB, U8 promoID, U8 toIndex);
	U64 moveWhiteKingCastle(U64, U64, U8, U8);
	
	void moveBlackPiece(U64, U64);
	U64 moveBlackPawnsDouble(U64, U64, U8, U8);
	U64 moveBlackPawnsEnpassent(U64, U64, U8, U8);
	U64 moveBlackPawnsPromo(U64, U64, U8, U8);
	U64 moveBlackKingCastle(U64, U64, U8, U8);


	/*
		Game State related functions
	*/

	State move_raw(U8 fromIndex, U8 toIndex);
	State internal_move(Move& mv);
	void internal_move_inplace(Move& mv);

	void perft_all_moves(U64 depth, U64& total_moves);

	U64 handle_sliding_checkers(U64 queen_checkers, U64 rook_checker, U64 bishop_checker, U64 kingIndexMultiplied, U64& coverage);
	U64 handle_pinning_and_checks(U64 rook_pinning, U64 bishop_pinning, U64 queen_pinning, U8 enemy_king, U64& coverage, U64& checkers);

	void update_moves_and_squares();
	void update_moves_start(U64 coverage, U64 check_mask, U64 checkers);
	void update_moves(U64 coverage);
	void update_moves_check(U64 coverage, U64 check_mask);

	std::tuple<U64, U64, U64> update_covered_squares();
	void add_to_coverage(U64, U64, U64&, U64(State::*get_moves_func)(U8, U64));
	void handle_enpassent_pin(U64 pawns, U64 king, U64 coverage, U64 rook_rays);

	void extract_moves(U64 moves, U8 squareIndex, Move*& mv);
	void update_move_template(U64 piecesID, U64 friendly_pieces_BB, Move*& mv, U64(State::* move_func)(U8, U64));
	void update_move_check_template(U64 piecesID, U64 friendly_pieces_BB, U64 check_mask, Move*& mv, U64(State::* move_func)(U8, U64));
	constexpr U64 get_pinned_line(U64 squareIndex);

	void insert_pawn_moves(U64 moves, I8 offset, Move*& mv);
	void insert_white_promo_moves(U64 moves, U8 squareIndex, Move*& mv);
	void insert_black_promo_moves(U64 moves, U8 squareIndex, Move*& mv);

	template<PawnTypes T, PawnTypes U, PawnTypes V>
	void insert_wpawn_template(U64 pinned_wpawns, U64 empty_squares, U64 captures, U64 check_mask, Move*& mv);

	template<PawnTypes T, PawnTypes U, PawnTypes V>
	void insert_bpawn_template(U64 pinned_wpawns, U64 empty_squares, U64 captures, U64 check_mask, Move*& mv);
	
	template<PawnTypes U>
	Move* update_wpawns(U64 pawns, U64, Move* mv);

	template<PawnTypes U>
	Move* update_bpawns(U64 pawns, U64, Move* mv);

	void update_wking_pawn_eval(U8 kingIndex);
	void update_bking_pawn_eval(U8 kingIndex);

	U64 update_moves_wking(U8 squareIndex, U64 own_pieces, U64);
	U64 update_moves_bking(U8 squareIndex, U64 own_pieces, U64);
	U64 update_moves_wking_checked(U8 squareIndex, U64 own_pieces, U64 coverage);
	U64 update_moves_bking_checked(U8 squareIndex, U64 own_pieces, U64 coverage);

	U64 update_moves_knight(U8 squareIndex, U64 own_pieces);
	U64 update_moves_rook(U8 squareIndex, U64 own_pieces);
	U64 update_moves_bishop(U8 squareIndex, U64 own_pieces);
	U64 update_moves_queen(U8 squareIndex, U64 own_pieces);

	// Captures are used for faster quiescence search
	U64 get_captures_wking(U8 squareIndex, U64 enemy_pieces, U64 coverage);
	U64 get_captures_bking(U8 squareIndex, U64 enemy_pieces, U64 coverage);
	U64 get_captures_knight(U8 squareIndex, U64 enemy_pieces);
	U64 get_captures_queen(U8 squareIndex, U64 own_pieces);
	U64 get_captures_rook(U8 squareIndex, U64 own_pieces);
	U64 get_captures_bishop(U8 squareIndex, U64 own_pieces);

	U64 get_moves_king(U8 squareIndex, U64);
	U64 get_moves_knight(U8 squareIndex, U64);
	U64 get_moves_rook(U8 squareIndex, U64 all_pieces);
	U64 get_moves_bishop(U8 squareIndex, U64 all_pieces);
	U64 get_moves_queen(U8 squareIndex, U64 all_pieces);

	U64 get_rook_rays_custom(U64, U64);
	U64 get_bishop_rays_custom(U64, U64);

	/*
		I need to do declare the function here because C++ is terrible and there is a 
		MSVC compiler bug or something that causes it to miss these functions.
	*/

	template<PawnTypes U>
	Move* get_captures_wpawn(U64 pawns, U64 check_mask, Move* mv) {
		U64 captures;
		if constexpr (IS_CHECKED) captures = (this->piecesBB[BLACK_PIECES_ID] & check_mask) | this->enpassent_square;
		else captures = this->piecesBB[BLACK_PIECES_ID] | this->enpassent_square;

		U64 promotion_pawns = pawns & RANK_7;
		U64 pinned_wpawns = this->pinned_pawns & ~promotion_pawns;
		pawns ^= pinned_wpawns | promotion_pawns;

		U64 left_attack = (pawns << 7) & NOT_FILE_H & captures;
		U64 right_attack = (pawns << 9) & NOT_FILE_A & captures;

		insert_pawn_moves(left_attack, -7, mv);
		insert_pawn_moves(right_attack, -9, mv);

		if constexpr (IS_CHECKED) {
			insert_wpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::None>(pinned_wpawns, 0ULL, captures, check_mask, mv);
			insert_wpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::Promo>(promotion_pawns, 0ULL, captures, check_mask, mv);
		}
		else {
			insert_wpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::None>(pinned_wpawns, 0ULL, captures, check_mask, mv);
			insert_wpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::Promo>(promotion_pawns, 0ULL, captures, check_mask, mv);
		}

		return mv;
	}

	template<PawnTypes U>
	Move* get_captures_bpawn(U64 pawns, U64 check_mask, Move* mv) {
		U64 captures;
		if constexpr (IS_CHECKED) captures = (this->piecesBB[WHITE_PIECES_ID] & check_mask) | this->enpassent_square;
		else captures = this->piecesBB[WHITE_PIECES_ID] | this->enpassent_square;

		U64 promotion_pawns = pawns & RANK_2;
		U64 pinned_bpawns = this->pinned_pawns & ~promotion_pawns;
		pawns ^= pinned_bpawns | promotion_pawns;

		U64 left_attack = (pawns >> 9) & NOT_FILE_H & captures;
		U64 right_attack = (pawns >> 7) & NOT_FILE_A & captures;

		insert_pawn_moves(left_attack, 9, mv);
		insert_pawn_moves(right_attack, 7, mv);

		if constexpr (IS_CHECKED) {
			insert_bpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::None>(pinned_bpawns, 0ULL, captures, check_mask, mv);
			insert_bpawn_template<PawnTypes::Pinned, PawnTypes::Checked, PawnTypes::Promo>(promotion_pawns, 0ULL, captures, check_mask, mv);
		}
		else {
			insert_bpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::None>(pinned_bpawns, 0ULL, captures, check_mask, mv);
			insert_bpawn_template<PawnTypes::Pinned, PawnTypes::None, PawnTypes::Promo>(promotion_pawns, 0ULL, captures, check_mask, mv);
		}

		return mv;
	}

};

class StateWhite;
class StateBlack;
using AlignedState = std::aligned_storage<sizeof(State), alignof(State)>::type;

class StateWhite: public State {
public:
	StateWhite() : State(&DataTable::getInstance()) {}
	StateWhite(DataTable* mv_table) : State(mv_table) {}
	StateWhite(StateBlack& s);

	void update_moves_start(U64 coverage, U64 check_mask, U64 checkers);
	void update_moves(U64 coverage);
	void update_moves_check(U64 coverage, U64 check_mask);

	void update_captures_start(U64 coverage, U64 check_mask, U64 checkers);
	void update_captures(U64 coverage);
	void update_captures_check(U64 coverage, U64 check_mask);

	void update_pawn_structure_eval();
	void update_moves_and_squares();
	void update_captures_and_squares();
	std::tuple<U64, U64, U64> update_covered_squares();

	StateBlack* move_board_update(Move& mv, AlignedState* aligned_state);
	StateBlack* internal_move_aligned(Move& mv, AlignedState* aligned_state);
	StateBlack* null_move_aligned(AlignedState* aligned_state);
	void internal_move_inplace(Move& mv);
	void perft_all_moves(U8 depth, U64& total_moves);
};

class StateBlack : public State {
public:
	StateBlack() : State(&DataTable::getInstance()) {}
	StateBlack(DataTable* mv_table) : State(mv_table) {}
	StateBlack(StateWhite& s); 

	void update_moves_start(U64 coverage, U64 check_mask, U64 checkers);
	void update_moves(U64 coverage);
	void update_moves_check(U64 coverage, U64 check_mask);

	void update_captures_start(U64 coverage, U64 check_mask, U64 checkers);
	void update_captures(U64 coverage);
	void update_captures_check(U64 coverage, U64 check_mask);

	void update_pawn_structure_eval();
	void update_moves_and_squares();
	void update_captures_and_squares();
	std::tuple<U64, U64, U64> update_covered_squares();

	StateWhite* move_board_update(Move& mv, AlignedState* aligned_state);
	StateWhite* internal_move_aligned(Move& mv, AlignedState* aligned_state);
	StateWhite* null_move_aligned(AlignedState* aligned_state);
	void internal_move_inplace(Move& mv);
	void perft_all_moves(U8 depth, U64& total_moves);
};

typedef std::variant<StateWhite*, StateBlack*> StateMix;

/*
	Visitor Patterns
	Used to access the underlying State in the StateMix variant.
	Minor performance impact, but helps in retaining sanity
*/

struct MoveVisitor {
	Move mv;
	AlignedState* aligned_st;
	MoveVisitor(Move& mv, AlignedState* sta) : mv(mv), aligned_st(sta) {}

	StateMix operator()(StateWhite* st) {
		StateBlack* stb = st->internal_move_aligned(mv, aligned_st);
		//std::cout << bbStr(stb->enpassent_square) << "\n";
		return StateMix(stb);
	}

	StateMix operator()(StateBlack* st) {
		StateWhite* stw = st->internal_move_aligned(mv, aligned_st);
		//std::cout << bbStr(stw->enpassent_square) << "\n";
		return StateMix(stw);
	}
};

struct NullMoveVisitor {
	AlignedState* aligned_st;
	NullMoveVisitor(AlignedState* sta) : aligned_st(sta) {}

	StateMix operator()(StateWhite* st) {
		StateBlack* stb = st->null_move_aligned(aligned_st);
		return StateMix(stb);
	}

	StateMix operator()(StateBlack* st) {
		StateWhite* stw = st->null_move_aligned(aligned_st);
		return StateMix(stw);
	}
};

// Only updates internal board data (piecesBB and squareOcc), not data related to moves.
struct PartialMoveVisitor {
	Move mv;
	I16& score;
	AlignedState* aligned_st;
	PartialMoveVisitor(Move& mv, I16& score, AlignedState* sta) : mv(mv), score(score), aligned_st(sta) {}

	StateMix operator()(StateWhite* st) {
		StateBlack* stb = st->move_board_update(mv, aligned_st);
		score = stb->get_eval();
		return StateMix(stb);
	}
	StateMix operator()(StateBlack* st) {
		StateWhite* stw = st->move_board_update(mv, aligned_st);
		score = stw->get_eval();
		return StateMix(stw);
	}
};

struct PromoCheck {
	Move mv;
	PromoCheck(Move& mv) : mv(mv) {}
	U8 operator()(auto& st) { return st->pawn_implicit_promo_check(mv); }
};

struct PerftVisitor {
	U8 depth;
	PerftVisitor(U8 depth) : depth(depth) {}
	U64 operator()(auto& st) { 
		U64 total_moves = 0ULL;
		st->perft_all_moves(depth, total_moves); 
		return total_moves;
	}
};

struct StateCast { State* operator()(auto& st) { return static_cast<State*>(st); } };
struct UpdateMoves { void operator()(auto& st) { st->update_moves_and_squares(); } };
struct UpdateCaptures { void operator()(auto& st) { st->update_captures_and_squares(); } };

struct FenString { std::string operator()(auto& st) { return st->toFenString(); } };
struct ZobristHash { U64 operator()(auto& st) { return st->zobrist_hash; } };
struct TurnCheck { U8 operator()(auto& st) { return st->turn; } };

struct PrintBoard { void operator()(auto& st) { st->printUnicodeBoardString(); } };

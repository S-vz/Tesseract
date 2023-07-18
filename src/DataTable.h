#pragma once

#include "ChessConstants.h"
#include "EvalData.h"
#include "DataGenerator.h"


struct HTableEntryPerft {
	U32 hash = 0ULL;
	U32 perft_moves = 0ULL;
	U8 depth = 0U;

	bool softEquals(U64 cmp_hash) { return hash == (cmp_hash >> 32); }
	void setHash(U64 hsh) { this->hash = (hsh >> 32); }
};

struct HTableEntry {
	U64 hash = 0ULL;
	I16 score = 0;
	Move best_move = Move(0, 0);

	// Bit packed data, contains node type, depth and bool flag for quiescent entry
	U8 node_and_depth = 0;

	bool softEquals(U64 cmp_hash) { return hash == (cmp_hash); } 

	void setHash(U64 hsh) { this->hash = (hsh); }
	void setTypeAndDepth(U8 node_type, U8 depth) { node_and_depth = (node_type << 6) | depth; }
	void setTypeAndDepthAndQuis(U8 node_type, U8 depth) { node_and_depth = (node_type << 6) | depth | 0b00100000; }

	U8 depth() { return node_and_depth & 0b00011111; }
	U8 node_type() { return node_and_depth >> 6; }
	bool is_quiesecent() { return ((node_and_depth >> 5) & 1) == 1; }
};

struct PTableEntry {
	U32 hash = 0ULL;
	I16 score = 0;

	bool softEquals(U64 cmp_hash) { return hash == (cmp_hash >> 32); }
	void setHash(U64 hsh) { this->hash = (hsh >> 32); }
};

const class DataTable {
public:
	/*
		Move Generation Data
	*/
	BoardArray all_king_moves;
	BoardArray all_knight_moves;
	BoardArray all_wpawn_moves;
	BoardArray all_bpawn_moves;

	BoardArray bishop_square_offsets;
	BoardArray rook_square_offsets;

	BoardArray rook_blocking_masks;
	BoardArray bishop_blocking_masks;

	std::array<U64, ROOK_TABLE_SIZE> all_rook_moves;
	std::array<U64, BISHOP_TABLE_SIZE> all_bishop_moves;

	std::array<QueenLine, 64> queen_lines;
	std::array<Line, 4096> line_index;

	std::vector<Move> divide_moves = {};

	/*
		Transposition Table Data
	*/

	std::array<U64, 848> zobrist_hash_table;

	HTableEntry* TPT;
	HTableEntryPerft* TPT_Perft;
	U64 TP_TABLE_SIZE = 1ULL << 24;
	U64 TP_TABLE_SIZE_ROOT = (TP_TABLE_SIZE - 1);

	/*
		Evaluation Data
	*/

	EvalData eval = EvalData();
	std::array<KingPawns, 64> wking_pawns;
	std::array<KingPawns, 64> bking_pawns;

	std::array<PawnStructure, 64> wpawn_structure;
	std::array<PawnStructure, 64> bpawn_structure;

	PTableEntry* PHT;
	U64 PH_TABLE_SIZE = 1ULL << 20; 
	U64 PH_TABLE_SIZE_ROOT = (PH_TABLE_SIZE - 1);

	DataTable() {
		generate_empty_tables();
		useSearchTable();
		generate_pawn_hash_table();
		generate_zobrist_hashes(zobrist_hash_table);
		generate_all_lines(this->line_index);
		generate_all_king_moves(&this->all_king_moves);
		generate_all_knight_moves(&this->all_knight_moves);
		generate_all_pawn_attacks(&this->all_wpawn_moves, &this->all_bpawn_moves);
		generate_all_rook_moves(&this->all_rook_moves, &this->rook_blocking_masks, &this->rook_square_offsets);
		generate_all_bishop_moves(&this->all_bishop_moves, &this->bishop_blocking_masks, &this->bishop_square_offsets);
		generate_queen_lines();
		generate_king_pawns(wking_pawns, bking_pawns);
		generate_pawn_structure(wpawn_structure, bpawn_structure, line_index);
	}

	static DataTable& getInstance() {
		static DataTable dtable;
		return dtable;
	}

	void reset_TPT() { generate_search_hash_table(TP_TABLE_SIZE); }

	void set_hash_table_size(U64 target_MB) {
		U64 target_bytes = target_MB << 20;
		U64 entry_num = target_bytes / sizeof(HTableEntry);
		TP_TABLE_SIZE = 1ULL << (U8)std::log2(entry_num);
		TP_TABLE_SIZE_ROOT = (TP_TABLE_SIZE - 1);
		generate_search_hash_table(TP_TABLE_SIZE);
		std::cout << "Number of hash table entries: 2^" << (U16)std::log2(entry_num) << "\n";
	}

	void generate_empty_tables() {
		PHT = new PTableEntry[0];
		TPT = new HTableEntry[0];
		TPT_Perft = new HTableEntryPerft[0];
	}

	void generate_pawn_hash_table() {
		delete[] PHT;
		PHT = new PTableEntry[PH_TABLE_SIZE]();
	}

	void useSearchTable() {
		generate_perft_hash_table(0);
		generate_search_hash_table(TP_TABLE_SIZE);
	}

	void usePerftTable() {
		generate_search_hash_table(0);
		generate_perft_hash_table(TP_TABLE_SIZE);
	}

	void generate_search_hash_table(U64 size) {
		delete[] TPT;
		TPT = new HTableEntry[size]();
	}

	void generate_perft_hash_table(U64 size) {
		delete[] TPT_Perft;
		TPT_Perft = new HTableEntryPerft[size]();
	}

	constexpr U64 get_zhash_turn() {
		return this->zobrist_hash_table[56];
	}

	constexpr U64 get_zobrist_hash_index(U16 zindex) {
		return this->zobrist_hash_table[zindex];
	}

	constexpr U64 get_zhash_wpawn_table(U16 zindex) {
		return this->zobrist_hash_table[zindex];
	}

	constexpr U64 get_zhash_bpawn_table(U16 zindex) {
		return this->zobrist_hash_table[zindex+64];
	}

	constexpr U64 get_zobrist_hash(U16 pieceIndex, U16 pieceID) {
		return this->zobrist_hash_table[(pieceID * (U16)64) + pieceIndex];
	}

	constexpr HTableEntry& get_zobrist_entry(U64 zhash) {
		return *(TPT + (zhash & TP_TABLE_SIZE_ROOT));
	}

	constexpr HTableEntryPerft& get_perft_entry(U64 zhash) {
		return *(TPT_Perft + (zhash & TP_TABLE_SIZE_ROOT));
	}

	constexpr PTableEntry& get_ptable_entry(U64 zhash) {
		return *(PHT + (zhash & PH_TABLE_SIZE_ROOT));
	}

	constexpr U64 get_king_move(U64 squareIndex) {
		return this->all_king_moves[squareIndex];
	}

	constexpr U64 get_knight_move(U64 squareIndex) {
		return this->all_knight_moves[squareIndex];
	}

	constexpr U64 get_pawn_move(U64 squareIndex, int color) {
		return (color == WHITE) ? this->all_wpawn_moves[squareIndex] : this->all_bpawn_moves[squareIndex];
	}

	constexpr U64 get_wpawn_move(U64 squareIndex) {
		return this->all_wpawn_moves[squareIndex];
	}

	constexpr U64 get_bpawn_move(U64 squareIndex) {
		return this->all_bpawn_moves[squareIndex];
	}

	constexpr void generate_queen_lines() {
		for (int i = 0; i < 64; i++) {
			U64 rook = get_rook_move(i, 0ULL);
			U64 bishop = get_bishop_move(i, 0ULL);
			this->queen_lines[i] = { rook | bishop, rook, bishop };
		}
	}

	U64 get_rook_move(U64 squareIndex, U64 occuppationBB) {
		U64 pext_blocking_mask = pext(occuppationBB, this->rook_blocking_masks[squareIndex]);
		return this->all_rook_moves[this->rook_square_offsets[squareIndex] + pext_blocking_mask];
	}

	U64 get_bishop_move(U64 squareIndex, U64 occuppationBB) {
		U64 pext_blocking_mask = pext(occuppationBB, this->bishop_blocking_masks[squareIndex]);
		return this->all_bishop_moves[this->bishop_square_offsets[squareIndex] + pext_blocking_mask];
	}

	U64 get_queen_move(U64 squareIndex, U64 occuppationBB) {
		return get_rook_move(squareIndex, occuppationBB) | get_bishop_move(squareIndex, occuppationBB);
	}
};


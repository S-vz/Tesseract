#pragma once

#include <chrono>
#include <numeric>
#include <shared_mutex> 
#include <type_traits>
#include <set>
#include <map>

#include "ChessConstants.h"
#include "State.h"

constexpr U8 HASH_EXACT = 0;
constexpr U8 HASH_ALPHA = 1;
constexpr U8 HASH_BETA = 2;

// Uses built in C++ bitpacking
struct SortedMove {
	U16 mv_data : 16 = 0;
	I16 score_data : 16 = 0;

	SortedMove() {}
	SortedMove(Move mv, I16 score) : mv_data(mv.data), score_data(score) {}

	Move mv() { return { mv_data }; }
	I16 score() { return score_data; }

	bool operator<(const SortedMove& other) const { return score_data < other.score_data; }
	bool operator>(const SortedMove& other) const { return score_data > other.score_data; }
};

struct SearchStats {
	U64 total_nodes;
	U64 total_qnodes;
	U64 zobrist_hits;
	U64 time_taken;
	U8 tests_done;

	U64 nodes_searched;

	std::chrono::steady_clock::time_point time_started;
	std::vector<double> ratio_vec;
	std::vector<double> zhits_vec;
	std::vector<double> EBF_vec;
	std::vector<double> EBF_to_MBF_vec;

	SearchStats() : total_nodes(0), total_qnodes(0), nodes_searched(0), zobrist_hits(0), time_taken(0), tests_done(0) {}

	void reset() {
		time_started = std::chrono::high_resolution_clock::now();

		zobrist_hits = 0;
		total_nodes = 0;
		total_qnodes = 0;
		nodes_searched = 0;
		time_taken = 0;

		ratio_vec.clear();
		EBF_vec.clear();
		EBF_to_MBF_vec.clear();
	}

	void finish() {
		time_taken  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - time_started).count();
	}

	double get_mean(std::vector<double>& vec) { return vec.empty() ? (double)0 : std::accumulate(vec.begin(), vec.end(), (double)0) / vec.size(); }
	double get_median(std::vector<double>& vec) { 
		std::sort(vec.begin(), vec.end());
		return vec.empty() ? 0.0 : vec[vec.size() / 2];
	}

	double mean_ebf() { return get_mean(EBF_vec); }
	double median_ebf() { return get_median(EBF_vec); }

	double mean_ebf_to_mbf() { return get_mean(EBF_to_MBF_vec); }
	double median_ebf_to_mbf() { return get_median(EBF_to_MBF_vec); }

	double mean_cut_perc() { return (1 - get_mean(ratio_vec)) * 100; }
	double median_cut_perc() { return (1 - get_median(ratio_vec)) * 100; }

	double mean_time() { return time_taken / (double)tests_done; }

	void add(SearchStats& oth) {
		tests_done++;

		total_nodes += oth.total_nodes;
		total_qnodes += oth.total_qnodes;
		time_taken += oth.time_taken;
		zobrist_hits += oth.zobrist_hits;

		EBF_vec.insert(EBF_vec.end(), oth.EBF_vec.begin(), oth.EBF_vec.end());
		ratio_vec.insert(ratio_vec.end(), oth.ratio_vec.begin(), oth.ratio_vec.end());
		EBF_to_MBF_vec.insert(EBF_to_MBF_vec.end(), oth.EBF_to_MBF_vec.begin(), oth.EBF_to_MBF_vec.end());
	}
};

struct SMP_Package {
	std::shared_mutex* alpha_beta_lock;

	std::atomic<I16>* alpha_value;
	std::atomic<I16>* beta_value;

	I16 alpha() { return *this->alpha_value; }
	I16 beta() { return *this->beta_value; }

	void allocate(I16 alpha_val, I16 beta_val) {
		alpha_beta_lock = new std::shared_mutex();
		this->alpha_value = new std::atomic<I16>(alpha_val);
		this->beta_value = new std::atomic<I16>(beta_val);
	} 

	void destroy() {
		delete alpha_beta_lock;
		delete alpha_value;
		delete beta_value;
	}

	void lock() { alpha_beta_lock->lock(); }
	void unlock() { alpha_beta_lock->unlock(); }
};

struct TimePackage {
	U64 time_left;
	U64 max_thinking_time = 5000;
	bool stop_searching = false;

	void reset(U64 time_left_arg) {
		stop_searching = false;
		this->time_left = time_left_arg;
	}

	U64 get_allocated_time() { return time_left / 20; }
};

struct Regular;
struct Debug;
#define IF_DEBUG if constexpr (std::is_same<T, Debug>::value)

template <class T>
class Search {
public:
	std::vector<SortedMove> best_moves;
	std::map<U64, U8> repetition_map;

	TimePackage time_pkg;
	SearchStats stats;
	U64 current_search_ID;
	U8 start_depth;

	Move killer_moves[2][64];
	I16 history_moves[12][64] = {};

	Search() : start_depth(0) {}

	bool is_debug() { return std::is_same<T, Debug>::value; }

	bool should_stop_searching(std::chrono::steady_clock::time_point& start, I64& max_time) {
		auto time_passed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
		max_time -= time_passed;
		start = std::chrono::high_resolution_clock::now();
		return (time_pkg.stop_searching || (max_time < time_passed * 3));
	}

	void search_reset() {
		best_moves.clear();
		stats.reset();
		std::memset(history_moves, 0, sizeof(history_moves));
		std::memset(killer_moves, 0, sizeof(killer_moves));
	}

	Move timed_search(StateMix& stx, U64 time_allocated) {
		search_reset(); 
		start_timer(time_allocated);
		negamax_iterative_timed(stx, time_allocated);
		stats.finish();
		this->current_search_ID++;
		//std::cerr << "Best move score: " << best_moves.begin()->score << "\n";
		return best_moves.begin()->mv();
	}

	Move depth_search(StateMix& stx, U8 depth) {
		search_reset();
		time_pkg.reset(0);
		negamax_iterative(stx, depth);
		stats.finish();
		return best_moves.begin()->mv();
	}

	bool hasTripleRepetition() {
		U8 rep_count = 0;
		for (auto const [_, val] : this->repetition_map) 
			if (val == 2) {
				if (rep_count == 2) return true;
				rep_count++;
			}
		return false;
	}

	bool isRepetitionDraw(StateMix& stx) {
		U64 zobrist = std::visit(ZobristHash(), stx);
		U8 reps = (repetition_map.count(zobrist)) ? repetition_map[zobrist] : 0;
		if (reps == 2) return true;

		if (this->hasTripleRepetition()) {
			State& st = *std::visit(StateCast(), stx);
			if (st.get_eval() > 0) return false;
			AlignedState aligned_st;
			for (Move* mv = st.move_arr; mv != st.move_iter; mv++) {
				StateMix next_stx = std::visit(MoveVisitor{ *mv, &aligned_st }, stx);
				U64 zhash = std::visit(ZobristHash(), next_stx);
				if (repetition_map[zhash] == 2) return true;
			}
		}
		return false;
	}

	void update_stats(U64& previous_nodes) {
		this->stats.total_nodes += stats.nodes_searched;
		if (previous_nodes == 0) previous_nodes = stats.nodes_searched; 
		else {
			this->stats.EBF_vec.push_back((double)stats.nodes_searched / previous_nodes);
			previous_nodes = stats.nodes_searched;
		}
		stats.nodes_searched = 0ULL;
	}

	void start_timer(U64 time_allocated) {
		std::thread t(&Search<T>::start_timeout_sleep, this, time_allocated, current_search_ID);
		t.detach();
	}

	void start_timeout_sleep(U64 time_allocated, U64 search_ID) {
		std::this_thread::sleep_for(std::chrono::milliseconds(time_allocated));
		if (this->current_search_ID == search_ID) time_pkg.stop_searching = true;	
	}

	void negamax_iterative_timed(StateMix& stx, I64 max_time) {
		U64 previous_nodes = 0ULL;

		auto start = std::chrono::high_resolution_clock::now();
		for (start_depth = 2; start_depth <= 100; start_depth++) {
			negamax_start_threaded(stx);
			if (should_stop_searching(start, max_time)) {
				std::cerr << "Searched until depth: " << (U64)start_depth << " | Best move: " << best_moves.begin()->mv().toString() << "\n";
				return;
			}
			IF_DEBUG this->update_stats(previous_nodes);
		}
	}

	void negamax_iterative(StateMix& stx, U8 depth) {
		U64 previous_nodes = 0ULL;
		for (start_depth = 2; start_depth <= depth; start_depth++) {
			negamax_start_threaded(stx);
			IF_DEBUG this->update_stats(previous_nodes);
		}
	}

	void starting_move_search(StateMix& stx, Move mv, I16& alpha, I16& beta) {
		I16 score = 0;
		AlignedState aligned_st;
		StateMix next_stx = std::visit(MoveVisitor{ mv, &aligned_st }, stx);

		if (isRepetitionDraw(next_stx)) score = 0;
		else if (alpha != INT16_MIN + 1) 
			score = -negamax(-alpha - 1, -alpha, start_depth - 1, next_stx); // PV search
		if (alpha == INT16_MIN + 1 || (score > alpha) && (score < beta)) 
			score = -negamax(-beta, -alpha, start_depth - 1, next_stx);

		if (score > alpha) {
			alpha = score;
			this->best_moves.push_back({ mv, score });
		}
	};

	void negamax_start_threaded(StateMix& stx, I16 alpha = INT16_MIN+1, I16 beta = INT16_MAX) {
		if (time_pkg.stop_searching) return;
		State& st = *std::visit(StateCast(), stx);
		if (st.move_iter == st.move_arr) return;

		auto best_moves_copy = best_moves;
		best_moves.clear();

		for (auto mv : best_moves_copy)
			starting_move_search(stx, mv.mv(), alpha, beta);
		for (Move* mv = st.move_arr; mv != st.move_iter; mv++) 
			starting_move_search(stx, *mv, alpha, beta);
		
		std::sort(best_moves.begin(), best_moves.end(), std::greater<SortedMove>());

		// Search was not fully finished, use previous results
		if (time_pkg.stop_searching) best_moves = best_moves_copy;
	}

	I16 evalScoreTPT(HTableEntry& zentry, I16 alpha, I16 beta, U8 depth) {
		if (!zentry.is_quiesecent() && zentry.depth() >= depth) {
			U8 node_type = zentry.node_type();
			if (node_type == HASH_EXACT) return zentry.score;
			else if (node_type == HASH_ALPHA && zentry.score <= alpha) return alpha;
			else if (node_type == HASH_BETA && zentry.score >= beta) return beta;
		}
		return INT16_MAX;
	}

	I16 evalScoreQuiescenceTPT(HTableEntry& zentry, I16 alpha, I16 beta, U8 depth) {
		if (zentry.depth() >= depth) {
			U8 node_type = zentry.node_type();
			if (node_type == HASH_EXACT) return zentry.score;
			else if (node_type == HASH_ALPHA && zentry.score <= alpha) return alpha;
			else if (node_type == HASH_BETA && zentry.score >= beta) return beta;
		}
		return INT16_MAX;
	}

	I16 negamax(I16 alpha, I16 beta, I8 depth, StateMix& stx) {
		IF_DEBUG stats.nodes_searched++;
		State& st = *std::visit(StateCast(), stx);
		if (time_pkg.stop_searching) return st.get_eval();

		if (st.move_iter == st.move_arr) {
			if (st.in_check) return INT16_MIN + (start_depth - depth);
			else return 0;
		}

		// Search extension
		if (st.in_check) depth++;

		if (depth <= 1) { 
			IF_DEBUG stats.nodes_searched += st.move_iter - st.move_arr;
			return quiescent(alpha, beta, 10, stx);
		}

		Move best_move_local;
		U8 node_type = HASH_ALPHA;
		I16 best_score = INT16_MIN + 1;
		I16 score;

		AlignedState aligned_st;
		if (!st.null_move && !st.in_check) {
			StateMix next_stx = std::visit(NullMoveVisitor{ &aligned_st }, stx);
			score = -negamax(-beta, -beta + 1, depth - 3, next_stx);
			if (score >= beta) return beta;
		}

		HTableEntry& zentry_og = st.data_table->get_zobrist_entry(st.zobrist_hash);
		HTableEntry zentry = zentry_og;

		auto process_score = [&](I16 score, Move mv) {
			if (score > alpha) {
				if (st.squareOcc[mv.to()] == EMPTY_ID) 
					history_moves[st.squareOcc[mv.from()]][mv.to()] += depth;
				alpha = score;
				node_type = HASH_EXACT;
				best_score = score;
				best_move_local = mv;
			}
			else if (score > best_score) {
				best_score = score;
				best_move_local = mv;
			}
		};

		auto set_zentry = [&] (I16 score, U8 node_type, Move mv) {
			zentry.setHash(st.zobrist_hash);
			zentry.setTypeAndDepth(node_type, depth);
			zentry.best_move = mv;
			zentry.score = score;
			zentry_og = zentry;
		};

		auto PV_search = [&](Move mv) {
			StateMix next_stx = std::visit(MoveVisitor{ mv, &aligned_st }, stx);
			if (node_type == HASH_EXACT) {
				score = -negamax(-alpha - 1, -alpha, depth - 1, next_stx);
				if ((score > alpha) && (score < beta))
					score = -negamax(-beta, -alpha, depth - 1, next_stx);
			}
			else score = -negamax(-beta, -alpha, depth - 1, next_stx);
		};

		bool zobrist_hit = zentry.softEquals(st.zobrist_hash);
		if (zobrist_hit) {
			IF_DEBUG stats.zobrist_hits++;
			I16 stored_score = evalScoreTPT(zentry, alpha, beta, depth);
			if (stored_score != INT16_MAX) { return stored_score; }
		}

		std::vector<SortedMove> moves = sort_moves<true>(st, depth, zentry, zobrist_hit);
		for (auto& smv : moves) {
			Move mv = smv.mv();
			PV_search(mv);

			if (score >= beta) {
				if (st.squareOcc[mv.to()] == EMPTY_ID) {
					Move first_killer = killer_moves[0][depth];
					if (first_killer != mv) {
						killer_moves[0][depth] = mv;
						killer_moves[1][depth] = first_killer;
					}
				}
				set_zentry(beta, HASH_BETA, mv);
				return beta;
			}
			process_score(score, mv);
		}

		if (!zobrist_hit || zentry.depth() <= depth) set_zentry(alpha, node_type, best_move_local);

		return alpha;
	}

	I16 quiescent(I16 alpha, I16 beta, I8 depth, StateMix& stx) {
		State& st = *std::visit(StateCast(), stx);

		if (st.move_iter == st.move_arr) {
			I16 orig_eval = st.get_eval();

			std::visit(UpdateMoves(), stx);
			if (st.move_iter == st.move_arr) {
				if (st.in_check) return INT16_MIN + 1 + (start_depth + depth);
				else return 0;
			}

			return orig_eval;
		}

		if (depth <= 0) { return st.get_eval(); }

		if (!st.in_check) {
			I16 stand_pat_score = st.get_eval();
			if (stand_pat_score >= beta) return beta;
			if (stand_pat_score > alpha) alpha = stand_pat_score;
		}
		
		I16 score;
		Move best_move_local;
		U8 node_type = HASH_ALPHA;
		I16 best_score = INT16_MIN + 1;

		HTableEntry& zentry_og = st.data_table->get_zobrist_entry(st.zobrist_hash);
		HTableEntry zentry = zentry_og;

		AlignedState aligned_st;
		auto quiescence_move = [&] (Move mv) {
			StateMix next_stx = std::visit(PartialMoveVisitor{ mv, score, &aligned_st }, stx);
			if (score >= -alpha) score = alpha;
			else {
				std::visit(UpdateCaptures(), next_stx);
				score = -quiescent(-beta, -alpha, depth - 1, next_stx);
			}
		};

		auto process_score = [&](I16 score, Move mv) {
			if (score > alpha) {
				alpha = score;
				node_type = HASH_EXACT;
				best_score = score;
				best_move_local = mv;
			}
			else if (score > best_score) {
				best_score = score;
				best_move_local = mv;
			}
		};

		auto set_zentry = [&](I16 score, U8 node_type, Move mv) {
			zentry.setHash(st.zobrist_hash);
			zentry.setTypeAndDepthAndQuis(node_type, depth);
			zentry.best_move = mv;
			zentry.score = score;
			zentry_og = zentry;
		};

		bool zobrist_hit = zentry.softEquals(st.zobrist_hash);
		if (zobrist_hit) {
			IF_DEBUG{ stats.zobrist_hits++; stats.total_qnodes++; }
			I16 stored_score = evalScoreQuiescenceTPT(zentry, alpha, beta, 0);
			if (stored_score != INT16_MAX) { return stored_score; }
		}

		std::vector<SortedMove> moves = sort_moves<false>(st, 0, zentry, zobrist_hit);
		for (auto& smv : moves) {
			IF_DEBUG stats.total_qnodes++;
			Move mv = smv.mv();
			quiescence_move(mv);

			if (score >= beta) {
				if (zentry.is_quiesecent() && zentry.depth() <= depth)
					set_zentry(beta, HASH_BETA, mv);
				return beta;
			}
			process_score(score, mv);
		}

		if ((zentry.is_quiesecent() || !zobrist_hit) && zentry.depth() < depth) 
			set_zentry(alpha, node_type, best_move_local);

		return alpha;
	}

	template <bool RegularSearch>
	std::vector<SortedMove> sort_moves(State& st, U8 depth, HTableEntry& zentry, bool htable_hit) {
		U8 move_num = (U8)(st.move_iter - st.move_arr);
		std::vector<SortedMove> all_moves;
		all_moves.reserve(move_num);

		for (U8 i = 0; i < move_num; i++) {
			Move mv = st.move_arr[i];

			// Hash table move
			if (htable_hit && zentry.best_move == mv) {
				all_moves.push_back(SortedMove{ mv, 10000 });
				continue;
			}

			// Captures
			U8 toID = st.squareOcc[mv.to()];
			if (toID != EMPTY_ID) {
				I16 score = piece_eval_mult[toID] - piece_value_linear[st.squareOcc[mv.from()]];
				all_moves.push_back(SortedMove{ mv, score });
				continue;
			}

			if constexpr (RegularSearch) {
				if (mv == killer_moves[0][depth]) all_moves.push_back(SortedMove{ mv, 890 });
				else if (mv == killer_moves[1][depth]) all_moves.push_back(SortedMove{ mv, 889 });
				else all_moves.push_back(SortedMove{ mv, history_moves[st.squareOcc[mv.from()]][mv.to()] });
			}
			else all_moves.push_back(SortedMove{ mv, history_moves[st.squareOcc[mv.from()]][mv.to()] });
		}
		std::sort(all_moves.begin(), all_moves.end(), std::greater<SortedMove>());
		return all_moves;
	}
};
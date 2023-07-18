#pragma once

#include <string>
#include <vector>
#include <array>
#include <string>
#include <fstream>

#include "ChessConstants.h"
#include "Search.h"
#include "Uci.h"

// ELO estimation derived from Lucas Chess program
constexpr double elo_X = 0.2065;
constexpr double elo_K = 154.51;

class STS_suite {
public:
	UCI uci;
	U16 total_points;
	SearchStats global_stats;

	std::vector<std::string> read_lines(std::string filename) {
		std::ifstream input(filename);
		std::vector<std::string> file_content;
		for (std::string line; getline(input, line);) file_content.push_back(line);
		return file_content;
	}

	U8 get_score(std::string bm_pred, std::string best_moves_str) {
		if (bm_pred.back() == ' ') bm_pred = bm_pred.substr(0, bm_pred.size() - 1);
		auto best_moves = splitString(best_moves_str, ';');
		for (auto& bm : best_moves) {
			auto split_bm = splitString(bm, '=');
			if (bm_pred == (split_bm[0])) return (U8)std::stoi(split_bm[1]);
		}
		return 0;
	}

	void print_stats(SearchStats stats) {
		U64 kN_speed_total = (U64)((stats.total_nodes + stats.total_qnodes) / (stats.time_taken / (double)1e6));
		std::cout << std::fixed << std::setprecision(2);
		std::cout << "Completed in " << stats.time_taken / 1e9 << " seconds\n"
				  << "Speed: " << kN_speed_total << " kN/s\n"
				  << "Hash table hits: " << stats.zobrist_hits << "\n"
				  << "Nodes visited: " << stats.total_nodes << "\n"
				  << "Quiesence Nodes visited: " << stats.total_qnodes << "\n"
				  << "Cut | Median: " << stats.median_cut_perc() << "% | Mean: " << stats.mean_cut_perc() << "%\n"
				  << "EBF | Median: " << stats.median_ebf() << " | Mean: " << stats.mean_ebf() << "\n"
				  << "EBF/MBF | Median: " << stats.median_ebf_to_mbf() << " | Mean: " << stats.mean_ebf_to_mbf() << "\n\n";
	}

	void process_ebf_to_mbf(std::string perft_data, SearchStats& search_stats, SearchStats& total_stats, U8 depth) {
		auto perft_nodes = splitString(perft_data, ';');
		U64 total_perft_nodes = 0ULL;
		for (auto& depth_str : perft_nodes) {
			auto depth_split = splitString(depth_str, '=');
			U8 pdepth = (U8)std::stoi(depth_split[0]);
			if (pdepth > depth) break;

			U64 nodes = std::stoll(depth_split[1]);
			double mbf = (total_perft_nodes + nodes) / (double)total_perft_nodes;
			total_perft_nodes += nodes;

			if (pdepth <= 2) continue;

			double search_ebf = search_stats.EBF_vec[pdepth - 3];
			total_stats.EBF_to_MBF_vec.push_back(search_ebf / sqrt(mbf));
		}
		total_stats.ratio_vec.push_back((double)search_stats.total_nodes / total_perft_nodes);
	}

	void test_entry(U8 sts_index, U8 depth, U8 pos_to_test) {
		auto uci_p = std::unique_ptr<UCI>(&uci);
		UCI& local_uci = *uci_p;
		uci_p.release();

		auto sts_tests = read_lines("STS/STS" + std::to_string(sts_index) + ".cpd");
		sts_tests = std::vector<std::string>(sts_tests.begin(), sts_tests.begin() + pos_to_test);

		U16 score = 0;
		SearchStats total_stats = SearchStats();

		auto start = std::chrono::high_resolution_clock::now();
		for (auto& sts_entry : sts_tests) {
			auto split_line = splitString(sts_entry, ',');

			local_uci.parse_message("position fen " + split_line[0]);
			auto best_move = local_uci.parse_message("go depth " + std::to_string(depth));
			score += get_score(best_move.substr(9, best_move.size() - 9), split_line[1]);

			SearchStats local_stats = std::visit(StatsVisitor(), local_uci.search);

			if (uci.debug) process_ebf_to_mbf(split_line[2], local_stats, total_stats, depth);
			total_stats.add(local_stats);
		}
		total_stats.time_taken = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - start).count();

		std::cout << "\nScore for STS test " << (U16)sts_index << " at depth " << (U16)depth << ": " << (U16)score << "\n";
		print_stats(total_stats);

		total_points += score;
		global_stats.add(total_stats);
	}

	void test_all(U8 depth) {
		total_points = 0;
		uci.set_debug(true);

		for (U8 i = 1; i <= 15; i++) test_entry(i, depth, 100);

		std::cout << "Global stats for full STS test at depth " << (U16)depth << ":\n";
		print_stats(global_stats);

		// Estimates ELO using lucas chess formula, no idea if it's accurate.
		std::cout << "Estimated ELO: " << int(total_points * elo_X + elo_K) << " | Total points: " << total_points << "\n";
	}
};

void UCI::process_STS(std::vector<std::string> split_msg) {
	STS_suite sts = STS_suite();
	U8 depth = (U8)std::stoi(split_msg[1]);
	if (split_msg.size() > 2) {
		U8 sts_test_index = (U8)std::stoi(split_msg[2]);
		sts.uci.set_debug(true);
		sts.test_entry(sts_test_index, depth, 100);
	}
	else sts.test_all(depth);
}
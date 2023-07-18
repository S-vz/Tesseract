#include <chrono>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

#include <chrono>
#include <bitset>
#include <thread>
#include <intrin.h>

#include "STS.h"
#include "State.h"
#include "Uci.h"
#include "Search.h"
#include "DataGenerator.h"

void perft_template(StateWhite& st, U8 depth, std::string fen, U64 true_moves) {
	U64 num_moves = 0;
	if (fen != "") st.loadFenString(fen);
	auto start = std::chrono::high_resolution_clock::now();
	st.perft_all_moves(depth, num_moves);
	auto finish = std::chrono::high_resolution_clock::now();
	auto time_spent = std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();

	if (num_moves != true_moves) {
		std::cout << "Incorrect perft result for depth: " << depth << "\n";
		std::cout << "Anwser vs Real: " << num_moves << " | " << true_moves << "\n";
		std::cout << std::fixed << "Speed: " << int(num_moves / (time_spent / 1e6)) << " kN/S | Total time: " << time_spent / 1e6 << " ms\n\n";
	}
	else {
		std::cout << "Number of moves at depth " << (U64)depth << ": " << num_moves << "\n";
		std::cout << std::fixed << "Speed: " << int(num_moves / (time_spent / 1e6)) << " kN/S | Total time: " << time_spent / 1e6 << " ms\n\n";
	}
}

void test_perft() {
	DataTable mtable = DataTable();
	StateWhite st = StateWhite(&mtable);
	auto all_start = std::chrono::high_resolution_clock::now();
	mtable.usePerftTable();

	std::cout << "Starting positon:\n";
	perft_template(st, 7, "", 3195901860);

	std::cout << "KiwiPete positon:\n";
	perft_template(st, 5, "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", 193690690);

	std::cout << "Pinned enpassent positon:\n";
	perft_template(st, 7, "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", 178633661);

	std::cout << "Position 5:\n";
	perft_template(st, 6, "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -", 706045033);

	std::cout << "Position 6:\n";
	perft_template(st, 5, "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ -", 89941194);

	auto all_finish = std::chrono::high_resolution_clock::now();

	auto time_spent = std::chrono::duration_cast<std::chrono::nanoseconds>(all_finish - all_start).count();
	std::cout << "Total time: " << time_spent / 1e9 << " seconds\n";
}

int main() {
	//test_perft();
	UCI u = UCI();
	u.start_loop();
}
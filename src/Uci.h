#pragma once

#include <string>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <map>

#include "State.h"
#include "Search.h"
#include "DataTable.h"

typedef std::variant<Search<Regular>, Search<Debug>> SearchVar;

struct StatsVisitor { SearchStats operator()(auto& search) { return search.stats; } };
struct RepetitionVisitor { std::map<U64, U8> operator()(auto& search) { return search.repetition_map; } };

struct RepetitionSetter {
	std::map<U64, U8> rep_map;
	RepetitionSetter(std::map<U64, U8> repetition_map) : rep_map(repetition_map) {}
	void operator()(auto& search) {	search.repetition_map = rep_map; }
};

struct MaxSearchTimeSetter {
	U64 max_time;
	MaxSearchTimeSetter(U64 max_time) : max_time(max_time) {}
	void operator()(auto& search) { search.time_pkg.max_thinking_time = max_time; }
};

struct DepthSearchVisitor {
	StateMix stx;
	U8 depth;
	DepthSearchVisitor(StateMix& passed_stx, U8 d) : stx(passed_stx), depth(d) {}
	Move operator()(auto& st) { return st.depth_search(stx, depth); }
};

struct InfiniteSearchVisitor {
	StateMix stx;
	InfiniteSearchVisitor(StateMix& passed_stx) : stx(passed_stx) {}
	Move operator()(auto& search) { return search.timed_search(stx, search.time_pkg.max_thinking_time); }
};

struct TimedSearchVisitor {
	StateMix stx;
	U64 time_left;
	TimedSearchVisitor(StateMix& passed_stx, U64 time_left) : stx(passed_stx), time_left(time_left) {}
	Move operator()(auto& search) { 
		search.time_pkg.reset(time_left);
		return search.timed_search(stx, search.time_pkg.get_allocated_time());
	}
};

struct NewCerr {
	std::stringstream new_buf;
	std::streambuf* old_buf;

	NewCerr() : old_buf(std::cerr.rdbuf(new_buf.rdbuf())) {}
	~NewCerr() { std::cerr.rdbuf(old_buf); }
};

class UCI {
public:
	std::string log_filename;
	bool debug;

	SearchVar search = Search<Regular>();
	DataTable& dtable = DataTable::getInstance();

	StateWhite stw = StateWhite(&dtable);
	StateBlack stb = StateBlack(&dtable);
	StateMix stx = StateMix(&stw);

	UCI() : debug(false) {
		this->log_filename = this->get_log_filename();
		start_error_logging();
	}

	void start_error_logging(bool threaded = false) {
		if (!threaded) {
			std::thread t(&UCI::start_error_logging, this, true);
			t.detach();
			return;
		}
		auto captured_cerr = NewCerr();
		U64 chars_read = 0;
		while (1) {
			auto full_buffer = captured_cerr.new_buf.str();
			if (chars_read != 0) full_buffer = full_buffer.substr(chars_read);
			U64 pos = full_buffer.find("\n");
			if (pos == -1) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			chars_read += pos+1;
			full_buffer = full_buffer.substr(0, pos);
			log_msg(full_buffer);
		}
	}

	std::string get_log_filename() {
		std::filesystem::create_directory("logs");
		std::string unix_time = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
		return "logs/EngineLog-" + unix_time + ".txt";
	}

	std::string get_timestamp() {
		auto now = std::chrono::system_clock::now();
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		int millisecondsComponent = milliseconds.count() % 1000;

		std::stringstream ss;
		struct tm timeinfo;
		localtime_s(&timeinfo, &time);
		ss << std::put_time(&timeinfo, "%H:%M:%S.") << std::setfill('0') << std::setw(3) << millisecondsComponent;
		return ss.str();
	}

	void log_msg(std::string msg) {
		if (msg == "") return;
		std::ofstream engine_log;
		engine_log.open(log_filename, std::ios::app);
		engine_log << get_timestamp() << " | " << msg << "\n";
		engine_log.close();
	}

	void uci_resp(std::string resp) {
		if (resp == "") return;
		log_msg(resp);
		std::cout << resp << "\n";
	}

	void set_debug(bool turn_on_debug) {
		std::map<U64, U8> rep_map = std::visit(RepetitionVisitor(), search);
		if (turn_on_debug) search = Search<Debug>();
		else search = Search<Regular>();
		std::visit(RepetitionSetter{ rep_map }, search);
		this->debug = turn_on_debug;
	}

	std::string parse_message(std::string msg) {
		log_msg(msg);
		auto split_msg = splitString(msg, ' ');
		std::string cmd = split_msg[0];
		str_lower(cmd);

		if (cmd == "uci") return uci_init();
		else if (cmd == "debug") process_debug(split_msg);
		else if (cmd == "isready") return "readyok";
		else if (cmd == "setoption") process_setoption(split_msg);
		else if (cmd == "reset") this->dtable.reset_TPT();
		else if (cmd == "go") return process_go(split_msg);
		else if (cmd == "position") process_position(split_msg);
		else if (cmd == "ucinewgame") { stw.construct_startpos(stw.data_table); stx = StateMix(&stw); }
		else if (cmd == "sts") process_STS(split_msg);
		else if (cmd == "print") std::visit(PrintBoard(), stx);
		else if (cmd == "quit") exit(0);
		else return "Unknown command: '" + cmd + "'.\n";

		return "";
	}

	void start_loop() {
		while (1) {
			std::string msg;
			std::getline(std::cin, msg);
			if (msg != "") {
				try { uci_resp(parse_message(msg)); }
				catch (...) { uci_resp("Failed to run command: '" + msg + "'"); }
			}
		}
	}

	std::string uci_init() {
		return
			"id name Tesseract\n"
			"id author S\n"
			"\n"
			"option name Hash type spin default 256 min 1 max 16384\n"
			//"option name Ponder type check default false\n"
			"option name MaxSearchTime type spin default 5 min 1 max 120\n"
			"uciok\n";
	}

	void process_setoption(std::vector<std::string> split_msg) {
		try {
			std::string option = split_msg[2];
			std::string value = split_msg[4];
			str_lower(option);
			if (option == "hash") this->dtable.set_hash_table_size((U64)std::stoi(value));
			else if (option == "maxsearchtime") std::visit(MaxSearchTimeSetter{ (U64)std::stoi(value) }, search);
			else uci_resp("Unknown option: '" + option + "'");
		} catch (...) { uci_resp("Failed to process setoption"); }
	}

	void process_debug(std::vector<std::string> split_msg) {
		if (split_msg.size() == 1) std::cout << "Missing parameter: [ on | off ]\n";
		set_debug(split_msg[1] == "on");
	}

	void process_moves(std::vector<std::string> move_vector, bool whiteTurn) {
		std::map<U64, U8> rep_map;
		AlignedState aligned_st;
		for (auto& mv_str : move_vector) {
			State& st = *std::visit(StateCast(), stx);
			Move mv = Move(mv_str, st.turn);
			if (st.is_irreversible_move(mv)) rep_map.clear();
			if (!mv.promotion()) mv.data |= st.pawn_implicit_promo_check(mv) << 12;

			stx = std::visit(MoveVisitor{ mv, &aligned_st }, stx);
			rep_map[std::visit(ZobristHash(), stx)]++; // Add occurence
		}
		std::visit(RepetitionSetter{ rep_map }, search);

		auto final_fen_str = std::visit(FenString(), stx);
		U8 turn = ((U8)!whiteTurn + move_vector.size()) & 1;
		if (turn) {
			stb.loadFenString(final_fen_str);
			stx = StateMix(&stb);
		}
		else {
			stw.loadFenString(final_fen_str);
			stx = StateMix(&stw);
		}
	}

	void process_STS(std::vector<std::string> split_msg);

	void process_position(std::vector<std::string> split_msg) {
		int i = 3;
		std::string pos = split_msg[1];
		bool whiteTurn = true;
		if (pos == "startpos") {
			stw.construct_startpos(stw.data_table);
			stx = StateMix(&stw);
		}
		else {
			std::string fen_str = "";
			for (i = 2; i < split_msg.size() && split_msg[i] != "moves"; i++) fen_str.append(split_msg[i] + " ");
			i++;
			whiteTurn = fen_str.find('w') != std::string::npos;
			if (whiteTurn) {
				stw.loadFenString(fen_str);
				stx = StateMix(&stw);
			}
			else {
				stb.loadFenString(fen_str);
				stx = StateMix(&stb);
			}
		}

		if (split_msg.size() < i + 1) return;

		auto move_vector = std::vector(split_msg.begin() + i, split_msg.end());
		process_moves(move_vector, whiteTurn);
	}

	std::string process_go(std::vector<std::string> all_cmds) {
		std::string go_cmd = all_cmds[1];
		if (go_cmd == "depth") {
			U8 depth = (U8)std::stoi(all_cmds[2]);
			Move mv = std::visit(DepthSearchVisitor{ stx, depth }, search);
			return "bestmove " + mv.toString();
		}
		else if (go_cmd == "infinite") {
			Move mv = std::visit(InfiniteSearchVisitor{ stx }, search);
			return "bestmove " + mv.toString();
		}
		else if (go_cmd == "wtime" || go_cmd == "btime") {
			Move mv = std::visit(TimedSearchVisitor{ stx, this->parse_time_left(all_cmds) }, search);
			return "bestmove " + mv.toString();
		}
		else if (go_cmd == "movetime") {
			Move mv = std::visit(TimedSearchVisitor{ stx, (U64)std::stoi(all_cmds[2])*20 }, search);
			return "bestmove " + mv.toString();
		}
		else if (go_cmd == "perft") {
			this->dtable.usePerftTable();
			U8 depth = (U8)std::stoi(all_cmds[2]);
			U64 total_moves = std::visit(PerftVisitor{ depth }, stx);
			this->dtable.useSearchTable();
			return "Nodes searched: " + std::to_string(total_moves);
		}
		return "bestmove a1a1";
	}

	U64 parse_time_left(std::vector<std::string> all_cmds) {
		U64 time_left = 0;
		U64 time_inc = 0;
		U8 our_color = std::visit(TurnCheck{}, stx);
		char color_char = (our_color == 0) ? 'w' : 'b';
		for (U8 i = 1; i < all_cmds.size(); i++) {
			if (all_cmds[i][0] != color_char) continue;
			else if (all_cmds[i] == std::string(1, color_char) + "time") time_left = std::stoi(all_cmds[i + 1]);
			else if (all_cmds[i] == std::string(1, color_char) + "inc") time_inc = std::stoi(all_cmds[i + 1]);
		}
		return time_left + time_inc;
	}

};
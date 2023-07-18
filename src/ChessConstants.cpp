#include <bitset>
#include <vector>
#include <algorithm>

#include "ChessConstants.h"


// Prints according to LERF standard
std::string bbStr(U64 bb) {
	std::string bbStr = std::bitset<64>(bb).to_string();
	std::replace(bbStr.begin(), bbStr.end(), '0', '.');
	for (int i = 8; i > 0; i--) {
		std::reverse(bbStr.begin() + (i - 1) * 8, bbStr.begin() + i * 8);
		bbStr.insert(i * 8, "\n");
	}
	bbStr.append("\n");
	return bbStr;
}

std::vector<std::string> splitString(std::string str, char splitter) {
	std::vector<std::string> result;
	std::string current = "";
	for (int i = 0; i < str.size(); i++) {
		if (str[i] == splitter) {
			if (current != "") {
				result.push_back(current);
				current = "";
			}
			continue;
		}
		current += str[i];
	}
	if (current.size() != 0)
		result.push_back(current);
	return result;
}

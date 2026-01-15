#pragma once

#include <vector>
#include <string>

std::vector<int> parseUserClauseInput(std::string input);
void runBenchMark();
void ipamirClauseCollector(int lit, void *solver);
#pragma once

#include <vector>
#include <string>

std::vector<int> parseUserClauseInput(std::string input);
void runBenchMark();
void ipamirAddClause(void *solver, std::vector<int> clause, uint32_t &literalCount, bool hardClause, int penalty = 0);
void ipamirAddSoftClause(void *solver, std::vector<int> clause, uint32_t &literalCount, int penalty);
void ipamirClauseCollector(int lit, void *solver);
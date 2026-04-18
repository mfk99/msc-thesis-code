#pragma once

#include <vector>
#include <string>
#include <stdint.h>

struct Result
{
    long long solveTimeMs;
    bool satisfied;
    uint64_t penalty;
};

std::vector<int> parseUserClauseInput(std::string input);
void runBenchMark();
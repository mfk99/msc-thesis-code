#pragma once

#include <vector>
#include <tuple>
#include <string>
#include <stdint.h>

struct Result
{
    long long solveTimeMs;
    bool satisfied;
    uint64_t penalty;
};

struct IterationResult
{
    std::vector<Result> results;
    std::vector<std::tuple<std::string, uint16_t>> optimization;
};

const char *getSolverSignature();
std::vector<int> parseUserClauseInput(std::string input);
void runBenchMark();
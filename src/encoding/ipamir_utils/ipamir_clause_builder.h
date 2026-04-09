#pragma once
#include <vector>

enum ClauseType
{
    TIME,
    ROOM,
    DISTRIBUTION,
    STUDENT,
    CLAUSE_NONE
};

void ipamirAddSoftClause(void *solver, std::vector<int> clause, uint32_t &literalCount, int penalty);
void ipamirAddClause(void *solver, std::vector<int> clause, uint32_t &literalCount, bool hardClause, int penalty = 0, ClauseType clauseType = ClauseType::CLAUSE_NONE);
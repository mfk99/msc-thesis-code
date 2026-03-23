#include <iostream>
#include <vector>
#include "../input_parser/input_parser.h"
#include "../logging/logging.h"
#include "../../../../ipamir.h"

void ipamirAddSoftClause(void *solver, std::vector<int> clause, uint32_t &literalCount, int penalty)
{
    if (clause.size() == 1)
    {
        verboseLog("Adding a unit soft literal " +
                   std::to_string(clause[0]) + " with weight " +
                   std::to_string(penalty));
        ipamir_add_soft_lit(solver, clause[0], penalty);
        return;
    }

    int softLit = literalCount;
    literalCount += 1;
    ipamir_add_soft_lit(solver, softLit, penalty);
    if (verbose)
        std::cout << "[VERBOSE] Adding soft literal " << softLit << " with weight " << penalty << " and clause ";

    for (int literal : clause)
    {
        ipamir_add_hard(solver, literal);
        if (verbose)
            std::cout << literal << ", ";
    }
    ipamir_add_hard(solver, softLit);
    ipamir_add_hard(solver, 0);
    if (verbose)
        std::cout << softLit << ", 0\n";
}

void ipamirAddClause(void *solver, std::vector<int> clause, uint32_t &literalCount, bool hardClause, int penalty = 0)
{
    if (hardClause)
    {
        for (int literal : clause)
        {
            ipamir_add_hard(solver, literal);
        }
        ipamir_add_hard(solver, 0);
    }
    else
    {
        ipamirAddSoftClause(solver, clause, literalCount, penalty);
    }
}
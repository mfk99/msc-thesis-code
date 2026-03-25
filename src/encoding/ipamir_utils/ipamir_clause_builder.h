#include <vector>

void ipamirAddSoftClause(void *solver, std::vector<int> clause, uint32_t &literalCount, int penalty);
void ipamirAddClause(void *solver, std::vector<int> clause, uint32_t &literalCount, bool hardClause, int penalty = 0);
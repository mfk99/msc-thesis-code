#include "../../utils/logging/logging.h"
#include "../../../libs/ipamir/ipamir.h"

struct ClauseCollectorData
{
    void *solver;
    bool required;
    int penalty;
};

void ipamirGteClauseCollector(int lit, void *collectorData)
{
    ClauseCollectorData *data = static_cast<ClauseCollectorData *>(collectorData);
    if (data->required)
    {
        ipamir_add_hard(data->solver, lit);
        ipamir_add_hard(data->solver, 0);
        verboseLog("Adding hard clause [" +
                   std::to_string(lit) + ",0]");
    }
    else
    {
        ipamir_add_soft_lit(data->solver, abs(lit), data->penalty);
        verboseLog("Adding soft lit: " +
                   std::to_string(abs(lit)) + " with penalty: " +
                   std::to_string(data->penalty));
    }
}

void ipamirClauseCollector(int lit, void *solver)
{
    ipamir_add_hard(solver, lit);
}
struct ClauseCollectorData
{
    void *solver;
    bool required;
    int penalty;
};
void ipamirGteClauseCollector(int lit, void *collectorData);
void ipamirClauseCollector(int lit, void *solver);
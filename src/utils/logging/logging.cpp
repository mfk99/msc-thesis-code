#include "../input_parser/input_parser.h"
#include <iostream>
#include <vector>
#include <string>

using namespace std;

void verboseLog(string log)
{
    if (opts.verbose)
        cout << "[VERBOSE] " << log << "\n";
}

void logTimingLiterals(vector<vector<int>> *t)
{
    if (!opts.verbose)
        return;
    verboseLog("timing literals: ");
    for (vector<int> classPeriods : (*t))
    {
        for (int periodLiteral : classPeriods)
        {
            cout << "[" << periodLiteral << "]";
        }
        cout << "\n";
    }
}

void logRoomLiterals(vector<vector<int>> *r)
{
    if (!opts.verbose)
        return;
    verboseLog("room literals: ");
    for (vector<int> classRooms : (*r))
    {
        for (int roomLiteral : classRooms)
        {
            cout << "[" << roomLiteral << "]";
        }
        cout << "\n";
    }
}
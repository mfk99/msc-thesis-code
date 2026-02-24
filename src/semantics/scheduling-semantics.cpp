#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <numeric>
#include "../logging/logging.h"
#include "../input-parser/input-parser.h"
#include "../config/config.h"
#include "../am1/am1-encoder.h"
#include "../../../../ipamir.h"
#include "../../../../rustsat/capi/rustsat.h"

using namespace std;
using namespace RustSAT;

void ipamirAddSoftClause(void *solver, vector<int> clause, uint32_t &literalCount, int penalty);

vector<int> parseUserClauseInput(string input)
{
    vector<int> clauseLiterals;
    string delimiter = " ";
    size_t pos = 0;
    string token;
    while ((pos = input.find(delimiter)) != string::npos)
    {
        token = input.substr(0, pos);
        clauseLiterals.push_back(stoi(token));
        input.erase(0, pos + delimiter.length());
    }
    clauseLiterals.push_back(stoi(input));
    return clauseLiterals;
}

void ipamirAddClause(void *solver, vector<int> clause, uint32_t &literalCount, bool hardClause, int penalty = 0)
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

void ipamirAddSoftClause(void *solver, vector<int> clause, uint32_t &literalCount, int penalty)
{
    if (clause.size() == 1)
    {
        verboseLog("Adding a unit soft literal " + to_string(clause[0]) + " with weight " + to_string(penalty));
        ipamir_add_soft_lit(solver, clause[0], penalty);
        return;
    }

    int softLit = literalCount;
    literalCount += 1;
    ipamir_add_soft_lit(solver, softLit, penalty);
    if (verbose)
        cout << "[VERBOSE] Adding soft literal " << softLit << " with weight " << penalty << " and clause ";

    for (int literal : clause)
    {
        ipamir_add_hard(solver, literal);
        if (verbose)
            cout << literal << ", ";
    }
    ipamir_add_hard(solver, softLit);
    ipamir_add_hard(solver, 0);
    if (verbose)
        cout << softLit << ", 0\n";
}

void ipamirClauseCollector(int lit, void *solver)
{
    ipamir_add_hard(solver, lit);
}

void runBenchMark()
{

    vector<int> generationVariables = getConfigVariables();

    int weeks = generationVariables[0];
    int days = generationVariables[1];
    int hours = generationVariables[2];
    int rooms = generationVariables[3];
    int courses = generationVariables[4];
    int courseHours = generationVariables[5];

    int periods = weeks * days * hours;
    int classes = courses * courseHours;

    void *solver = ipamir_init();

    map<string, Class> classMap = getClasses();
    map<string, int> classIndexMap;

    vector<Class> classVec;
    int index = 0;
    for (auto &pair : classMap)
    {
        classVec.push_back(pair.second);
        classIndexMap[pair.second.id] = index;
        index++;
    }

    // Represents class period assignments
    vector<vector<int>> t;
    // Represents class room assignments
    vector<vector<int>> r;

    uint32_t literalCounter = 1;

    for (auto &[classId, classObj] : classMap)
    {
        // Initilaize t
        vector<int> timingLiterals;
        if (verbose)
            cout << "[VERBOSE] Created timing assignment literals " << literalCounter;
        for ([[maybe_unused]] Timing timing : classObj.timings)
        {
            timingLiterals.push_back(literalCounter);
            literalCounter++;
        }
        if (verbose)
            cout << " - " << literalCounter - 1 << " for course " << classId << "\n";
        t.push_back(timingLiterals);

        // Initilaize r
        vector<int> roomLiterals;
        if (verbose)
            cout << "[VERBOSE] Created room assignment literals " << literalCounter;
        for ([[maybe_unused]] Room room : classObj.rooms)
        {
            roomLiterals.push_back(literalCounter);
            literalCounter++;
        }
        if (verbose)
            cout << " - " << literalCounter - 1 << " for course " << classId << "\n";
        r.push_back(roomLiterals);
    }

    verboseLog("Creating clauses for " +
               to_string(classes) + " classes with " +
               to_string(periods) + " time periods and " +
               to_string(rooms) + " rooms.");

    // Map of distributions, used for generating distribution encodings
    map<string, vector<DistributionVariant>> distributionsMap = getDistributions();

    logTimingLiterals(t);
    logRoomLiterals(r);

    // Encode at-least-one period constraints
    for (long long i = 0; i < classes; i++)
    {
        for (int lit : t[i])
        {
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
    }

    // Encode at-least-one room constraints
    for (long long i = 0; i < classes; i++)
    {
        if (r[i].size() == 0)
            continue;
        for (int lit : r[i])
        {
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
    }

    string am1EncoderType = "pairwise";

    // Encode am1 period constraints
    for (long long i = 0; i < classes; i++)
    {
        if (verbose)
            cout << "[VERBOSE] Adding am1 period constraints for literals [";
        AM1Encoder am1Encoder = AM1Encoder(am1EncoderType);
        for (int lit : t[i])
        {
            if (verbose)
                cout << lit << ",";
            am1Encoder.am1encoder_add(lit);
        }
        am1Encoder.am1encoder_encode(&literalCounter, ipamirClauseCollector, solver);
        if (verbose)
            cout << "0] \n";
        am1Encoder.am1encoder_drop();
    }

    // Encode am1 room constraints
    for (long long i = 0; i < classes; i++)
    {
        if (r[i].size() == 0)
        {
            if (verbose)
                cout << "[VERBOSE] No rooms defined for class n." << i << ", skipping am1 encoding\n";
            continue;
        }

        if (verbose)
            cout << "[VERBOSE] Adding am1 room constraints for literals [";
        AM1Encoder am1Encoder = AM1Encoder(am1EncoderType);
        for (int lit : r[i])
        {
            if (verbose)
                cout << lit << ",";
            am1Encoder.am1encoder_add(lit);
        }
        am1Encoder.am1encoder_encode(&literalCounter, ipamirClauseCollector, solver);
        if (verbose)
            cout << "0] \n";
        am1Encoder.~AM1Encoder();
    }

    // Encode penalties
    for (long long classIndex = 0; classIndex < classes; classIndex++)
    {
        Class classObj = classVec[classIndex];
        for (long long timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
        {
            Timing timing = classObj.timings[timingIndex];
            if (timing.penalty != 0)
            {
                int timingLit = t[classIndex][timingIndex];
                ipamir_add_soft_lit(solver, timingLit, timing.penalty);
                verboseLog("Adding penalty: " + to_string(timing.penalty) + " to timing literal: " + to_string(timingLit));
            }
        }

        for (long long roomIndex = 0; roomIndex < classObj.rooms.size(); roomIndex++)
        {
            Room room = classObj.rooms[roomIndex];
            if (room.penalty != 0)
            {
                int roomLit = r[classIndex][roomIndex];
                ipamir_add_soft_lit(solver, roomLit, room.penalty);
                verboseLog("Adding penalty: " + to_string(room.penalty) + " to room literal: " + to_string(roomLit));
            }
        }
    }

    // Encode RoomConflict constraints
    for (long long class1Index = 0; class1Index < classes; class1Index++)
    {
        Class class1 = classVec[class1Index];
        for (long long class2Index = class1Index + 1; class2Index < classes; class2Index++)
        {
            Class class2 = classVec[class2Index];
            for (int class1RoomIndex = 0; class1RoomIndex < class1.rooms.size(); class1RoomIndex++)
            {
                Room room1 = class1.rooms[class1RoomIndex];
                for (int class2RoomIndex = 0; class2RoomIndex < class2.rooms.size(); class2RoomIndex++)
                {
                    Room room2 = class2.rooms[class2RoomIndex];
                    if (room1.id != room2.id)
                        continue;

                    for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                    {
                        Timing timing1 = class1.timings[class1TimingIndex];
                        for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                        {
                            Timing timing2 = class2.timings[class2TimingIndex];
                            string timing1Weeks = timing1.weeks;
                            string timing2Weeks = timing2.weeks;
                            string timing1Days = timing1.days;
                            string timing2Days = timing2.days;
                            bool overlapConstraintEncoded = false;
                            for (int weekIndex = 0; weekIndex < weeks && !overlapConstraintEncoded; weekIndex++)
                            {
                                if (timing1Weeks[weekIndex] == '0' || timing2Weeks[weekIndex] == '0')
                                    continue;

                                for (int dayIndex = 0; dayIndex < days && !overlapConstraintEncoded; dayIndex++)
                                {
                                    if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                        continue;

                                    int timing1EndSlot = timing1.start + timing1.length;
                                    int timing2EndSlot = timing2.start + timing2.length;
                                    bool overLap = (timing1.start < timing2EndSlot && timing1.start >= timing2.start) ||
                                                   (timing2.start < timing1EndSlot && timing2.start >= timing1.start);
                                    if (overLap)
                                    {
                                        ipamir_add_hard(solver, -t[class1Index][class1TimingIndex]);
                                        ipamir_add_hard(solver, -t[class2Index][class2TimingIndex]);
                                        ipamir_add_hard(solver, -r[class1Index][class1RoomIndex]);
                                        ipamir_add_hard(solver, -r[class2Index][class2RoomIndex]);
                                        ipamir_add_hard(solver, 0);
                                        verboseLog("Adding RoomConflict constraint: " +
                                                   to_string(-t[class1Index][class1TimingIndex]) + ", " +
                                                   to_string(-t[class2Index][class2TimingIndex]) + ", " +
                                                   to_string(-r[class1Index][class1RoomIndex]) + ", " +
                                                   to_string(-r[class2Index][class2RoomIndex]) + ", 0");
                                        overlapConstraintEncoded = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode RoomUnavailability constraints
    for (int classIndex = 0; classIndex < classVec.size(); classIndex++)
    {
        Class classObj = classVec[classIndex];
        for (int roomIndex = 0; roomIndex < classObj.rooms.size(); roomIndex++)
        {
            Room room = classObj.rooms[roomIndex];
            for (RoomUnavailability unavailability : room.unavailability)
            {
                string roomUnavailabilityDays = unavailability.days;
                string roomUnavailabilityWeeks = unavailability.weeks;
                for (int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
                {

                    bool constraintEncoded = false;
                    Timing classTiming = classObj.timings[timingIndex];
                    string classTimingDays = classTiming.days;
                    string classTimingWeeks = classTiming.weeks;
                    for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                    {
                        if (roomUnavailabilityWeeks[weekIndex] == '0' || classTimingWeeks[weekIndex] == '0')
                            continue;
                        for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                        {
                            if (roomUnavailabilityDays[dayIndex] == '0' || classTimingDays[dayIndex] == '0')
                                continue;
                            int roomUnavailabilityStart = unavailability.start;
                            int roomUnavailabilityLength = unavailability.length;
                            int roomUnavailabilityEnd = roomUnavailabilityStart + roomUnavailabilityLength;
                            int classTimingStart = classTiming.start;
                            int classTimingLength = classTiming.length;
                            int classTimingEnd = classTimingStart + classTimingLength;
                            bool overLap = (roomUnavailabilityStart < roomUnavailabilityEnd && roomUnavailabilityStart >= classTimingStart) ||
                                           (classTimingStart < roomUnavailabilityEnd && classTimingStart >= roomUnavailabilityStart);

                            if (overLap)
                            {
                                int timingLit = t[classIndex][timingIndex];
                                int roomLit = r[classIndex][roomIndex];
                                verboseLog("Adding RoomUnavailability constraint: -" + to_string(timingLit) + ", -" + to_string(roomLit) + ", 0");
                                ipamir_add_hard(solver, -timingLit);
                                ipamir_add_hard(solver, -roomLit);
                                ipamir_add_hard(solver, 0);
                                constraintEncoded = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameStart constraints
    for (auto &dist : distributionsMap["SameStart"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                for (size_t class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    int class1TimingStart = class1.timings[class1TimingIndex].start;
                    for (size_t class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        int class2TimingStart = class2.timings[class2TimingIndex].start;
                        if (class1TimingStart == class2TimingStart)
                            continue;

                        int periodLit1 = t[class1Index][class1TimingIndex];
                        int periodLit2 = t[class2Index][class2TimingIndex];
                        verboseLog("Adding SameStart constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                        ipamirAddClause(solver,
                                        {-periodLit1, -periodLit2},
                                        literalCounter,
                                        required,
                                        penalty);
                    }
                }
            }
        }
    }

    // Encode SameTime constraints
    for (auto &dist : distributionsMap["SameTime"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (size_t class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    int class1TimingStart = class1.timings[class1TimingIndex].start;
                    int class1TimingEnd = class1TimingStart + class1.timings[class1TimingIndex].length;
                    for (size_t class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        int class2TimingStart = class2.timings[class2TimingIndex].start;
                        int class2TimingEnd = class2TimingStart + class2.timings[class2TimingIndex].length;
                        if (!((class1TimingStart <= class2TimingStart && class2TimingEnd <= class1TimingEnd) ||
                              (class2TimingStart <= class1TimingStart && class1TimingEnd <= class2TimingEnd)))
                        {
                            int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                            int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                            verboseLog("Adding SameTime constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                            ipamirAddClause(solver,
                                            {-periodLit1, -periodLit2},
                                            literalCounter,
                                            required,
                                            penalty);
                        }
                    }
                }
            }
        }
    }

    // Encode DifferentTime constraints
    for (auto &dist : distributionsMap["DifferentTime"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (size_t class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    int class1TimingStart = class1.timings[class1TimingIndex].start;
                    int class1TimingEnd = class1TimingStart + class1.timings[class1TimingIndex].length;
                    for (size_t class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        int class2TimingStart = class2.timings[class2TimingIndex].start;
                        int class2TimingEnd = class2TimingStart + class2.timings[class2TimingIndex].length;
                        if (!(class1TimingEnd <= class2TimingStart || class2TimingEnd <= class1TimingStart))
                        {
                            int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                            int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                            verboseLog("Adding DifferentTime constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                            ipamirAddClause(solver,
                                            {-periodLit1, -periodLit2},
                                            literalCounter,
                                            required,
                                            penalty);
                        }
                    }
                }
            }
        }
    }

    // Encode SameDays constraints
    for (auto &dist : distributionsMap["SameDays"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];

                for (size_t class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    string class1TimingDays = class1.timings[class1TimingIndex].days;
                    for (size_t class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        string class2TimingDays = class2.timings[class2TimingIndex].days;
                        bool is1SubSet = true;
                        bool is2SubSet = true;
                        for (int day = 0; day < days; day++)
                        {
                            if (class1TimingDays[day] == '1' && class2TimingDays[day] == '0')
                                is1SubSet = false;
                            if (class1TimingDays[day] == '0' && class2TimingDays[day] == '1')
                                is2SubSet = false;
                        }
                        if (!is1SubSet && !is2SubSet)
                        {
                            int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                            int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                            verboseLog("Adding SameDays constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                            ipamirAddClause(solver,
                                            {-periodLit1, -periodLit2},
                                            literalCounter,
                                            required,
                                            penalty);
                        }
                    }
                }
            }
        }
    }

    // Encode DifferentDays constraints
    for (auto &dist : distributionsMap["DifferentDays"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];

                for (size_t class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    string class1TimingDays = class1.timings[class1TimingIndex].days;
                    for (size_t class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        string class2TimingDays = class2.timings[class2TimingIndex].days;
                        bool is1SubSet = true;
                        bool is2SubSet = true;
                        for (int day = 0; day < days; day++)
                        {
                            if (class1TimingDays[day] == '1' && class2TimingDays[day] == '1')
                            {
                                int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                verboseLog("Adding SameDays constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0 \n");
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                required,
                                                penalty);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameWeeks constraints
    for (auto &dist : distributionsMap["SameWeeks"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];

                for (size_t class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    string class1TimingWeeks = class1.timings[class1TimingIndex].weeks;
                    for (size_t class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        string class2TimingWeeks = class1.timings[class2TimingIndex].weeks;
                        bool is1SubSet = true;
                        bool is2SubSet = true;
                        for (int week = 0; week < weeks; week++)
                        {
                            if (class1TimingWeeks[week] == '1' && class2TimingWeeks[week] == '0')
                                is1SubSet = false;
                            if (class1TimingWeeks[week] == '0' && class2TimingWeeks[week] == '1')
                                is2SubSet = false;
                        }
                        if (!is1SubSet && !is2SubSet)
                        {
                            int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                            int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                            verboseLog("Adding SameWeeks constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                            ipamirAddClause(solver,
                                            {-periodLit1, -periodLit2},
                                            literalCounter,
                                            required,
                                            penalty);
                        }
                    }
                }
            }
        }
    }

    // Encode DifferentWeeks constraints
    for (auto &dist : distributionsMap["DifferentWeeks"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];

                for (size_t class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    string class1TimingWeeks = class1.timings[class1TimingIndex].weeks;
                    for (size_t class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        string class2TimingWeeks = class1.timings[class2TimingIndex].weeks;

                        for (int week = 0; week < weeks; week++)
                        {
                            if (class1TimingWeeks[week] == '1' && class2TimingWeeks[week] == '1')
                            {
                                int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                verboseLog("Adding DifferentWeeks constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                required,
                                                penalty);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameRoom constraints
    for (auto &dist : distributionsMap["SameRoom"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];

                for (size_t class1RoomIndex = 0; class1RoomIndex < class1.rooms.size(); class1RoomIndex++)
                {
                    string class1RoomId = class1.rooms[class1RoomIndex].id;
                    for (size_t class2RoomIndex = 0; class2RoomIndex < class1.rooms.size(); class2RoomIndex++)
                    {
                        string class2RoomId = class2.rooms[class2RoomIndex].id;
                        if (class1RoomId == class2RoomId)
                            continue;

                        int roomLit1 = r[class1LiteralIndex][class1RoomIndex];
                        int roomLit2 = r[class2LiteralIndex][class2RoomIndex];
                        verboseLog("Adding SameRoom constraint: -" + to_string(roomLit1) + ", -" + to_string(roomLit2) + ", 0");
                        ipamirAddClause(solver,
                                        {-roomLit1, -roomLit2},
                                        literalCounter,
                                        required,
                                        penalty);
                    }
                }
            }
        }
    }

    // Encode DifferentRoom constraints
    for (auto &dist : distributionsMap["DifferentRoom"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];

                for (size_t class1RoomIndex = 0; class1RoomIndex < class1.rooms.size(); class1RoomIndex++)
                {
                    string class1RoomId = class1.rooms[class1RoomIndex].id;
                    for (size_t class2RoomIndex = 0; class2RoomIndex < class1.rooms.size(); class2RoomIndex++)
                    {
                        string class2RoomId = class2.rooms[class2RoomIndex].id;
                        if (class1RoomId != class2RoomId)
                            continue;
                        int roomLit1 = r[class1LiteralIndex][class1RoomIndex];
                        int roomLit2 = r[class2LiteralIndex][class2RoomIndex];
                        verboseLog("Adding DifferentRoom constraint: -" + to_string(roomLit1) + ", -" + to_string(roomLit2) + ", 0");
                        ipamirAddClause(solver,
                                        {-roomLit1, -roomLit2},
                                        literalCounter,
                                        required,
                                        penalty);
                    }
                }
            }
        }
    }

    // Encode OverLap constraints
    // TODO: refactor this ASAP, a single for-loop shouldn't be 100 lines long
    for (auto &dist : distributionsMap["OverLap"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];
                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        bool constraintEncoded = false;
                        for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                        {
                            string timing1Weeks = timing1.weeks;
                            string timing2Weeks = timing2.weeks;
                            if (timing1Weeks[weekIndex] == '0' && timing2Weeks[weekIndex] == '0')
                                continue;
                            if (timing1Weeks[weekIndex] != timing2Weeks[weekIndex])
                            {
                                int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                verboseLog("Adding OverLap constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                required,
                                                penalty);
                                constraintEncoded = true;
                            }
                            for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                            {
                                string timing1Days = timing1.days;
                                string timing2Days = timing2.days;
                                if (timing1Days[dayIndex] == '0' && timing2Days[dayIndex] == '0')
                                    continue;
                                if (timing1Days[dayIndex] != timing2Days[dayIndex])
                                {
                                    int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                    int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                    verboseLog("Adding OverLap constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    required,
                                                    penalty);
                                    constraintEncoded = true;
                                }
                                int timing1EndSlot = timing1.start + timing1.length;
                                int timing2EndSlot = timing2.start + timing2.length;
                                bool overLap = (timing1.start < timing2EndSlot) &&
                                               (timing2.start < timing1EndSlot);
                                if (!overLap)
                                {
                                    int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                    int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                    verboseLog("Adding OverLap constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    required,
                                                    penalty);
                                    constraintEncoded = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode NotOverLap constraints
    for (auto &dist : distributionsMap["NotOverLap"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        bool constraintEncoded = false;
                        for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                        {
                            string timing1Weeks = timing1.weeks;
                            string timing2Weeks = timing2.weeks;
                            if (timing1Weeks[weekIndex] == '0' || timing2Weeks[weekIndex] == '0')
                                continue;
                            for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                            {
                                string timing1Days = timing1.days;
                                string timing2Days = timing2.days;
                                if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                    continue;
                                int timing1EndSlot = timing1.start + timing1.length;
                                int timing2EndSlot = timing2.start + timing2.length;
                                bool overLap = (timing1.start < timing2EndSlot) &&
                                               (timing2.start < timing1EndSlot);
                                if (overLap)
                                {
                                    int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                    int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                    verboseLog("Adding NotOverLap constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    required,
                                                    penalty);
                                    constraintEncoded = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameAttendees constraints
    for (auto &dist : distributionsMap["SameAttendees"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        for (int class1RoomIndex = 0; class1RoomIndex < class1.rooms.size(); class1RoomIndex++)
                        {
                            Room class1Room = class1.rooms[class1RoomIndex];
                            for (int class2RoomIndex = 0; class2RoomIndex < class2.rooms.size(); class2RoomIndex++)
                            {
                                Room class2Room = class2.rooms[class2RoomIndex];
                                bool constraintEncoded = false;
                                for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                                {
                                    string timing1Weeks = timing1.weeks;
                                    string timing2Weeks = timing2.weeks;
                                    if (timing1Weeks[weekIndex] == '0' || timing2Weeks[weekIndex] == '0')
                                        continue;
                                    for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                                    {
                                        string timing1Days = timing1.days;
                                        string timing2Days = timing2.days;
                                        if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                            continue;
                                        int class1TimingStart = timing1.start;
                                        int class2TimingStart = timing2.start;
                                        int class1TimingEnd = class1TimingStart + timing1.length;
                                        int class2TimingEnd = class2TimingStart + timing2.length;

                                        int travelTime1To2 = 0;
                                        if (class1Room.travelTimes.count(class2Room.id))
                                            travelTime1To2 = class1Room.travelTimes[class2Room.id];
                                        int travelTime2To1 = 0;
                                        if (class2Room.travelTimes.count(class1Room.id))
                                            travelTime2To1 = class2Room.travelTimes[class1Room.id];
                                        if (!((class1TimingEnd + travelTime1To2 <= class2TimingStart) ||
                                              (class2TimingEnd + travelTime2To1 <= class1TimingStart)))
                                        {
                                            int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                            int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                            verboseLog("Adding SameAttendees constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                            ipamirAddClause(solver,
                                                            {-periodLit1, -periodLit2},
                                                            literalCounter,
                                                            required,
                                                            penalty);
                                            constraintEncoded = true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode WorkDay constraints
    int S = hours;
    for (auto &dist : distributionsMap["WorkDay"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);

        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        for (int dayIndex = 0; dayIndex < days; dayIndex++)
                        {
                            string timing1Days = timing1.days;
                            string timing2Days = timing2.days;
                            if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                continue;

                            int timing1End = timing1.start + timing1.length;
                            int timing2End = timing2.start + timing2.length;
                            if (S < max(timing1End, timing2End) - min(timing1.start, timing2.start))
                            {
                                int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                verboseLog("Adding WorkDay constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                required,
                                                penalty);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode Precedence constraints
    // TODO: refactor this, too long and convoluted
    for (auto &dist : distributionsMap["Precedence"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        std::visit([&distributionClasses, &required, &penalty](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty; },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                        int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                        bool constraintEncoded = false;
                        for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                        {

                            if ((timing1.weeks[weekIndex] == '0' && timing2.weeks[weekIndex] == '0') ||
                                (timing1.weeks[weekIndex] == '1' && timing2.weeks[weekIndex] == '0'))
                                continue;
                            if (timing2.weeks[weekIndex] == '1')
                            {
                                for (int timing1WeekIndex = weekIndex + 1; timing1WeekIndex < weeks && !constraintEncoded; timing1WeekIndex++)
                                {
                                    if (timing1.weeks[timing1WeekIndex] == '1')
                                    {
                                        verboseLog("Adding Precedence constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0 ");
                                        ipamirAddClause(solver,
                                                        {-periodLit1, -periodLit2},
                                                        literalCounter,
                                                        required,
                                                        penalty);
                                        constraintEncoded = true;
                                    }
                                }
                            }
                            if (timing1.weeks[weekIndex] == '1' && timing2.weeks[weekIndex] == '1')
                            {
                                for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                                {

                                    if ((timing1.days[dayIndex] == '0' && timing2.days[dayIndex] == '0') ||
                                        (timing1.days[dayIndex] == '1' && timing2.days[dayIndex] == '0'))
                                        continue;
                                    if (timing2.days[dayIndex] == '1')
                                    {
                                        for (int timing1DayIndex = dayIndex + 1; timing1DayIndex < days && !constraintEncoded; timing1DayIndex++)
                                        {
                                            if (timing1.days[timing1DayIndex] == '1')
                                            {
                                                verboseLog("Adding Precedence constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                                ipamirAddClause(solver,
                                                                {-periodLit1, -periodLit2},
                                                                literalCounter,
                                                                required,
                                                                penalty);
                                                constraintEncoded = true;
                                            }
                                        }
                                    }
                                    if (timing1.days[dayIndex] == '1' && timing2.days[dayIndex] == '1')
                                    {
                                        int timing1End = timing1.start + timing1.length;
                                        if (timing1End <= timing2.start)
                                            continue;
                                        verboseLog("Adding Precedence constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                        ipamirAddClause(solver,
                                                        {-periodLit1, -periodLit2},
                                                        literalCounter,
                                                        required,
                                                        penalty);
                                        constraintEncoded = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode MinGap Constraints

    for (auto &dist : distributionsMap["MinGap"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        int G = 1;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MinGapDistribution>)
                    {
                        G = d.G;
                    } },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        bool constraintEncoded = false;
                        for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                        {
                            string timing1Weeks = timing1.weeks;
                            string timing2Weeks = timing2.weeks;
                            if (timing1Weeks[weekIndex] == '0' || timing2Weeks[weekIndex] == '0')
                                continue;
                            for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                            {
                                string timing1Days = timing1.days;
                                string timing2Days = timing2.days;
                                if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                    continue;

                                int timing1End = timing1.start + timing1.length;
                                int timing2End = timing2.start + timing2.length;
                                if (!((timing1End + G <= timing2.start) ||
                                      (timing2End + G <= timing1.start)))
                                {
                                    int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                    int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                    verboseLog("Adding MinGap(" + to_string(G) + ") constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    required,
                                                    penalty);
                                    constraintEncoded = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode MaxDays Constraints

    for (auto &dist : distributionsMap["MaxDays"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        int D = 4;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MaxDaysDistribution>)
                    {
                        D = d.D;
                    } },
                   dist);
        // Initialize dayUsed literals
        vector<int> dayUsed;
        for (int dayIndex = 0; dayIndex < days; dayIndex++)
        {
            dayUsed.push_back(literalCounter);
            verboseLog("Adding literal " + to_string(literalCounter) + " as a day literal for MaxDays constraint");
            literalCounter++;
        }

        // Add at-most-k constraint
        int initalLiteralCounter = literalCounter;

        Totalizer *tot = tot_new();
        for (int dayLiteral : dayUsed)
            tot_add(tot, dayLiteral);
        tot_reserve(tot, &literalCounter);
        tot_encode_ub(tot, D, D, &literalCounter, ipamirClauseCollector, solver);
        tot_drop(tot);
        // Last literal represents whether at-most-k holds
        ipamir_add_soft_lit(solver, literalCounter, penalty);
        verboseLog("Added literals :" + to_string(initalLiteralCounter) + " - " + to_string(literalCounter) + " for at-most-k encoding MaxDays");
        literalCounter++;

        for (size_t classIndex = 0; classIndex < distributionClasses.size(); classIndex++)
        {
            string classId = distributionClasses[classIndex];
            Class classObj = classMap[classId];
            int classLiteralIndex = 0;
            for (Class iteratorClassObj : classVec)
            {
                if (classObj.id == iteratorClassObj.id)
                    break;
                classLiteralIndex++;
            }
            for (int classTimingIndex = 0; classTimingIndex < classObj.timings.size(); classTimingIndex++)
            {
                Timing timing = classObj.timings[classTimingIndex];
                string timingDays = timing.days;
                for (int dayIndex = 0; dayIndex < days; dayIndex++)
                {

                    if (timingDays[dayIndex] == '0')
                        continue;
                    int periodLit = t[classLiteralIndex][classTimingIndex];
                    int dayUsedLit = dayUsed[dayIndex];
                    // (periodLit -> dayUsedLit)
                    ipamirAddClause(solver, {-periodLit, dayUsedLit}, literalCounter, true, 0);
                    verboseLog("Adding MaxDays(" + to_string(D) + ") constraint: -" + to_string(periodLit) + ", " + to_string(dayUsedLit) + ", 0 ");
                }
            }
        }
    }

    // Encode MaxDayLoad Constraints
    // TODO: Make this work with variable amount of courses

    for (auto &dist : distributionsMap["MaxDayLoad"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        int maxDayLoadS = 0;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MaxDayLoadDistribution>)
                    {
                        maxDayLoadS = d.S;
                    } },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];
                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        bool constraintEncoded = false;
                        for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                        {
                            string timing1Weeks = timing1.weeks;
                            string timing2Weeks = timing2.weeks;
                            if (timing1Weeks[weekIndex] == '0' || timing2Weeks[weekIndex] == '0')
                                continue;

                            for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                            {
                                string timing1Days = timing1.days;
                                string timing2Days = timing2.days;
                                if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                    continue;

                                int dayLoad = timing1.length + timing2.length;
                                if (maxDayLoadS < dayLoad)
                                {
                                    int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                    int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                    verboseLog("Adding MaxDayLoad(" + to_string(maxDayLoadS) + ") constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    required,
                                                    penalty);
                                    constraintEncoded = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode MaxBreaks Constraints
    // TODO: Make this work with variable amount of courses
    for (auto &dist : distributionsMap["MaxBreaks"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        // Max n.o. blocks
        int maxBreaksR = 0;
        // Length of blocks
        int maxBreaksS = 0;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MaxBreaksDistribution>)
                    {
                        maxBreaksR = d.R;
                        maxBreaksS = d.S;
                    } },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];
                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        bool constraintEncoded = false;
                        for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                        {
                            string timing1Weeks = timing1.weeks;
                            string timing2Weeks = timing2.weeks;
                            if (timing1Weeks[weekIndex] == '0' || timing2Weeks[weekIndex] == '0')
                                continue;

                            for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                            {
                                string timing1Days = timing1.days;
                                string timing2Days = timing2.days;
                                if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                    continue;

                                int breakAmount = 0;
                                int timing1End = timing1.start + timing1.length;
                                int timing2End = timing2.start + timing2.length;
                                int breakLength = min(abs(timing1End - timing2.start), abs(timing2End - timing1.start));

                                if (maxBreaksS < breakLength)
                                {
                                    int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                    int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                    verboseLog("Adding MaxBreaks(" + to_string(maxBreaksR) + "," + to_string(maxBreaksS) + ") constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    required,
                                                    penalty);
                                    constraintEncoded = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode MaxBlocks Constraints
    for (auto &dist : distributionsMap["MaxBlocks"])
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        // Max n.o. blocks
        int maxBlocksM = 0;
        // Required break length
        int maxBlocksS = 0;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MaxBlockDistribution>)
                    {
                        maxBlocksM = d.M;
                        maxBlocksS = d.S;
                    } },
                   dist);
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = classIndexMap[class2.id];
                for (int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];
                    for (int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        bool constraintEncoded = false;
                        for (int weekIndex = 0; weekIndex < weeks && !constraintEncoded; weekIndex++)
                        {
                            string timing1Weeks = timing1.weeks;
                            string timing2Weeks = timing2.weeks;
                            if (timing1Weeks[weekIndex] == '0' || timing2Weeks[weekIndex] == '0')
                                continue;

                            for (int dayIndex = 0; dayIndex < days && !constraintEncoded; dayIndex++)
                            {
                                string timing1Days = timing1.days;
                                string timing2Days = timing2.days;
                                if (timing1Days[dayIndex] == '0' || timing2Days[dayIndex] == '0')
                                    continue;

                                int timing1End = timing1.start + timing1.length;
                                int timing2End = timing2.start + timing2.length;
                                int breakLength = min(abs(timing1End - timing2.start), abs(timing2End - timing1.start));
                                int blockLength = max(abs(timing1End - timing2.start), abs(timing2End - timing1.start));
                                if (maxBlocksM < blockLength && breakLength < maxBlocksS)
                                {
                                    int periodLit1 = t[class1LiteralIndex][class1TimingIndex];
                                    int periodLit2 = t[class2LiteralIndex][class2TimingIndex];
                                    verboseLog("Adding MaxBlocks(" + to_string(maxBlocksM) + "," + to_string(maxBlocksS) + ") constraint: -" + to_string(periodLit1) + ", -" + to_string(periodLit2) + ", 0");
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    required,
                                                    penalty);
                                    constraintEncoded = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Print answer and ask user for input
    while (true)
    {
        // TODO: Write encoding to file based on generate-variable
        std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
        int code = ipamir_solve(solver);
        std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
        long long timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        cout << "Solving took:" << timeDiff << "[µs], " << timeDiff / 1000000.0 << "[s] \n";
        cout << "Code returned by ipamir: " << code << "\n";
        if (code == 30)
        {
            cout << "Assignment:\n";
            for (int classIndex = 0; classIndex < classes; classIndex++)
            {
                Class classObj = classVec[classIndex];
                string classId = classObj.id;
                string timingWeeks,
                    timingDays;
                int timingStart,
                    timingLength;

                for (int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
                {
                    int timingLit = t[classIndex][timingIndex];
                    if (0 < ipamir_val_lit(solver, timingLit))
                    {
                        Timing timing = classObj.timings[timingIndex];
                        timingWeeks = timing.weeks;
                        timingDays = timing.days;
                        timingStart = timing.start;
                        timingLength = timing.length;
                        verboseLog("timingLit: " + to_string(timingLit) + "=1");
                    }
                }

                string roomId;
                if (classObj.rooms.size() == 0)
                {
                    roomId = "[/]";
                    verboseLog("roomLit: -");
                }
                for (int roomIndex = 0; roomIndex < classObj.rooms.size(); roomIndex++)
                {
                    int roomLit = r[classIndex][roomIndex];
                    if (0 < ipamir_val_lit(solver, roomLit))
                    {
                        Room room = classObj.rooms[roomIndex];
                        roomId = room.id;
                        verboseLog("roomLit:" + to_string(roomLit) + "=1");
                    }
                }

                cout << "Class " << classId
                     << " is assigned to weeks: " << timingWeeks
                     << ", days: " << timingDays
                     << ", starting at slot: " << timingStart
                     << ", for " << timingLength
                     << " slot(s) to room " << roomId << "\n";
            }
            uint64_t penalty = ipamir_val_obj(solver);
            cout << "Penalty incurred by the solution: " << penalty << "\n";
        }

        cout << "Insert a new clause or give an empty input to exit\n";
        string input;
        std::getline(cin, input);
        if (input.size() == 0)
            break;
        vector<int> clauseLiterals = parseUserClauseInput(input);
        for (int lit : clauseLiterals)
        {
            verboseLog("Adding literal " + to_string(lit));
            ipamir_add_hard(solver, lit);
        }
    }
    cout << "Releasing ipamir...\n";
    ipamir_release(solver);
    cout << "Exiting...\n";
}
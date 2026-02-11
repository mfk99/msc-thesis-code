#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <numeric>
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
        if (verbose)
            cout << "[VERBOSE] Adding a unit soft literal " << clause[0] << " with weight " << penalty << "\n";
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
    if (verbose)
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

    vector<Class> classVec;
    for (auto &pair : classMap)
    {
        classVec.push_back(pair.second);
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

    if (verbose)
        cout << "[VERBOSE] Creating clauses for "
             << classes << " classes with "
             << periods << " time periods and "
             << rooms << " rooms.\n";

    // Map of distributions, used for generating distribution encodings
    map<string, vector<Distribution>> distributionsMap = getDistributions();

    if (verbose)
    {
        cout << "[VERBOSE] period literals: \n";
        for (long long i = 0; i < classes; i++)
        {
            for (int periodLiteral : t[i])
            {
                cout << "[" << periodLiteral << "]";
            }
            cout << "\n";
        }

        cout << "[VERBOSE] room literals: \n";
        for (long long i = 0; i < classes; i++)
        {
            if (r[i].size() == 0)
                cout << "[]";
            for (int periodLiteral : r[i])
            {
                cout << "[" << periodLiteral << "]";
            }
            cout << "\n";
        }
    }

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
                                        if (verbose)
                                        {
                                            cout << "[VERBOSE] Adding RoomConflict constraint: "
                                                 << -t[class1Index][class1TimingIndex] << ", "
                                                 << -t[class2Index][class2TimingIndex] << ", "
                                                 << -r[class1Index][class1RoomIndex] << ", "
                                                 << -r[class2Index][class2RoomIndex] << ", 0 \n";
                                        }
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
                                if (verbose)
                                    cout
                                        << "[VERBOSE] Adding RoomUnavailability constraint: -" << timingLit << ", -" << roomLit << ", 0 \n";
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
    for (Distribution sameStartDistribution : distributionsMap["SameStart"])
    {
        vector<string> distributionClasses = sameStartDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            // TODO This and all instances of it should REALLY be refactored ASAP
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }
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
                        if (verbose)
                            cout << "[VERBOSE] Adding SameStart constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                        ipamirAddClause(solver,
                                        {-periodLit1, -periodLit2},
                                        literalCounter,
                                        sameStartDistribution.required,
                                        sameStartDistribution.penalty);
                    }
                }
            }
        }
    }

    // Encode SameDays constraints
    for (Distribution SameDaysDistribution : distributionsMap["SameDays"])
    {
        vector<string> distributionClasses = SameDaysDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }

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
                            if (verbose)
                                cout << "[VERBOSE] Adding SameDays constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                            ipamirAddClause(solver,
                                            {-periodLit1, -periodLit2},
                                            literalCounter,
                                            SameDaysDistribution.required,
                                            SameDaysDistribution.penalty);
                        }
                    }
                }
            }
        }
    }

    // Encode DifferentDays constraints
    for (Distribution DifferentDaysDistribution : distributionsMap["DifferentDays"])
    {
        vector<string> distributionClasses = DifferentDaysDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }

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
                                if (verbose)
                                    cout << "[VERBOSE] Adding SameDays constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                DifferentDaysDistribution.required,
                                                DifferentDaysDistribution.penalty);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameWeeks constraints
    for (Distribution sameWeeksDistribution : distributionsMap["SameWeeks"])
    {
        vector<string> distributionClasses = sameWeeksDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }

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
                            if (verbose)
                                cout << "[VERBOSE] Adding SameWeeks constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                            ipamirAddClause(solver,
                                            {-periodLit1, -periodLit2},
                                            literalCounter,
                                            sameWeeksDistribution.required,
                                            sameWeeksDistribution.penalty);
                        }
                    }
                }
            }
        }
    }

    // Encode DifferentWeeks constraints
    for (Distribution differentWeeksDistribution : distributionsMap["DifferentWeeks"])
    {
        vector<string> distributionClasses = differentWeeksDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }

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
                                if (verbose)
                                    cout << "[VERBOSE] Adding DifferentWeeks constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                differentWeeksDistribution.required,
                                                differentWeeksDistribution.penalty);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameRoom constraints
    for (Distribution sameRoomDistribution : distributionsMap["SameRoom"])
    {
        vector<string> distributionClasses = sameRoomDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }

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
                        if (verbose)
                            cout << "[VERBOSE] Adding SameRoom constraint: -" << roomLit1 << ", -" << roomLit2 << ", 0 \n";
                        ipamirAddClause(solver,
                                        {-roomLit1, -roomLit2},
                                        literalCounter,
                                        sameRoomDistribution.required,
                                        sameRoomDistribution.penalty);
                    }
                }
            }
        }
    }

    // Encode DifferentRoom constraints
    for (Distribution differentRoomDistribution : distributionsMap["DifferentRoom"])
    {
        vector<string> distributionClasses = differentRoomDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }

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
                        if (verbose)
                            cout << "[VERBOSE] Adding DifferentRoom constraint: -" << roomLit1 << ", -" << roomLit2 << ", 0 \n";
                        ipamirAddClause(solver,
                                        {-roomLit1, -roomLit2},
                                        literalCounter,
                                        differentRoomDistribution.required,
                                        differentRoomDistribution.penalty);
                    }
                }
            }
        }
    }

    // Encode OverLap constraints
    // TODO: refactor this ASAP, a single for-loop shouldn't be 100 lines long
    for (Distribution overLapDistribution : distributionsMap["OverLap"])
    {
        vector<string> distributionClasses = overLapDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }
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
                                if (verbose)
                                    cout
                                        << "[VERBOSE] Adding OverLap constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                overLapDistribution.required,
                                                overLapDistribution.penalty);
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
                                    if (verbose)
                                        cout
                                            << "[VERBOSE] Adding OverLap constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    overLapDistribution.required,
                                                    overLapDistribution.penalty);
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
                                    if (verbose)
                                        cout
                                            << "[VERBOSE] Adding OverLap constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    overLapDistribution.required,
                                                    overLapDistribution.penalty);
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
    for (Distribution notOverLapDistribution : distributionsMap["NotOverLap"])
    {
        vector<string> distributionClasses = notOverLapDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }
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
                                    if (verbose)
                                        cout
                                            << "[VERBOSE] Adding NotOverLap constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                    ipamirAddClause(solver,
                                                    {-periodLit1, -periodLit2},
                                                    literalCounter,
                                                    notOverLapDistribution.required,
                                                    notOverLapDistribution.penalty);
                                    constraintEncoded = true;
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
    for (Distribution workDayDistribution : distributionsMap["WorkDay"])
    {
        vector<string> distributionClasses = workDayDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }
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
                                if (verbose)
                                    cout
                                        << "[VERBOSE] Adding WorkDay constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                ipamirAddClause(solver,
                                                {-periodLit1, -periodLit2},
                                                literalCounter,
                                                workDayDistribution.required,
                                                workDayDistribution.penalty);
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
    for (Distribution precedenceDistribution : distributionsMap["Precedence"])
    {
        vector<string> distributionClasses = precedenceDistribution.classes;
        for (size_t class1Index = 0; class1Index < distributionClasses.size(); class1Index++)
        {
            string class1Id = distributionClasses[class1Index];
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = 0;
            for (Class classObj : classVec)
            {
                if (classObj.id == class1.id)
                    break;
                class1LiteralIndex++;
            }

            for (size_t class2Index = class1Index + 1; class2Index < distributionClasses.size(); class2Index++)
            {
                string class2Id = distributionClasses[class2Index];
                Class class2 = classMap[class2Id];
                int class2LiteralIndex = 0;
                for (Class classObj : classVec)
                {
                    if (classObj.id == class2.id)
                        break;
                    class2LiteralIndex++;
                }
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
                                        if (verbose)
                                            cout
                                                << "[VERBOSE] Adding Precedence constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                        ipamirAddClause(solver,
                                                        {-periodLit1, -periodLit2},
                                                        literalCounter,
                                                        precedenceDistribution.required,
                                                        precedenceDistribution.penalty);
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
                                                if (verbose)
                                                    cout
                                                        << "[VERBOSE] Adding Precedence constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                                ipamirAddClause(solver,
                                                                {-periodLit1, -periodLit2},
                                                                literalCounter,
                                                                precedenceDistribution.required,
                                                                precedenceDistribution.penalty);
                                                constraintEncoded = true;
                                            }
                                        }
                                    }
                                    if (timing1.days[dayIndex] == '1' && timing2.days[dayIndex] == '1')
                                    {
                                        int timing1End = timing1.start + timing1.length;
                                        if (timing1End <= timing2.start)
                                            continue;
                                        if (verbose)
                                            cout
                                                << "[VERBOSE] Adding Precedence constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                        ipamirAddClause(solver,
                                                        {-periodLit1, -periodLit2},
                                                        literalCounter,
                                                        precedenceDistribution.required,
                                                        precedenceDistribution.penalty);
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
                        if (verbose)
                            cout << "[VERBOSE] timingLit:" << timingLit << "=1 \n";
                    }
                }

                string roomId;
                if (classObj.rooms.size() == 0)
                {
                    roomId = "[/]";
                    if (verbose)
                        cout << "[VERBOSE] roomLit: - \n";
                }
                for (int roomIndex = 0; roomIndex < classObj.rooms.size(); roomIndex++)
                {
                    int roomLit = r[classIndex][roomIndex];
                    if (0 < ipamir_val_lit(solver, roomLit))
                    {
                        Room room = classObj.rooms[roomIndex];
                        roomId = room.id;
                        if (verbose)
                            cout << "[VERBOSE] roomLit:" << roomLit << "=1 \n";
                    }
                }

                cout << "Class " << classId
                     << " is assigned to weeks: " << timingWeeks
                     << ", days: " << timingDays
                     << ", starting at slot: " << timingStart
                     << ", for " << timingLength
                     << " slot(s) to room " << roomId << "\n";
            }
        }

        cout << "Insert a new clause or give an empty input to exit\n";
        string input;
        std::getline(cin, input);
        if (input.size() == 0)
            break;
        vector<int> clauseLiterals = parseUserClauseInput(input);
        for (int lit : clauseLiterals)
        {
            if (verbose)
            {
                cout << "[VERBOSE] Adding literal " << lit << "\n";
            }
            ipamir_add_hard(solver, lit);
        }
    }
    cout << "Releasing ipamir...\n";
    ipamir_release(solver);
    cout << "Exiting...\n";
}
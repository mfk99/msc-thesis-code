#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <numeric>
#include <functional>
#include <algorithm>
#include <set>
#include "../logging/logging.h"
#include "../input_parser/input_parser.h"
#include "../config/config.h"
#include "../am1/am1_encoder.h"
#include "../../../../ipamir.h"
#include "../../../../rustsat/capi/rustsat.h"
#include "../ipamir_utils/ipamir_clause_builder.h"
#include "../ipamir_utils/ipamir_clause_collector.h"

using namespace std;
using namespace RustSAT;

void encodeAtLeast1Timing(void *solver, int classes, vector<vector<int>> t)
{
    for (long long i = 0; i < classes; i++)
    {
        for (int lit : t[i])
        {
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
    }
};

void encodeAtLeast1Room(void *solver, int classes, vector<vector<int>> r)
{
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
};

void encodeAM1Timing(void *solver,
                     uint32_t literalCounter,
                     int classes,
                     vector<vector<int>> t,
                     string am1EncoderType = "pairwise")
{
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
}

void encodeAM1Room(void *solver,
                   uint32_t literalCounter,
                   int classes,
                   vector<vector<int>> r,
                   string am1EncoderType = "pairwise")
{
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
}

void encodeAssignmentPenalties(void *solver,
                               int classes,
                               vector<vector<int>> t,
                               vector<vector<int>> r,
                               vector<Class> classVec)
{
    for (long long classIndex = 0; classIndex < classes; classIndex++)
    {
        // cout << "classIndex: " << classIndex << "\n";
        Class classObj = classVec[classIndex];
        for (long unsigned int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
        {
            Timing timing = classObj.timings[timingIndex];
            // cout << "timingIndex: " << timingIndex << "\n";
            if (timing.penalty != 0)
            {
                int timingLit = t[classIndex][timingIndex];
                // cout << "timingLit: " << timingLit << "\n";
                ipamir_add_soft_lit(solver, timingLit, timing.penalty);
                verboseLog("Adding penalty: " + to_string(timing.penalty) + " to timing literal: " + to_string(timingLit));
            }
        }

        for (long unsigned int roomIndex = 0; roomIndex < classObj.rooms.size(); roomIndex++)
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
}

void encodeRoomConflictConstraints(void *solver,
                                   int classes,
                                   int weeks,
                                   int days,
                                   vector<vector<int>> t,
                                   vector<vector<int>> r,
                                   vector<Class> classVec)
{
    for (long long class1Index = 0; class1Index < classes; class1Index++)
    {
        Class class1 = classVec[class1Index];
        for (long long class2Index = class1Index + 1; class2Index < classes; class2Index++)
        {
            Class class2 = classVec[class2Index];
            for (long unsigned int class1RoomIndex = 0; class1RoomIndex < class1.rooms.size(); class1RoomIndex++)
            {
                Room room1 = class1.rooms[class1RoomIndex];
                for (long unsigned int class2RoomIndex = 0; class2RoomIndex < class2.rooms.size(); class2RoomIndex++)
                {
                    Room room2 = class2.rooms[class2RoomIndex];
                    if (room1.id != room2.id)
                        continue;

                    for (long unsigned int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                    {
                        Timing timing1 = class1.timings[class1TimingIndex];
                        for (long unsigned int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
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
}

void encodeRoomUnavailabilityConstraints(
    void *solver,
    int weeks,
    int days,
    vector<vector<int>> t,
    vector<vector<int>> r,
    vector<Class> classVec)
{
    for (long unsigned int classIndex = 0; classIndex < classVec.size(); classIndex++)
    {
        Class classObj = classVec[classIndex];
        for (long unsigned int roomIndex = 0; roomIndex < classObj.rooms.size(); roomIndex++)
        {
            Room room = classObj.rooms[roomIndex];
            for (RoomUnavailability unavailability : room.unavailability)
            {
                string roomUnavailabilityDays = unavailability.days;
                string roomUnavailabilityWeeks = unavailability.weeks;
                for (long unsigned int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
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
}

void encodeSameStartConstraint(void *solver,
                               uint32_t literalCounter,
                               vector<vector<int>> t,
                               map<string, Class> classMap,
                               vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeSameTimeConstraint(void *solver,
                              uint32_t literalCounter,
                              vector<vector<int>> t,
                              map<string, Class> classMap,
                              map<string, int> classIndexMap,
                              vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeDifferentTimeConstraint(void *solver,
                                   uint32_t literalCounter,
                                   vector<vector<int>> t,
                                   map<string, Class> classMap,
                                   map<string, int> classIndexMap,
                                   vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeSameDaysConstraint(void *solver,
                              uint32_t literalCounter,
                              int days,
                              vector<vector<int>> t,
                              map<string, Class> classMap,
                              map<string, int> classIndexMap,
                              vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeDifferentDaysConstraint(void *solver,
                                   uint32_t literalCounter,
                                   int days,
                                   vector<vector<int>> t,
                                   map<string, Class> classMap,
                                   map<string, int> classIndexMap,
                                   vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeSameWeeksConstraints(void *solver,
                                uint32_t literalCounter,
                                int weeks,
                                vector<vector<int>> t,
                                map<string, Class> classMap,
                                map<string, int> classIndexMap,
                                vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeDifferentWeeksConstraints(void *solver,
                                     uint32_t literalCounter,
                                     int weeks,
                                     vector<vector<int>> t,
                                     map<string, Class> classMap,
                                     map<string, int> classIndexMap,
                                     vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeSameRoomConstraints(void *solver,
                               uint32_t literalCounter,
                               vector<vector<int>> r,
                               map<string, Class> classMap,
                               map<string, int> classIndexMap,
                               vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

void encodeDifferentRoomConstraints(void *solver,
                                    uint32_t literalCounter,
                                    vector<vector<int>> r,
                                    map<string, Class> classMap,
                                    map<string, int> classIndexMap,
                                    vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
}

// TODO: refactor this ASAP, a single for-loop shouldn't be 100 lines long
void encodeOverLapConstraints(void *solver,
                              uint32_t literalCounter,
                              int weeks,
                              int days,
                              vector<vector<int>> t,
                              vector<vector<int>> r,
                              map<string, Class> classMap,
                              map<string, int> classIndexMap,
                              vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
                for (long unsigned int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];
                    for (long unsigned int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
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
}

void encodeNotOverLapConstraints(void *solver,
                                 uint32_t literalCounter,
                                 int weeks,
                                 int days,
                                 vector<vector<int>> t,
                                 vector<vector<int>> r,
                                 map<string, Class> classMap,
                                 map<string, int> classIndexMap,
                                 vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
                for (long unsigned int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (long unsigned int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
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
}

void encodeSameAttendeesConstraints(void *solver,
                                    uint32_t literalCounter,
                                    int weeks,
                                    int days,
                                    vector<vector<int>> t,
                                    map<string, Class> classMap,
                                    map<string, int> classIndexMap,
                                    vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
                for (long unsigned int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (long unsigned int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
                    {
                        Timing timing2 = class2.timings[class2TimingIndex];
                        for (long unsigned int class1RoomIndex = 0; class1RoomIndex < class1.rooms.size(); class1RoomIndex++)
                        {
                            Room class1Room = class1.rooms[class1RoomIndex];
                            for (long unsigned int class2RoomIndex = 0; class2RoomIndex < class2.rooms.size(); class2RoomIndex++)
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
}

void encodeWorkDayConstraints(void *solver,
                              uint32_t literalCounter,
                              int weeks,
                              int days,
                              vector<vector<int>> t,
                              map<string, Class> classMap,
                              map<string, int> classIndexMap,
                              vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        int S = -1;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, WorkDayDistribution>)
                    {
                        S = d.S;
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
                for (long unsigned int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (long unsigned int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
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
}

// TODO: refactor this, too long and convoluted
void encodePrecedenceConstraints(void *solver,
                                 uint32_t literalCounter,
                                 int weeks,
                                 int days,
                                 vector<vector<int>> t,
                                 map<string, Class> classMap,
                                 map<string, int> classIndexMap,
                                 vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
                for (long unsigned int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (long unsigned int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
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
}

void encodeMinGapConstraints(void *solver,
                             uint32_t literalCounter,
                             int weeks,
                             int days,
                             vector<vector<int>> t,
                             map<string, Class> classMap,
                             map<string, int> classIndexMap,
                             vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
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
                for (long unsigned int class1TimingIndex = 0; class1TimingIndex < class1.timings.size(); class1TimingIndex++)
                {
                    Timing timing1 = class1.timings[class1TimingIndex];

                    for (long unsigned int class2TimingIndex = 0; class2TimingIndex < class2.timings.size(); class2TimingIndex++)
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
}

void encodeMaxDaysConstraints(void *solver,
                              uint32_t literalCounter,
                              int weeks,
                              int days,
                              vector<vector<int>> t,
                              vector<Class> classVec,
                              map<string, Class> classMap,
                              map<string, int> classIndexMap,
                              vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        int D = 0;
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
        int initialLiteralCounter = literalCounter;

        Totalizer *tot = tot_new();
        for (int dayLiteral : dayUsed)
            tot_add(tot, dayLiteral);
        tot_reserve(tot, &literalCounter);
        tot_encode_ub(tot, D, D, &literalCounter, ipamirClauseCollector, solver);
        tot_drop(tot);
        // Last literal represents whether at-most-k holds
        if (required)
        {
            ipamir_add_hard(solver, -literalCounter);
            ipamir_add_hard(solver, 0);
        }
        else
        {
            ipamir_add_soft_lit(solver, literalCounter, penalty);
        }
        verboseLog("Added literals :" + to_string(initialLiteralCounter) + " - " + to_string(literalCounter) + " for at-most-k encoding MaxDays");
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
            for (long unsigned int classTimingIndex = 0; classTimingIndex < classObj.timings.size(); classTimingIndex++)
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
}

// TODO: Correct penalty calculation
void encodeMaxDayLoadConstraints(void *solver,
                                 uint32_t literalCounter,
                                 int weeks,
                                 int days,
                                 vector<vector<int>> t,
                                 map<string, Class> classMap,
                                 map<string, int> classIndexMap,
                                 vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        int S = 0;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MaxDayLoadDistribution>)
                    {
                        S = d.S;
                    } },
                   dist);

        for (int weekIndex = 0; weekIndex < weeks; weekIndex++)
        {
            for (int dayIndex = 0; dayIndex < days; dayIndex++)
            {
                vector<int> literalVec;
                vector<Timing> timingVec;
                for (string classId : distributionClasses)
                {
                    Class classObj = classMap[classId];
                    for (long unsigned int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
                    {
                        Timing timing = classObj.timings[timingIndex];
                        if (timing.weeks[weekIndex] == '0' || timing.days[dayIndex] == '0')
                            continue;
                        int timingLiteral = t[classIndexMap[classId]][timingIndex];
                        literalVec.push_back(timingLiteral);
                        timingVec.push_back(timing);
                    }
                }

                if (literalVec.size() < 2)
                    continue;

                GeneralizedTotalizer *genTot = gte_new();
                for (long unsigned int index = 0; index < literalVec.size(); index++)
                {
                    gte_add(genTot, literalVec[index], timingVec[index].length);
                }
                uint32_t n_vars_used = literalCounter - 1;
                gte_reserve(genTot, &n_vars_used);
                gte_encode_ub(genTot, S, S, &n_vars_used, ipamirClauseCollector, solver);
                ClauseCollectorData data;
                data.penalty = penalty;
                data.required = required;
                data.solver = solver;
                gte_enforce_ub(genTot, S, ipamirGteClauseCollector, &data);
                gte_drop(genTot);

                verboseLog("Adding MaxDayLoad(" + to_string(S) + ") constraint");
                literalCounter = n_vars_used;
            }
        }
    }
}

// TODO: Check penalty calculation matches ITC
void encodeMaxBreaksConstraints(void *solver,
                                uint32_t literalCounter,
                                int weeks,
                                int days,
                                int timeSlots,
                                vector<vector<int>> t,
                                map<string, Class> classMap,
                                map<string, int> classIndexMap,
                                vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        // Max n.o. blocks
        int R = 0;
        // Length of blocks
        int S = 0;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MaxBreaksDistribution>)
                    {
                        R = d.R;
                        S = d.S;
                    } },
                   dist);

        for (int weekIndex = 0; weekIndex < weeks; weekIndex++)
        {
            for (int dayIndex = 0; dayIndex < days; dayIndex++)
            {
                vector<int> literalVec;
                vector<Timing> timingVec;
                for (string classId : distributionClasses)
                {
                    Class classObj = classMap[classId];
                    for (long unsigned int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
                    {
                        Timing timing = classObj.timings[timingIndex];
                        if (timing.weeks[weekIndex] == '0' || timing.days[dayIndex] == '0')
                            continue;
                        int timingLiteral = t[classIndexMap[classId]][timingIndex];
                        literalVec.push_back(timingLiteral);
                        timingVec.push_back(timing);
                    }
                }

                if (literalVec.size() < 2)
                    continue;

                // Create auxiliary literals for timeslots
                vector<int> slotLiterals = {};
                for (int timeSlotIndex = 0; timeSlotIndex < timeSlots; timeSlotIndex++)
                {
                    // Collect overlapping timing literals
                    vector<int> timeSlotLiterals;
                    for (long unsigned int timingIndex = 0; timingIndex < timingVec.size(); timingIndex++)
                    {
                        Timing timing = timingVec[timingIndex];
                        if (timing.start <= timeSlotIndex && timeSlotIndex < timing.start + timing.length)
                        {
                            timeSlotLiterals.push_back(literalVec[timingIndex]);
                        }
                    }

                    int startLiteral = literalCounter;
                    literalCounter++;
                    slotLiterals.push_back(startLiteral);
                    if (timeSlotLiterals.size() == 0)
                        continue;

                    // Slot literal <-> overlapping timing literals
                    ipamir_add_hard(solver, -startLiteral);
                    for (int literal : timeSlotLiterals)
                    {
                        ipamir_add_hard(solver, literal);
                    }
                    ipamir_add_hard(solver, 0);

                    ipamir_add_hard(solver, startLiteral);
                    for (int literal : timeSlotLiterals)
                    {
                        ipamir_add_hard(solver, -literal);
                    }
                    ipamir_add_hard(solver, 0);
                }

                // Create auxiliary literals for block beginnings
                vector<int> beginningLiterals;
                for (long unsigned int slotIndex = 0; slotIndex < slotLiterals.size(); slotIndex++)
                {
                    beginningLiterals.push_back(literalCounter);
                    literalCounter++;
                }

                // Create encoding for block beginnings
                // beginning_i -> no occupancies in last S+1 slots and occupied at slot i
                for (int slotIndex = 0; slotIndex < int(slotLiterals.size()); slotIndex++)
                {
                    int beginningLiteral = beginningLiterals[slotIndex];
                    int timeslotLiteral = slotLiterals[slotIndex];
                    ipamir_add_hard(solver, -beginningLiteral);
                    ipamir_add_hard(solver, timeslotLiteral);
                    ipamir_add_hard(solver, 0);
                    for (int offset = 1; offset <= S + 1 && 0 <= slotIndex - offset; offset++)
                    {
                        int previousTimeslotLiteral = slotLiterals[slotIndex - offset];
                        ipamir_add_hard(solver, -beginningLiteral);
                        ipamir_add_hard(solver, -previousTimeslotLiteral);
                        ipamir_add_hard(solver, 0);
                    }

                    // no occupancies in last S+1 slots and occupied at slot i -> beginning_i
                    ipamir_add_hard(solver, beginningLiteral);
                    ipamir_add_hard(solver, -timeslotLiteral);
                    for (int offset = 1; offset <= S + 1 && 0 <= slotIndex - offset; offset++)
                    {
                        int previousTimeslotLiteral = slotLiterals[slotIndex - offset];
                        ipamir_add_hard(solver, previousTimeslotLiteral);
                    }
                    ipamir_add_hard(solver, 0);
                }

                // Encode am-k starts
                int initialLiteralCounter = literalCounter;
                Totalizer *tot = tot_new();
                for (int beginningLiteral : beginningLiterals)
                    tot_add(tot, beginningLiteral);
                tot_reserve(tot, &literalCounter);
                tot_encode_ub(tot, R + 1, R + 1, &literalCounter, ipamirClauseCollector, solver);
                tot_drop(tot);

                // Last literal represents whether at-most-k holds
                int amkLiteral = literalCounter - 1;
                if (required)
                {
                    ipamir_add_hard(solver, int32_t(-amkLiteral));
                    ipamir_add_hard(solver, 0);
                }
                else
                {
                    ipamir_add_soft_lit(solver, amkLiteral, penalty);
                }
                verboseLog("Added literals :" + to_string(initialLiteralCounter) + " - " + to_string(amkLiteral) + " for at-most-k encoding MaxBreaks(" + to_string(R) + "," + to_string(S) + ")\n");
            }
        }
    }
}

bool timingComp(tuple<int, int, Timing> a, tuple<int, int, Timing> b)
{
    Timing aTiming = get<Timing>(a);
    Timing bTiming = get<Timing>(b);
    return aTiming.start < bTiming.start;
}

void encodeViolatingBlocks(
    void *solver,
    uint32_t literalCounter,
    vector<tuple<int, int, Timing>> &timingVec,
    vector<int> &auxLiterals,
    set<vector<int>> &encodedClauses,
    vector<bool> &usedClasses,
    map<pair<int, int>, int> &startLenVar,
    int index,
    int start,
    int end,
    int M,
    int S,
    bool required,
    int penalty)
{
    for (int next = index + 1; next < (int)timingVec.size(); next++)
    {
        tuple<int, int, Timing> timingTuple = timingVec[next];

        int classId = get<1>(timingTuple);
        Timing timing = get<2>(timingTuple);

        if (usedClasses[classId])
            continue;

        if (timing.start - end > S)
            break;

        int newEnd = max(end, timing.start + timing.length);
        int span = newEnd - start;

        pair<int, int> key = {timing.start, timing.length};
        int aux = startLenVar[key];

        auxLiterals.push_back(aux);
        usedClasses[classId] = true;

        if (span > M && auxLiterals.size() >= 2)
        {
            vector<int> clause;
            for (int l : auxLiterals)
                clause.push_back(-l);
            sort(clause.begin(), clause.end());

            // Check if clause already encoded
            if (encodedClauses.insert(clause).second)
            {
                ipamirAddClause(solver, clause, literalCounter, required, penalty);
                if (verbose)
                {
                    cout << "[VERBOSE] Found MaxBlock violation: [ ";
                    for (int l : auxLiterals)
                        cout << l << " ";
                    cout << "]\n";
                }
            }
            auxLiterals.pop_back();
            usedClasses[classId] = false;

            continue; // do not recurse further
        }

        encodeViolatingBlocks(
            solver,
            literalCounter,
            timingVec,
            auxLiterals,
            encodedClauses,
            usedClasses,
            startLenVar,
            next,
            start,
            newEnd,
            M,
            S,
            required,
            penalty);

        auxLiterals.pop_back();
        usedClasses[classId] = false;
    }
}

// TODO: Check penalty calculation matches ITC
void encodeMaxBlockConstraints(void *solver,
                               uint32_t literalCounter,
                               int weeks,
                               int days,
                               int timeSlots,
                               vector<vector<int>> t,
                               map<string, Class> classMap,
                               map<string, int> classIndexMap,
                               vector<DistributionVariant> distributions)
{
    for (auto &dist : distributions)
    {
        vector<string> distributionClasses;
        bool required;
        int penalty;
        // Max block length
        int M = 0;
        // Required break length
        int S = 0;
        std::visit([&](auto &d)
                   { distributionClasses = d.classes;
                    required = d.required;
                    penalty = d.penalty;
                    using T = std::decay_t<decltype(d)>;
                    if constexpr (std::is_same_v<T, MaxBlockDistribution>)
                    {
                        M = d.M;
                        S = d.S;
                    } },
                   dist);
        for (int weekIndex = 0; weekIndex < weeks; weekIndex++)
        {
            for (int dayIndex = 0; dayIndex < days; dayIndex++)
            {
                vector<tuple<int, int, Timing>> timingVec;
                set<vector<int>> encodedClauses;
                for (string classId : distributionClasses)
                {
                    Class classObj = classMap[classId];
                    for (long unsigned int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
                    {
                        Timing timing = classObj.timings[timingIndex];
                        if (timing.weeks[weekIndex] == '0' || timing.days[dayIndex] == '0')
                            continue;
                        int timingLiteral = t[classIndexMap[classId]][timingIndex];
                        int classIdx = classIndexMap[classId];
                        timingVec.push_back({timingLiteral, classIdx, timing});
                    }
                }

                if (timingVec.size() < 2)
                    // Only one class on given day, no encoding needed
                    continue;

                // Create b_i <-> timings occuring at i
                map<pair<int, int>, int> startLenVar;
                for (tuple<int, int, Timing> tuple : timingVec)
                {
                    Timing timing = get<2>(tuple);
                    pair<int, int> key = {timing.start, timing.length};

                    if (!startLenVar.count(key))
                    {
                        startLenVar[key] = literalCounter++;
                    }

                    int timingLiteral = get<0>(tuple);

                    ipamir_add_hard(solver, -timingLiteral);
                    ipamir_add_hard(solver, startLenVar[key]);
                    ipamir_add_hard(solver, 0);
                    verboseLog("Added clause " + to_string(timingLiteral) + " <-> " + to_string(startLenVar[key]));
                }

                sort(timingVec.begin(), timingVec.end(), timingComp);

                // Check each timing for violating blocks
                for (int index = 0; index < int(timingVec.size()); index++)
                {
                    tuple<int, int, Timing> tuple = timingVec[index];
                    int classId = get<1>(tuple);
                    Timing timing = get<2>(tuple);
                    int start = timing.start;
                    int end = timing.start + timing.length;
                    pair<int, int> key = {timing.start, timing.length};
                    int aux = startLenVar[key];

                    vector<int> auxLiterals = {aux};
                    vector<bool> usedClasses(classIndexMap.size(), false);
                    usedClasses[classId] = true;

                    encodeViolatingBlocks(
                        solver,
                        literalCounter,
                        timingVec,
                        auxLiterals,
                        encodedClauses,
                        usedClasses,
                        startLenVar,
                        index,
                        start,
                        end,
                        M,
                        S,
                        required,
                        penalty);
                }
            }
        }
    }
}

void encodeConstraints(void *solver,
                       uint32_t literalCounter,
                       int weeks,
                       int days,
                       int timeSlots,
                       int classes,
                       vector<vector<int>> t,
                       vector<vector<int>> r,
                       vector<Class> classVec,
                       map<string, Class> classMap,
                       map<string, int> classIndexMap,
                       map<string, vector<DistributionVariant>> distributionsMap)
{
    encodeAtLeast1Timing(solver, classes, t);
    encodeAtLeast1Room(solver, classes, r);
    encodeAM1Timing(solver, literalCounter, classes, t);
    encodeAM1Room(solver, literalCounter, classes, r);
    encodeAssignmentPenalties(solver, classes, t, r, classVec);

    encodeRoomConflictConstraints(solver, classes, weeks, days, t, r, classVec);
    encodeRoomUnavailabilityConstraints(solver, weeks, days, t, r, classVec);

    encodeSameStartConstraint(solver, literalCounter, t, classMap, distributionsMap["SameStart"]);
    encodeSameTimeConstraint(solver, literalCounter, t, classMap, classIndexMap, distributionsMap["SameTime"]);
    encodeDifferentTimeConstraint(solver, literalCounter, t, classMap, classIndexMap, distributionsMap["DifferentTime"]);
    encodeSameDaysConstraint(solver, literalCounter, days, t, classMap, classIndexMap, distributionsMap["SameDays"]);
    encodeDifferentDaysConstraint(solver, literalCounter, days, t, classMap, classIndexMap, distributionsMap["DifferentDays"]);
    encodeSameWeeksConstraints(solver, literalCounter, weeks, t, classMap, classIndexMap, distributionsMap["SameWeeks"]);
    encodeDifferentWeeksConstraints(solver, literalCounter, weeks, t, classMap, classIndexMap, distributionsMap["DifferentWeeks"]);
    encodeSameRoomConstraints(solver, literalCounter, r, classMap, classIndexMap, distributionsMap["SameRoom"]);
    encodeDifferentRoomConstraints(solver, literalCounter, r, classMap, classIndexMap, distributionsMap["DifferentRoom"]);
    encodeOverLapConstraints(solver, literalCounter, weeks, days, t, r, classMap, classIndexMap, distributionsMap["OverLap"]);
    encodeNotOverLapConstraints(solver, literalCounter, weeks, days, t, r, classMap, classIndexMap, distributionsMap["NotOverLap"]);
    encodeSameAttendeesConstraints(solver, literalCounter, weeks, days, t, classMap, classIndexMap, distributionsMap["SameAttendees"]);
    encodeWorkDayConstraints(solver, literalCounter, weeks, days, t, classMap, classIndexMap, distributionsMap["WorkDay"]);
    encodePrecedenceConstraints(solver, literalCounter, weeks, days, t, classMap, classIndexMap, distributionsMap["Precedence"]);
    encodeMinGapConstraints(solver, literalCounter, weeks, days, t, classMap, classIndexMap, distributionsMap["MinGap"]);
    encodeMaxDaysConstraints(solver, literalCounter, weeks, days, t, classVec, classMap, classIndexMap, distributionsMap["MaxDays"]);
    encodeMaxDayLoadConstraints(solver, literalCounter, weeks, days, t, classMap, classIndexMap, distributionsMap["MaxDayLoad"]);
    encodeMaxBreaksConstraints(solver, literalCounter, weeks, days, timeSlots, t, classMap, classIndexMap, distributionsMap["MaxBreaks"]);
    encodeMaxBlockConstraints(solver, literalCounter, weeks, days, timeSlots, t, classMap, classIndexMap, distributionsMap["MaxBlock"]);
}
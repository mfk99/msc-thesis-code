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

    if (verbose)
        cout << "[VERBOSE] Creating clauses for "
             << classes << " classes with "
             << periods << " time periods and "
             << rooms << " rooms.\n";

    // Represents class period assignments
    vector<vector<int>> t(classes);
    // Represents class room assignments
    vector<vector<int>> r(classes);

    uint32_t literalCounter = 1;
    int periodLiteralCounter = 1;

    // Maps course id(name) to 2-slot vector containing first timing and room literal
    map<string, vector<int>> courseIdLookUp;
    vector<vector<string>> courseNames = getCourseNames();
    for (size_t courseIndex = 0; courseIndex < courseNames.size(); courseIndex++)
    {
        vector<string> classNames = courseNames[courseIndex];
        int classIndex = 0;
        for (string className : classNames)
        {
            int timingLiteral = (courseIndex * courseHours + classIndex) * weeks * days * hours + 1;
            int roomLiteral = weeks * days * hours * courses * courseHours + (courseIndex * courses + classIndex) * rooms + 1;
            courseIdLookUp[className] = {timingLiteral, roomLiteral};
            classIndex++;
        }
    }

    // Map of distributions, used for generating distribution encodings
    map<string, vector<Distribution>> distributionsMap = getDistributions();

    // Helper map, for each class returns first timing literal and n.o. literals for specified class
    map<int, vector<int>> classTimingLookUp;

    // Helper map, for each class returns first room literal and n.o. literals for specified class
    map<int, vector<int>> classRoomLookUp;

    // Helper vector, accessed with values `[course][courseHour][week][day]`
    vector<vector<vector<vector<int>>>> periodLiterals(courses, vector<vector<vector<int>>>(courseHours, vector<vector<int>>(weeks, vector<int>(days, 0))));
    for (long long i = 0; i < courses; i++)
    {
        for (long long i2 = 0; i2 < courseHours; i2++)
        {
            for (long long i3 = 0; i3 < weeks; i3++)
            {
                for (long long i4 = 0; i4 < days; i4++)
                {
                    periodLiterals[i][i2][i3][i4] = periodLiteralCounter;
                    if (verbose)
                        cout << "[VERBOSE] adding hourLiteral: " << periodLiteralCounter << "\n";
                    periodLiteralCounter += hours;
                };
            }
        }
    }

    // Initialize t
    for (long long i = 0; i < classes; i++)
    {
        int initialLiteralCounter = literalCounter;
        vector<int> periodAssignment(periods);
        iota(periodAssignment.begin(), periodAssignment.end(), literalCounter);
        t[i] = periodAssignment;
        literalCounter += periods;
        if (verbose)
        {
            cout << "[VERBOSE] Created literals " << initialLiteralCounter << "-" << literalCounter - 1
                 << " in t for class " << i << "\n";
        }
    }

    if (verbose)
    {
        cout << "[VERBOSE] literalCounter:" << literalCounter << "\n";
    }

    // Initialize r
    for (long long i = 0; i < classes; i++)
    {
        int initialLiteralCounter = literalCounter;
        vector<int> roomAssignment(rooms);
        iota(roomAssignment.begin(), roomAssignment.end(), literalCounter);
        r[i] = roomAssignment;
        literalCounter += rooms;
        if (verbose)
        {
            cout << "[VERBOSE] Created literals " << initialLiteralCounter << "-" << literalCounter - 1
                 << " in r for class " << i << "\n";
        }
    }

    if (verbose)
    {
        cout << "[VERBOSE] literalCounter:" << literalCounter << "\n";
    }

    vector<vector<int>> periodAssigned;
    vector<vector<int>> roomAssigned;

    for (long long i = 0; i < classes; i++)
    {
        vector<int> periodLiterals = t[i];
        periodAssigned.push_back(periodLiterals);

        vector<int> roomLiterals = r[i];
        roomAssigned.push_back(roomLiterals);
    }

    if (verbose)
    {
        cout << "[VERBOSE] periodAssigned: \n";
        for (long long i = 0; i < classes; i++)
        {

            for (long long i2 = 0; i2 < periods; i2++)
            {
                cout << "[" << periodAssigned[i][i2] << "]";
            }
            cout << "\n";
        }

        cout << "[VERBOSE] roomAssigned: \n";
        for (long long i = 0; i < classes; i++)
        {
            for (long long i2 = 0; i2 < rooms; i2++)
            {
                cout << "[" << roomAssigned[i][i2] << "]";
            }
            cout << "\n";
        }
    }

    // Encode at-least-one period constraints
    for (long long i = 0; i < classes; i++)
    {
        for (long long i2 = 0; i2 < periods; i2++)
        {
            int lit = t[i][i2];
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
    }

    // Encode at-least-one room constraints
    for (long long i = 0; i < classes; i++)
    {
        for (long long i2 = 0; i2 < rooms; i2++)
        {
            int lit = r[i][i2];
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
    }

    string am1EncoderType = "pairwise";

    // Encode am1 period constraints
    for (long long i = 0; i < classes; i++)
    {
        if (verbose)
            cout << "[VERBOSE] Adding am1 period constraints for literals[";
        AM1Encoder am1Encoder = AM1Encoder(am1EncoderType);
        for (long long i2 = 0; i2 < periods; i2++)
        {
            int lit = t[i][i2];
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
        if (verbose)
            cout << "[VERBOSE] Adding am1 room constraints for literals[";
        AM1Encoder am1Encoder = AM1Encoder(am1EncoderType);
        for (long long i2 = 0; i2 < rooms; i2++)
        {
            int lit = r[i][i2];
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
    for (long long class1 = 0; class1 < classes; class1++)
    {
        for (long long class2 = class1 + 1; class2 < classes; class2++)
        {
            for (long long period = 0; period < periods; period++)
            {
                for (long long room = 0; room < rooms; room++)
                {
                    ipamir_add_hard(solver, -t[class1][period]);
                    ipamir_add_hard(solver, -t[class2][period]);
                    ipamir_add_hard(solver, -r[class1][room]);
                    ipamir_add_hard(solver, -r[class2][room]);
                    ipamir_add_hard(solver, 0);
                    if (verbose)
                    {
                        cout << "[VERBOSE] Adding RoomConflict constraint: "
                             << -t[class1][period] << ", "
                             << -t[class2][period] << ", "
                             << -r[class1][room] << ", "
                             << -r[class2][room] << ", 0 \n";
                    }
                }
            }
        }
    }

    // Encode RoomUnavailability constraints
    vector<vector<vector<vector<int>>>> roomAvailability = getRoomAvailability();
    for (int i = 0; i < rooms; i++)
    {
        for (int i2 = 0; i2 < weeks; i2++)
        {
            for (int i3 = 0; i3 < days; i3++)
            {
                for (int i4 = 0; i4 < hours; i4++)
                {
                    if (roomAvailability[i][i2][i3][i4] == 1)
                    {

                        for (int i5 = 0; i5 < classes; i5++)
                        {
                            int periodIndex = i2 * days * hours + i3 * hours + i4;
                            int timingLit = t[i5][periodIndex];
                            int roomLit = r[i5][i];
                            if (verbose)
                                cout << "[VERBOSE] Adding RoomUnavailability constraint: -" << timingLit << ", -" << roomLit << ", 0 \n";
                            ipamir_add_hard(solver, -timingLit);
                            ipamir_add_hard(solver, -roomLit);
                            ipamir_add_hard(solver, 0);
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
        for (size_t class1 = 0; class1 < distributionClasses.size(); class1++)
        {
            string class1Id = distributionClasses[class1];
            int classPeriod1Literal = courseIdLookUp[class1Id][0];

            for (size_t class2 = class1 + 1; class2 < distributionClasses.size(); class2++)
            {
                string class2Id = distributionClasses[class2];
                int classPeriod2Literal = courseIdLookUp[class2Id][0];
                for (long long week1 = 0; week1 < weeks; week1++)
                {
                    for (long long day1 = 0; day1 < days; day1++)
                    {
                        for (long long hour1 = 0; hour1 < hours; hour1++)
                        {
                            int periodLit1 = classPeriod1Literal +
                                             (week1 * days * hours) +
                                             (day1 * hours) + hour1;
                            for (long long week2 = 0; week2 < weeks; week2++)
                            {
                                for (long long day2 = 0; day2 < days; day2++)
                                {
                                    for (long long hour2 = 0; hour2 < hours; hour2++)
                                    {
                                        if (hour1 == hour2)
                                            continue;

                                        int periodLit2 = classPeriod2Literal +
                                                         (week2 * days * hours) +
                                                         (day2 * hours) + hour2;
                                        if (verbose)
                                            cout << "[VERBOSE] Adding SameStart constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                        ipamirAddClause(solver,
                                                        {-periodLit1, -periodLit2},
                                                        literalCounter,
                                                        sameStartDistribution.required,
                                                        sameStartDistribution.weight);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameDays constraints
    for (Distribution sameDaysDistribution : distributionsMap["SameDays"])
    {
        vector<string> distributionClasses = sameDaysDistribution.classes;
        for (size_t class1 = 0; class1 < distributionClasses.size(); class1++)
        {
            string class1Id = distributionClasses[class1];
            int classPeriod1Literal = courseIdLookUp[class1Id][0];

            for (size_t class2 = class1 + 1; class2 < distributionClasses.size(); class2++)
            {
                string class2Id = distributionClasses[class2];
                int classPeriod2Literal = courseIdLookUp[class2Id][0];
                for (long long week1 = 0; week1 < weeks; week1++)
                {
                    for (long long day1 = 0; day1 < days; day1++)
                    {
                        for (long long hour1 = 0; hour1 < hours; hour1++)
                        {
                            int periodLit1 = classPeriod1Literal +
                                             (week1 * days * hours) +
                                             (day1 * hours) + hour1;
                            for (long long week2 = 0; week2 < weeks; week2++)
                            {
                                for (long long day2 = 0; day2 < days; day2++)
                                {
                                    if (day1 == day2)
                                        continue;
                                    for (long long hour2 = 0; hour2 < hours; hour2++)
                                    {

                                        int periodLit2 = classPeriod2Literal +
                                                         (week2 * days * hours) +
                                                         (day2 * hours) + hour2;
                                        if (verbose)
                                            cout << "[VERBOSE] Adding SameDays constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                        ipamirAddClause(solver,
                                                        {-periodLit1, -periodLit2},
                                                        literalCounter,
                                                        sameDaysDistribution.required,
                                                        sameDaysDistribution.weight);
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
    int maxWorkDayLength = hours;
    for (long long week = 0; week < weeks; week++)
    {
        for (long long day = 0; day < days; day++)
        {
            for (long long course1 = 0; course1 < courses; course1++)
            {
                for (long long courseHour1 = 0; courseHour1 < courseHours; courseHour1++)
                {
                    for (long long hour1 = 0; hour1 < hours; hour1++)
                    {
                        int periodLit1 = periodLiterals[course1][courseHour1][week][day] + hour1;

                        for (long long course2 = course1; course2 < courses; course2++)
                        {
                            for (long long courseHour2 = courseHour1; courseHour2 < courseHours; courseHour2++)
                            {
                                for (long long hour2 = hour1; hour2 < hours; hour2++)
                                {
                                    int periodLit2 = periodLiterals[course2][courseHour2][week][day] + hour2;
                                    if (maxWorkDayLength <= (abs(hour1 - hour2)))
                                    {
                                        if (verbose)
                                            cout
                                                << "[VERBOSE] Adding WorkDay constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                        ipamir_add_hard(solver, -periodLit1);
                                        ipamir_add_hard(solver, -periodLit2);
                                        ipamir_add_hard(solver, 0);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode NotOverLap constraints
    for (long long week = 0; week < weeks; week++)
    {
        for (long long day = 0; day < days; day++)
        {
            for (long long hour = 0; hour < hours; hour++)
            {
                for (long long course1 = 0; course1 < courses; course1++)
                {
                    for (long long courseHour1 = 0; courseHour1 < courseHours; courseHour1++)
                    {
                        int periodLit1 = periodLiterals[course1][courseHour1][week][day] + hour;

                        for (long long course2 = course1; course2 < courses; course2++)
                        {
                            long long courseHour2;
                            if (course2 == course1)
                                courseHour2 = courseHour1;
                            else
                                courseHour2 = 0;
                            for (; courseHour2 < courseHours; courseHour2++)
                            {
                                int periodLit2 = periodLiterals[course2][courseHour2][week][day] + hour;
                                if (periodLit1 == periodLit2)
                                    continue;
                                if (verbose)
                                    cout
                                        << "[VERBOSE] Adding NotOverLap constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                ipamir_add_hard(solver, -periodLit1);
                                ipamir_add_hard(solver, -periodLit2);
                                ipamir_add_hard(solver, 0);
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
        for (size_t class1 = 0; class1 < distributionClasses.size(); class1++)
        {
            string class1Id = distributionClasses[class1];
            int classPeriod1Literal = courseIdLookUp[class1Id][0];

            for (size_t class2 = class1 + 1; class2 < distributionClasses.size(); class2++)
            {
                string class2Id = distributionClasses[class2];
                int classPeriod2Literal = courseIdLookUp[class2Id][0];
                for (long long week1 = 0; week1 < weeks; week1++)
                {
                    for (long long day1 = 0; day1 < days; day1++)
                    {
                        for (long long hour1 = 0; hour1 < hours; hour1++)
                        {
                            int periodLit1 = classPeriod1Literal +
                                             (week1 * days * hours) +
                                             (day1 * hours) + hour1;
                            for (long long week2 = 0; week2 < weeks; week2++)
                            {
                                if (week1 == week2)
                                    continue;
                                for (long long day2 = 0; day2 < days; day2++)
                                {
                                    for (long long hour2 = 0; hour2 < hours; hour2++)
                                    {

                                        int periodLit2 = classPeriod2Literal +
                                                         (week2 * days * hours) +
                                                         (day2 * hours) + hour2;
                                        if (verbose)
                                            cout << "[VERBOSE] Adding SameWeeks constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                                        ipamirAddClause(solver,
                                                        {-periodLit1, -periodLit2},
                                                        literalCounter,
                                                        sameWeeksDistribution.required,
                                                        sameWeeksDistribution.weight);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Encode SameRoom constraints
    for (Distribution sameRoomsDistribution : distributionsMap["SameRoom"])
    {
        vector<string> distributionClasses = sameRoomsDistribution.classes;
        for (size_t class1 = 0; class1 < distributionClasses.size(); class1++)
        {
            string class1Id = distributionClasses[class1];
            int classRoom1Literal = courseIdLookUp[class1Id][1];

            for (size_t class2 = class1 + 1; class2 < distributionClasses.size(); class2++)
            {
                string class2Id = distributionClasses[class2];
                int classRoom2Literal = courseIdLookUp[class2Id][1];
                for (long long room1 = 0; room1 < rooms; room1++)
                {
                    int roomLit1 = classRoom1Literal + room1;
                    for (long long room2 = 0; room2 < rooms; room2++)
                    {
                        if (room1 == room2)
                            continue;
                        int roomLit2 = classRoom2Literal + room2;

                        if (verbose)
                            cout << "[VERBOSE] Adding SameRoom constraint: -" << roomLit1 << ", -" << roomLit2 << ", 0 \n";
                        ipamirAddClause(solver,
                                        {-roomLit1, -roomLit2},
                                        literalCounter,
                                        sameRoomsDistribution.required,
                                        sameRoomsDistribution.weight);
                    }
                }
            }
        }
    }

    // Encode Precedence constraints
    for (Distribution precedenceDistribution : distributionsMap["Precedence"])
    {
        vector<string> distributionClasses = precedenceDistribution.classes;
        for (size_t class1 = 0; class1 < distributionClasses.size(); class1++)
        {
            string class1Id = distributionClasses[class1];
            int classPeriod1Literal = courseIdLookUp[class1Id][0];

            for (size_t class2 = class1 + 1; class2 < distributionClasses.size(); class2++)
            {
                string class2Id = distributionClasses[class2];
                int classPeriod2Literal = courseIdLookUp[class2Id][0];

                int maxPeriodOffset = periods;

                for (long long period1Offset = 0; period1Offset < maxPeriodOffset; period1Offset++)
                {
                    int periodLit1 = classPeriod1Literal + period1Offset;
                    for (long long period2Offset = 0; period2Offset < period1Offset; period2Offset++)
                    {
                        int periodLit2 = classPeriod2Literal + period2Offset;
                        if (verbose)
                            cout << "[VERBOSE] v.2 Adding Precedence constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
                        ipamirAddClause(solver,
                                        {-periodLit1, -periodLit2},
                                        literalCounter,
                                        precedenceDistribution.required,
                                        precedenceDistribution.weight);
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
            for (size_t i = 0; i < courseNames.size(); i++)
            {
                vector<string> classNames = courseNames[i];
                int classIndex = 0;
                for (string className : classNames)
                {

                    int period = 0;
                    int room = 0;
                    for (int i2 = 0; i2 < periods; i2++)
                    {
                        int periodLit = t[i * courses + classIndex][i2];
                        if (0 < ipamir_val_lit(solver, periodLit))
                        {
                            period = i2;
                            if (verbose)
                                cout << "[VERBOSE] periodLit:" << periodLit << "=1 \n";
                            break;
                        }
                    }

                    for (int i2 = 0; i2 < rooms; i2++)
                    {
                        int roomLit = r[i * courses + classIndex][i2];
                        if (0 < ipamir_val_lit(solver, roomLit))
                        {
                            room = i2;
                            if (verbose)
                                cout << "[VERBOSE] roomLit:" << roomLit << "=1 \n";
                            break;
                        }
                    }
                    classIndex++;
                    cout << "Class " << className << " is assigned to room " << room << " in period " << period << "\n";
                }
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
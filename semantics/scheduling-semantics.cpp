#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include "../input-parser/input-parser.h"
#include "../config/config.h"
#include "../../../ipamir.h"
#include "../../../rustsat/capi/rustsat.h"
using namespace std;
using namespace RustSAT;

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

    int literalCounter = 1;
    int periodLiteralCounter = 1;

    vector<vector<vector<vector<int>>>> periodLiterals; // Helper vector
    //[course][week][day][hour]
    for (long long i = 0; i < weeks; i++)
    {
        vector<vector<vector<int>>> weekLiterals;
        for (long long i2 = 0; i2 < days; i2++)
        {
            vector<vector<int>> dayLiterals;
            for (long long i3 = 0; i3 < hours; i3++)
            {
                vector<int> hourLiterals;
                for (long long i4 = 0; i4 < courses; i4++)
                {
                    hourLiterals.push_back(periodLiteralCounter);
                    if (verbose)
                        cout << "[VERBOSE] adding hourLiteral: " << periodLiteralCounter << "\n";
                    for (long long i5 = 0; i5 < rooms; i5++)
                    {
                        periodLiteralCounter++;
                    }
                }
                dayLiterals.push_back(hourLiterals);
            }
            weekLiterals.push_back(dayLiterals);
        }
        periodLiterals.push_back(weekLiterals);
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

    for (long long i = 0; i < classes; i++)
    {
        for (long long i2 = 0; i2 < periods; i2++)
        {
            int lit = t[i][i2];
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
    }

    for (long long i = 0; i < classes; i++)
    {
        for (long long i2 = 0; i2 < rooms; i2++)
        {
            int lit = r[i][i2];
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
    }

    // Encode am1 period pairwise constraints
    for (long long i = 0; i < classes; i++)
    {
        Pairwise *pairwise = pairwise_new();
        for (long long i2 = 0; i2 < periods; i2++)
        {
            int lit = t[i][i2];
            pairwise_add(pairwise, lit);
        }
        pairwise_encode(pairwise, 0, ipamirClauseCollector, solver);
        pairwise_drop(pairwise);
    }

    // Encode am1 room pairwise constraints
    for (long long i = 0; i < classes; i++)
    {
        Pairwise *pairwise = pairwise_new();
        for (long long i2 = 0; i2 < rooms; i2++)
        {
            int lit = r[i][i2];
            pairwise_add(pairwise, lit);
        }
        pairwise_encode(pairwise, 0, ipamirClauseCollector, solver);
        pairwise_drop(pairwise);
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
                    if (!roomAvailability[i][i2][i3][i4])
                    {
                        for (int i5 = 0; i5 < classes; i5++)
                        {
                            int periodIndex = i2 * courseHours + i3;
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
    for (long long course = 0; course < courses; course++)
    {
        for (long long courseHour1 = 0; courseHour1 < courseHours; courseHour1++)
        {
            for (long long week1 = 0; week1 < weeks; week1++)
            {
                for (long long day1 = 0; day1 < days; day1++)
                {
                    for (long long hour1 = 0; hour1 < hours; hour1++)
                    {
                        int periodLit1 = periodLiterals[course][week1][day1][hour1] + courseHour1;

                        for (long long courseHour2 = courseHour1 + 1; courseHour2 < courseHours; courseHour2++)
                        {
                            for (long long week2 = week1 + 1; week2 < weeks; week2++)
                            {
                                for (long long day2 = day1 + 1; day2 < days; day2++)
                                {
                                    for (long long hour2 = hour1 + 1; hour2 < hours; hour2++)
                                    {
                                        int periodLit2 = periodLiterals[course][week2][day2][hour2] + courseHour2;
                                        if (verbose)
                                            cout << "[VERBOSE] Adding SameStart constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
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

    // Encode SameDays constraints
    for (long long course = 0; course < courses; course++)
    {
        for (long long courseHour1 = 0; courseHour1 < courseHours; courseHour1++)
        {
            for (long long day1 = 0; day1 < days; day1++)
            {
                for (long long week1 = 0; week1 < weeks; week1++)
                {
                    for (long long hour1 = 0; hour1 < hours; hour1++)
                    {
                        int periodLit1 = periodLiterals[course][courseHour1][week1][day1] + hour1;

                        for (long long courseHour2 = courseHour1 + 1; courseHour2 < courseHours; courseHour2++)
                        {
                            for (long long day2 = day1 + 1; day2 < days; day2++)
                            {
                                for (long long week2 = week1; week2 < weeks; week2++)
                                {
                                    for (long long hour2 = 0; hour2 < hours; hour2++)
                                    {
                                        int periodLit2 = periodLiterals[course][courseHour2][week2][day2] + hour2;
                                        if (verbose)
                                            cout << "[VERBOSE] Adding SameDays constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
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

    // Encode WorkDay constraints
    int maxWorkDayLength = hours - 1;
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

    // Encode SameWeeks constraints
    for (long long course = 0; course < courses; course++)
    {
        for (long long courseHour1 = 0; courseHour1 < courseHours; courseHour1++)
        {
            for (long long week1 = 0; week1 < weeks; week1++)
            {
                for (long long day1 = 0; day1 < days; day1++)
                {
                    for (long long hour1 = 0; hour1 < hours; hour1++)
                    {
                        int periodLit1 = periodLiterals[course][courseHour1][week1][day1] + hour1;

                        for (long long courseHour2 = courseHour1 + 1; courseHour2 < courseHours; courseHour2++)
                        {
                            for (long long week2 = week1 + 1; week2 < weeks; week2++)
                            {
                                for (long long day2 = 0; day2 < days; day2++)
                                {
                                    for (long long hour2 = 0; hour2 < hours; hour2++)
                                    {
                                        int periodLit2 = periodLiterals[course][courseHour2][week2][day2] + hour2;
                                        if (verbose)
                                            cout << "[VERBOSE] Adding SameWeeks constraint: -" << periodLit1 << ", -" << periodLit2 << ", 0 \n";
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

    // Encode Precedence constraints
    for (long long courseHour1 = 0; courseHour1 <= courses; courseHour1 += courseHours)
    {
        for (long long courseHour2 = courseHour1 + 1; courseHour2 < courseHour1 + courseHours; courseHour2++)
        {
            for (long long period1 = 0; period1 < periods; period1++)
            {
                int timing1Lit = t[courseHour1][period1];
                for (long long period2 = 0; period2 < period1; period2++)
                {
                    int timing2Lit = t[courseHour2][period2];
                    if (verbose)
                        cout << "[VERBOSE] Adding precedence constraint: -" << timing1Lit << ", -" << timing2Lit << ", 0 \n";
                    ipamir_add_hard(solver, -timing1Lit);
                    ipamir_add_hard(solver, -timing2Lit);
                    ipamir_add_hard(solver, 0);
                }
            }
        }
    }

    // Encode SameRoom constraints
    for (long long i = 0; i <= courses; i++)
    {
        for (long long i2 = i + 1; i2 % courses != 0; i2++)
        {
            for (long long i3 = 0; i3 < rooms; i3++)
            {
                int room1Lit = r[i][i3];
                for (long long i4 = 0; i4 < rooms; i4++)
                {
                    if (i3 == i4)
                        continue;
                    int room2Lit = r[i2][i4];
                    if (verbose)
                        cout << "[VERBOSE] Adding SameRoom constraint: -" << room1Lit << ", -" << room2Lit << ", 0 \n";
                    ipamir_add_hard(solver, -room1Lit);
                    ipamir_add_hard(solver, -room2Lit);
                    ipamir_add_hard(solver, 0);
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
            for (int i = 0; i < classes; i++)
            {
                int period = 0;
                int room = 0;
                for (int i2 = 0; i2 < periods; i2++)
                {
                    int periodLit = t[i][i2];
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
                    int roomLit = r[i][i2];
                    if (0 < ipamir_val_lit(solver, roomLit))
                    {
                        room = i2;
                        if (verbose)
                            cout << "[VERBOSE] roomLit:" << roomLit << "=1 \n";
                        break;
                    }
                }
                cout << "Class " << i << " is assigned to room " << room << " in period " << period << "\n";
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
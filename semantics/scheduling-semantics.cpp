#include <iostream>
#include <fstream>
#include <vector>
#include <limits>
#include <string>
#include <chrono>
#include "../input-parser/input-parser.h"
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

vector<int> parseGenerationVariablesFromFile(string filePath)
{
    vector<int> generationVariables;
    string s;

    // Read from the text file
    ifstream readStream(filePath);
    for (int i = 0; i < 5; i++)
    {
        getline(readStream, s);
        size_t pos = s.find(" ");
        if (pos != string::npos)
        {
            string valueStr = s.substr(pos + 1);
            int value = stoi(valueStr);
            generationVariables.push_back(value);
        }
    }
    readStream.close();

    return generationVariables;
}

void runBenchMark(string encodingFilePath)
{
    vector<int> generationVariables = parseGenerationVariablesFromFile(encodingFilePath);
    void *solver = ipamir_init();

    int days = generationVariables[0];
    int hours = generationVariables[1];
    int classRooms = generationVariables[2];
    int courses = generationVariables[3];
    int courseHours = generationVariables[4];

    int timeSlots = days * hours;
    int totalCourseHours = courses * courseHours;

    cout << "Creating clauses for " << courseHours << " course hours with " << timeSlots << " available timeslots and " << classRooms << " classrooms.\n";

    //

    /*
    Literals indicate whether a class is held in a specified room in a specified timeslot
    Constraints:
    1. Two classes can't have the same room
    2. Every class must have a classroom
    */

    cout << "Creating clauses for " << courseHours << " hours per course ("
         << totalCourseHours << " course-hours total) with "
         << timeSlots << " available timeslots and "
         << classRooms << " classrooms.\n";

    // Create 3D vector of literals:
    vector<vector<vector<int>>> literals;
    literals.resize((size_t)totalCourseHours);
    int literalCounter = 1;
    for (long long c = 0; c < totalCourseHours; c++)
    {
        literals[(size_t)c].resize((size_t)classRooms);
        for (int r = 0; r < classRooms; r++)
        {
            literals[(size_t)c][r].resize((size_t)timeSlots);
            for (int t = 0; t < timeSlots; t++)
            {
                if (verbose)
                {
                    cout << "[VERBOSE] Creating literal " << literalCounter
                         << " for courseHour=" << c << " room=" << r << " timeslot=" << t << "\n";
                }
                literals[(size_t)c][r][t] = literalCounter++;
            }
        }
    }

    long long vars = (long long)literalCounter - 1;
    if (verbose)
        cout << "[VERBOSE] Total variables (literals) = " << vars << "\n";

    // Clause vectors
    vector<vector<int>> mustHaveRoomClauses;
    vector<vector<int>> atMostOneClauses;
    vector<vector<int>> roomConflictClauses;

    // Build must-have clauses: for each course-hour, at least one (room,timeslot)
    mustHaveRoomClauses.reserve((size_t)totalCourseHours);
    for (long long c = 0; c < totalCourseHours; c++)
    {
        vector<int> clause;
        clause.reserve((size_t)classRooms * (size_t)timeSlots);
        for (int r = 0; r < classRooms; r++)
        {
            for (int t = 0; t < timeSlots; t++)
            {
                clause.push_back(literals[(size_t)c][r][t]); // positive literal
            }
        }
        mustHaveRoomClauses.push_back(std::move(clause));
    }

    // Build at-most-one clauses: for each course-hour, pairwise negative literals
    // (i.e., a single course-hour cannot occupy two different room-timeslot literals)
    atMostOneClauses.reserve((size_t)totalCourseHours * 10); // soft reserve; may grow more
    for (long long c = 0; c < totalCourseHours; c++)
    {
        // flatten this course-hour's literals to a vector to produce pairs
        vector<int> flat;
        flat.reserve((size_t)classRooms * (size_t)timeSlots);
        for (int r = 0; r < classRooms; r++)
        {
            for (int t = 0; t < timeSlots; t++)
            {
                flat.push_back(literals[(size_t)c][r][t]);
            }
        }
        for (size_t i = 0; i < flat.size(); ++i)
        {
            for (size_t j = i + 1; j < flat.size(); ++j)
            {
                atMostOneClauses.push_back(vector<int>{-flat[i], -flat[j]});
            }
        }
    }

    // Build room conflict clauses: two DIFFERENT course-hours cannot occupy same room AND timeslot
    // For each pair of different course-hours (a,b), and for each (room, timeslot): (-litA OR -litB)
    roomConflictClauses.reserve((size_t)((totalCourseHours * (totalCourseHours - 1) / 2) * classRooms * timeSlots / 10 + 1));
    for (long long a = 0; a < totalCourseHours; a++)
    {
        for (long long b = a + 1; b < totalCourseHours; b++)
        {
            for (int r = 0; r < classRooms; r++)
            {
                for (int t = 0; t < timeSlots; t++)
                {
                    int litA = literals[(size_t)a][r][t];
                    int litB = literals[(size_t)b][r][t];
                    roomConflictClauses.push_back(vector<int>{-litA, -litB});
                }
            }
        }
    }

    // Count clauses
    long long clauseCount = 0;
    clauseCount += (long long)mustHaveRoomClauses.size();
    clauseCount += (long long)atMostOneClauses.size();
    clauseCount += (long long)roomConflictClauses.size();

    // Output in DIMACS CNF format
    if (verbose)
        cout << "[VERBOSE] p cnf " << vars << " " << clauseCount << "\n";

    // print must-have (positive) clauses and add to ipamir
    for (const auto &cl : mustHaveRoomClauses)
    {
        for (int lit : cl)
        {
            if (verbose)
                cout << "[VERBOSE]";
            if (verbose)
                cout << lit << " ";
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
        if (verbose)
            cout << "0\n";
    }

    // print at-most-one clauses and add to ipamir
    for (const auto &cl : atMostOneClauses)
    {
        if (verbose)
            cout << "[VERBOSE]";
        for (int lit : cl)
        {
            if (verbose)
                cout << lit << " ";
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
        if (verbose)
            cout << "0\n";
    }

    // print room conflict clauses and add to ipamir
    for (const auto &cl : roomConflictClauses)
    {
        if (verbose)
            cout << "[VERBOSE]";

        for (int lit : cl)
        {
            if (verbose)
                cout << lit << " ";
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
        if (verbose)
            cout << "0\n";
    }

    if (verbose)
    {
        cout << "[VERBOSE] Clauses: mustHave=" << mustHaveRoomClauses.size()
             << " atMostOne=" << atMostOneClauses.size()
             << " roomConflicts=" << roomConflictClauses.size()
             << " total=" << clauseCount << "\n";
    }

    cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    // Print answer and ask user for input

    while (true)
    {
        std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
        int code = ipamir_solve(solver);
        std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
        long long timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
        cout << "Solving took:" << timeDiff << "[µs], " << timeDiff / 1000000.0 << "[s] \n";
        cout << "Code returned by ipamir: " << code << "\n";
        if (code == 30)
        {
            cout << "Assignment:\n";
            for (int i = 0; i < totalCourseHours; i++)
            {
                for (int i2 = 0; i2 < classRooms; i2++)
                {
                    for (int i3 = 0; i3 < timeSlots; i3++)
                    {
                        int literal = literals[i][i2][i3];
                        int literal_assignment = ipamir_val_lit(solver, literal);
                        if (literal_assignment > 0)
                        {
                            cout << "Coursehour " << i << " assigned to classroom " << i2 << " at timeslot " << i3 << "\n";
                        }
                    }
                }
                if (verbose)
                {
                    cout << "Full assignment:\n";
                    int lit_assignment = ipamir_val_lit(solver, i);
                    cout << lit_assignment << "\n";
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
                cout << "Adding literal " << lit << "\n";
            }
            ipamir_add_hard(solver, lit);
        }
    }
    cout << "Releasing ipamir...\n";
    ipamir_release(solver);
    cout << "Exiting...\n";
}
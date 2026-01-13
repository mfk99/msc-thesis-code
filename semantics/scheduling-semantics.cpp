#include <iostream>
#include <fstream>
#include <vector>
#include <limits>
#include <string>
#include <chrono>
#include <bits/stdc++.h>
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

void ipamirClauseCollector(int lit, void *solver)
{
    if (verbose)
        cout << "[VERBOSE] Adding:" << lit << " to solver \n";
    ipamir_add_hard(solver, lit);
}

void runBenchMark(string encodingFilePath)
{
    vector<int> generationVariables = parseGenerationVariablesFromFile(encodingFilePath);
    void *solver = ipamir_init();

    int days = generationVariables[0];
    int hours = generationVariables[1];
    int rooms = generationVariables[2];
    int courses = generationVariables[3];
    int courseHours = generationVariables[4];

    int periods = days * hours;
    int classes = courses * courseHours;

    if (verbose)
        cout << "[VERBOSE] Creating clauses for " << classes << " classes with " << periods << " time periods and " << rooms << " rooms.\n";

    // Represents class period assignments
    vector<vector<int>> t(classes);
    // Represents class room assignments
    vector<vector<int>> r(classes);

    int literalCounter = 1;

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
                }
            }
        }
    }

    // Encode Precedence constraints
    // TODO: Make classes into seperate vectors and adjust accordingly
    for (long long i = 0; i <= courses; i++)
    {
        for (long long i2 = i + 1; i2 % courses != 0; i2++)
        {
            for (long long i3 = 0; i3 < classes; i3++)
            {
                int timing1Lit = t[i][i3];
                for (long long i4 = 0; i4 < i3; i4++)
                {
                    int timing2Lit = t[i2][i4];
                    if (verbose)
                        cout << "[VERBOSE] Adding precedence constraint: -" << timing1Lit << ", -" << timing2Lit << ", 0 \n";
                    ipamir_add_hard(solver, -timing1Lit);
                    ipamir_add_hard(solver, -timing2Lit);
                    ipamir_add_hard(solver, 0);
                }
            }
        }
    }

    // TODO (?): Add config file generation and parsing

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
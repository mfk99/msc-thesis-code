#include "../input-parser/input-parser.h"
#include "../../../ipamir.h"
#include "../../../rustsat/capi/rustsat.h"
#include "scheduling-encoding-generator.h"
#include <iostream>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <fstream>
#include <filesystem>
using namespace std;
using namespace RustSAT;

void generateEncoding(vector<int> generationVariables)
{
    string encodingFilePath = getFilePath();
    int days = generationVariables[0];
    int hours = generationVariables[1];
    int classRooms = generationVariables[2];
    int courses = generationVariables[3];
    int courseHours = generationVariables[4];

    int timeSlots = days * hours;
    int totalCourseHours = courses * courseHours;

    if (verbose)
        cout << "Creating clauses for " << courseHours << " course hours with " << timeSlots << " available timeslots and " << classRooms << " classrooms.\n";

    //

    /*
    Literals indicate whether a class is held in a specified room in a specified timeslot
    Constraints:
    1. Two classes can't have the same room
    2. Every class must have a classroom
    */
    if (verbose)
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
    if (verbose)
        printEncoding(vars, clauseCount, mustHaveRoomClauses, atMostOneClauses, roomConflictClauses);
    writeEncodingToFile(encodingFilePath, vars, clauseCount, mustHaveRoomClauses, atMostOneClauses, roomConflictClauses, generationVariables);
}

void printEncoding(long long vars, long long clauseCount,
                   vector<vector<int>> mustHaveRoomClauses,
                   vector<vector<int>> atMostOneClauses,
                   vector<vector<int>> roomConflictClauses)
{
    // Output in DIMACS CNF format
    cout << "[VERBOSE] p cnf " << vars << " " << clauseCount << "\n";

    // print must-have (positive) clauses
    for (const auto &cl : mustHaveRoomClauses)
    {
        cout << "[VERBOSE]";
        for (int lit : cl)
        {

            cout << lit << " ";
        }
        cout << "0\n";
    }

    // print at-most-one clauses and add to ipamir
    for (const auto &cl : atMostOneClauses)
    {
        cout << "[VERBOSE]";
        for (int lit : cl)
        {
            cout << lit << " ";
        }
        cout << "0\n";
    }

    // print room conflict clauses and add to ipamir
    for (const auto &cl : roomConflictClauses)
    {
        cout << "[VERBOSE]";
        for (int lit : cl)
        {
            cout << lit << " ";
        }
        cout << "0\n";
    }

    cout << "[VERBOSE] Clauses: mustHave=" << mustHaveRoomClauses.size()
         << " atMostOne=" << atMostOneClauses.size()
         << " roomConflicts=" << roomConflictClauses.size()
         << " total=" << clauseCount << "\n";
}

void writeEncodingToFile(string encodingFilePath, long long vars, long long clauseCount,
                         vector<vector<int>> mustHaveRoomClauses,
                         vector<vector<int>> atMostOneClauses,
                         vector<vector<int>> roomConflictClauses,
                         vector<int> generationVariables)
{
    if (verbose)
        cout
            << "Writing to file: " << encodingFilePath << "\n";

    ofstream encodingfile;
    encodingfile.open(encodingFilePath);

    int days = generationVariables[0];
    int hours = generationVariables[1];
    int classRooms = generationVariables[2];
    int courses = generationVariables[3];
    int courseHours = generationVariables[4];

    encodingfile << "*days: " << days << "\n";
    encodingfile << "*hours: " << hours << "\n";
    encodingfile << "*classRooms: " << classRooms << "\n";
    encodingfile << "*courses: " << courses << "\n";
    encodingfile << "*courseHours: " << courseHours << "\n";

    encodingfile << "p cnf " << vars << " " << clauseCount << "\n";
    encodingfile << "*mustHaveRoomClauses - Each course hour must have a room and a timeslot assigned \n";
    for (const auto &cl : mustHaveRoomClauses)
    {
        for (int lit : cl)
        {

            encodingfile << lit << " ";
        }
        encodingfile << "0\n";
    }

    encodingfile << "*atMostOneClauses - A course hour cannot occupy more than one room and timeslot \n";
    for (const auto &cl : atMostOneClauses)
    {
        for (int lit : cl)
        {
            encodingfile << lit << " ";
        }
        encodingfile << "0\n";
    }

    encodingfile << "*roomConflictClauses - Two different courses can't have hours assigned to same room and timeslot \n";
    for (const auto &cl : roomConflictClauses)
    {
        for (int lit : cl)
        {
            encodingfile << lit << " ";
        }
        encodingfile << "0\n";
    }

    encodingfile.close();
    if (verbose)
        cout << "Finished writing encoding to file: " << encodingFilePath << "\n";
}

string getFilePath()
{
    if (filePath == "")
    {
        namespace fs = std::filesystem;
        string directoryPath = "./encodings";
        fs::create_directories(directoryPath);

        auto timeStamp = std::chrono::system_clock::now();
        std::time_t timeStamp_time = std::chrono::system_clock::to_time_t(timeStamp);
        string timeStampString = std::ctime(&timeStamp_time);
        timeStampString = timeStampString.substr(4, timeStampString.size() - 9);
        timeStampString[3] = '-';
        timeStampString[6] = '-';
        timeStampString[15] = '-';
        timeStampString.erase(std::remove_if(timeStampString.begin(), timeStampString.end(), ::isspace), timeStampString.end());
        string encodingFilePath = directoryPath + "/" + timeStampString + "timetableEncoding.cnf";
        filePath = encodingFilePath;
    }
    return filePath;
}

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <tuple>
#include "distribution_encodings.h"
#include "../../utils/logging/logging.h"
#include "../../utils/input_parser/input_parser.h"
#include "../../utils/config/config.h"
#include "../../utils/file_writer/file_writer.h"
#include "../../../libs/ipamir/ipamir.h"
#include "student_sectioning.h"
#include "scheduling_semantics.h"

using namespace std;

void logClassAssignments(void *solver,
                         int classes,
                         vector<Class> classVec,
                         vector<vector<int>> t,
                         vector<vector<int>> r)
{
    cout << "Assignment:\n";
    for (int classIndex = 0; classIndex < classes; classIndex++)
    {
        Class classObj = classVec[classIndex];
        string classId = classObj.id;
        string timingWeeks = "",
               timingDays = "";
        int timingStart = 0,
            timingLength = 0;

        for (long unsigned int timingIndex = 0; timingIndex < classObj.timings.size(); timingIndex++)
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

        string roomId = "";
        if (classObj.rooms.size() == 0)
        {
            roomId = "[/]";
            verboseLog("roomLit: -");
        }
        for (long unsigned int roomIndex = 0; roomIndex < classObj.rooms.size(); roomIndex++)
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
}

void logStudentSectioningAssignments(void *solver, StudentSectioningData sectioningData)
{
    vector<vector<DecisionVar>> s = sectioningData.s;
    vector<ConfDecisionVar> conf = sectioningData.conf;
    for (ConfDecisionVar clusterConfigAssignment : conf)
    {
        if (0 < ipamir_val_lit(solver, clusterConfigAssignment.literal))
        {
            cout << "Cluster "
                 << clusterConfigAssignment.clusterId
                 << " assigned to config "
                 << clusterConfigAssignment.configId
                 << "\n";
        }
    }
    for (vector<DecisionVar> clusterAssignment : s)
    {
        vector<string> classIds;
        for (DecisionVar var : clusterAssignment)
        {
            if (0 < ipamir_val_lit(solver, var.literal))
            {
                classIds.push_back(var.classId);
            }
        }
        cout << "Cluster [ ";
        cout << clusterAssignment[0].cluster.students[0].id;
        for (size_t i = 1; i < clusterAssignment[0].cluster.students.size(); i++)
        {
            cout << ", " << clusterAssignment[0].cluster.students[i].id;
        }
        cout << " ] assigned to class ";
        if (classIds.size() == 0)
        {
            cout << "none? \n";
        }
        else
        {
            cout << classIds[0];
            for (size_t i = 1; i < classIds.size(); i++)
            {
                cout << ", " << classIds[i];
            }
            cout << "\n";
        }
    }
}

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

void requestInput(void *solver)
{
    while (true)
    {
        cout << "Insert a new clause or give an empty input to execute benchmark \n";
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
}

void penalizeSolution(void *solver, int iteration, vector<vector<int>> *t, vector<vector<int>> *r)
{
    int timingWeight = (iteration + 1) * optimization.multiplierMap[TIME];
    for (vector<int> classTimingLiterals : (*t))
    {
        for (int timingLiteral : classTimingLiterals)
        {
            if (0 < ipamir_val_lit(solver, timingLiteral))
            {
                ipamir_add_soft_lit(solver, timingLiteral, timingWeight);
                verboseLog("Penalizing timing lit " + to_string(timingLiteral) + " with weight " + to_string(timingWeight));
                break;
            }
        }
    }

    int roomWeight = (iteration + 1) * optimization.multiplierMap[ROOM];
    for (vector<int> classRoomLiterals : (*r))
    {
        for (int roomLiteral : classRoomLiterals)
        {
            if (0 < ipamir_val_lit(solver, roomLiteral))
            {
                ipamir_add_soft_lit(solver, roomLiteral, roomWeight);
                verboseLog("Penalizing room lit " + to_string(roomLiteral) + " with weight " + to_string(roomWeight));
                break;
            }
        }
    }
}

vector<tuple<string, uint16_t>> getOptimizationVariables()
{
    vector<tuple<string, uint16_t>> optimizationVars;
    for (const auto &[key, value] : optimization.multiplierMap)
    {
        if (key == TIME)
            optimizationVars.push_back({"time", value});
        else if (key == ROOM)
            optimizationVars.push_back({"room", value});
        else if (key == DISTRIBUTION)
            optimizationVars.push_back({"distribution", value});
        else if (key == STUDENT)
            optimizationVars.push_back({"student", value});
    }
    return optimizationVars;
}

tuple<vector<vector<int>>, vector<vector<int>>>
initializeAssignmentLiteral(uint32_t *literalCounter, map<string, Class> classMap)
{
    vector<vector<int>> t;
    vector<vector<int>> r;
    for (auto &[classId, classObj] : classMap)
    {
        // Initilaize t
        vector<int> timingLiterals;
        if (opts.verbose)
            cout << "[VERBOSE] Created timing assignment literals " << *literalCounter;
        for ([[maybe_unused]] Timing timing : classObj.timings)
        {
            timingLiterals.push_back(*literalCounter);
            (*literalCounter)++;
        }
        if (opts.verbose)
            cout << " - " << *literalCounter - 1 << " for course " << classId << "\n";
        t.push_back(timingLiterals);

        // Initilaize r
        vector<int> roomLiterals;
        if (opts.verbose)
            cout << "[VERBOSE] Created room assignment literals " << *literalCounter;
        for ([[maybe_unused]] Room room : classObj.rooms)
        {
            roomLiterals.push_back(*literalCounter);
            (*literalCounter)++;
        }
        if (opts.verbose)
            cout << " - " << *literalCounter - 1 << " for course " << classId << "\n";
        r.push_back(roomLiterals);
    }
    return {t, r};
}

Result runBenchMarkIteration(int *i,
                             void *solver,
                             int classes,
                             vector<vector<int>> *t,
                             vector<vector<int>> *r,
                             vector<Class> *classVec,
                             StudentSectioningData *sectioningData)
{
    cout << "Running iteration " << *i << endl;
    // TODO: Write encoding to file based on generate-variable
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
    int code = ipamir_solve(solver);
    std::chrono::steady_clock::time_point endTime = std::chrono::steady_clock::now();
    cout << "Solved!" << endl;
    long long timeDiff = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
    cout << "Finished iteration " << *i << endl;
    cout << "Solving took:" << timeDiff / 1000.0 << "[ms], " << timeDiff / 1000000.0 << "[s] \n";
    cout << "Code returned by ipamir: " << code << "\n";
    Result result;
    result.solveTimeMs = timeDiff / 1000.0;
    if (code == 30)
    {
        logClassAssignments(solver, classes, *classVec, *t, *r);
        logStudentSectioningAssignments(solver, *sectioningData);
        uint64_t penalty = ipamir_val_obj(solver);
        result.satisfied = true;
        result.penalty = penalty;
        cout << "Penalty incurred by the solution: " << penalty << "\n";
    }
    else
    {
        result.satisfied = false;
        result.penalty = 0;
    }
    penalizeSolution(solver, *i, t, r);
    return result;
}

vector<Result> runBenchMarkInstance(vector<int> generationVariables)
{

    int weeks = generationVariables[0];
    int days = generationVariables[1];
    int hours = generationVariables[2];
    int rooms = generationVariables[3];

    void *solver = ipamir_init();

    map<string, Class> classMap = getClasses();
    map<string, int> classIndexMap;

    int periods = weeks * days * hours;
    int classes = classMap.size();

    vector<Class> classVec;
    int index = 0;
    for (auto &pair : classMap)
    {
        classVec.push_back(pair.second);
        classIndexMap[pair.second.id] = index;
        index++;
    }

    uint32_t literalCounter = 1;

    // t represents class period assignments, r represents class room assignments
    auto [t, r] = initializeAssignmentLiteral(&literalCounter, classMap);

    verboseLog("Creating clauses for " +
               to_string(classes) + " classes with " +
               to_string(periods) + " time periods and " +
               to_string(rooms) + " rooms.");

    // Map of distributions, used for generating distribution encodings
    map<string, vector<DistributionVariant>> distributionsMap = getDistributions();

    logTimingLiterals(&t);
    logRoomLiterals(&r);

    cout << "Running encodeStudentSectioning" << endl;
    StudentSectioningData sectioningData = encodeStudentSectioning(solver, literalCounter, weeks, days, t, classMap, classIndexMap);
    cout << "Finished encodeStudentSectioning" << endl;

    encodeConstraints(solver,
                      literalCounter,
                      weeks,
                      days,
                      hours,
                      classes,
                      &t,
                      &r,
                      &classVec,
                      &classMap,
                      &classIndexMap,
                      &distributionsMap);

    // Print answer and ask user for input
    vector<Result> results;
    if (opts.manual_input)
        requestInput(solver);
    for (int i = 0; i < opts.iterations; i++)
    {
        results.push_back(runBenchMarkIteration(&i, solver, classes, &t, &r, &classVec, &sectioningData));
    }
    cout << "Releasing ipamir...\n";
    ipamir_release(solver);
    return results;
}

vector<IterationResult> runOptimizationSwapBenchMark()
{
    vector<IterationResult> iterationResults;
    vector<int> configVariables = getConfigVariables();
    vector<int> optimizationVars = {1, 10};
    for (int timeMultiplier : optimizationVars)
    {
        optimization.multiplierMap[TIME] = timeMultiplier;
        for (int roomMultiplier : optimizationVars)
        {
            optimization.multiplierMap[ROOM] = roomMultiplier;
            for (int distributionMultiplier : optimizationVars)
            {
                optimization.multiplierMap[DISTRIBUTION] = distributionMultiplier;
                for (int studentMultiplier : optimizationVars)
                {
                    optimization.multiplierMap[STUDENT] = studentMultiplier;
                    verboseLog("Running a benchmarking instance with optimization { time: " +
                               to_string(optimization.multiplierMap[TIME]) + ", room: " +
                               to_string(optimization.multiplierMap[ROOM]) + ", distribution: " +
                               to_string(optimization.multiplierMap[DISTRIBUTION]) + ", student: " +
                               to_string(optimization.multiplierMap[STUDENT]) + "}");

                    vector<Result> results = runBenchMarkInstance(configVariables);
                    IterationResult iterationResult;
                    iterationResult.optimization = getOptimizationVariables();
                    iterationResult.results = results;
                    iterationResults.push_back(iterationResult);
                }
            }
        }
    }
    return iterationResults;
}

void runAssumptionBenchMark()
{
    vector<int> configVariables = getConfigVariables();
}

void runBenchMark()
{
    if (opts.testType == 1)
    {
        vector<int> configVariables = getConfigVariables();
        vector<Result> results = runBenchMarkInstance(configVariables);
        IterationResult iterationResults;
        iterationResults.optimization = getOptimizationVariables();
        iterationResults.results = results;
        writeResultsToFile(iterationResults);
    }
    else if (opts.testType == 2)
    {
        vector<IterationResult> results = runOptimizationSwapBenchMark();
        writeMultipleIterationResultsToFile(results);
    }
    else if (opts.testType == 3)
    {
        runAssumptionBenchMark();
    }
}
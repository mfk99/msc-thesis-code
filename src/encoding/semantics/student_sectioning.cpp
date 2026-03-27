#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <chrono>
#include <algorithm>
#include <unordered_map>
#include "distribution_encodings.h"
#include "../../utils/logging/logging.h"
#include "../../utils/input_parser/input_parser.h"
#include "../../utils/config/config.h"
#include "../../../libs/ipamir/ipamir.h"
#include "../am1/am1_encoder.h"
#include "../ipamir_utils/ipamir_clause_collector.h"

using namespace std;

vector<StudentCluster> createStudentClusters(vector<Student> students, vector<HierarchyCourse> hierarchyVec, uint32_t literalCounter)
{
    vector<StudentCluster> studentClusters;
    unordered_map<string, StudentCluster> clusterMap;
    int idCounter = 0;
    for (Student student : students)
    {
        vector<string> classes = student.classIds;
        sort(classes.begin(), classes.end());
        string key;
        for (string c : classes)
            key += c + "|";
        clusterMap[key].students.push_back(student);
    }
    for (auto &[key, cluster] : clusterMap)
    {
        cluster.id = idCounter++;
        studentClusters.push_back(cluster);
        if (opts.verbose)
        {
            cout << "[opts.verbose] Cluster id " << idCounter - 1 << " contains [ ";
            for (Student student : cluster.students)
            {
                cout << student.id << ", ";
            }
            cout << "]\n";
        }
    }
    return studentClusters;
}

vector<vector<DecisionVar>> generateDecisionVariables(vector<StudentCluster> &studentClusters,
                                                      vector<HierarchyCourse> &hierarchyVec,
                                                      uint32_t &literalCounter)
{
    vector<vector<DecisionVar>> s = {};
    for (StudentCluster &cluster : studentClusters)
    {
        vector<DecisionVar> classVars = {};
        for (HierarchyCourse &course : hierarchyVec)
        {
            for (HierarchyConfiguration &config : course.configs)
            {
                string configId = config.id;
                for (HierarchySubpart &subpart : config.subparts)
                {
                    for (HierarchyClass &hClass : subpart.classes)
                    {
                        DecisionVar var;
                        var.literal = literalCounter++;
                        var.configId = configId;
                        var.cluster = cluster;
                        var.classId = hClass.id;
                        verboseLog("Created literal " + to_string(literalCounter - 1) + " to represent " + var.classId + " assignment");
                        classVars.push_back(var);
                    }
                }
            }
        }
        s.push_back(classVars);
    }
    return s;
}

unordered_map<string, vector<DecisionVar>> createClassLookupMap(
    vector<vector<DecisionVar>> &s)
{
    unordered_map<string, vector<DecisionVar>> classToDecisionVars;

    for (vector<DecisionVar> clusterVars : s)
    {
        for (DecisionVar var : clusterVars)
        {
            classToDecisionVars[var.classId].push_back(var);
        }
    }

    return classToDecisionVars;
}

vector<ConfDecisionVar> generateAuxiliaryConfigVariables(vector<StudentCluster> &studentClusters,
                                                         vector<HierarchyCourse> &hierarchyVec,
                                                         uint32_t &literalCounter)
{
    vector<ConfDecisionVar> conf;
    for (StudentCluster &cluster : studentClusters)
    {
        for (HierarchyCourse &course : hierarchyVec)
        {
            for (HierarchyConfiguration &config : course.configs)
            {
                ConfDecisionVar confVar;
                confVar.clusterId = cluster.id;
                confVar.configId = config.id;
                confVar.literal = literalCounter++;
                conf.push_back(confVar);
                verboseLog("Created literal " + to_string(literalCounter - 1) + " to represent cluster " + to_string(cluster.id) + " config " + config.id + " assignment");
            }
        }
    }
    return conf;
}

unordered_map<int, vector<ConfDecisionVar>> generateConfMap(vector<ConfDecisionVar> conf)
{
    unordered_map<int, vector<ConfDecisionVar>> confMap;
    for (ConfDecisionVar var : conf)
    {
        confMap[var.clusterId].push_back(var);
    }
    return confMap;
}

unordered_map<string, vector<int>> createConfigLookupMap(
    const vector<vector<int>> &conf,
    const vector<HierarchyCourse> &hierarchyVec)
{
    unordered_map<string, vector<int>> configToLiteralsMap;

    size_t confIdx = 0;
    for (const auto &course : hierarchyVec)
    {
        for (const auto &config : course.configs)
        {
            string configId = config.id;
            if (confIdx < conf.size())
            {
                configToLiteralsMap[configId] = conf[confIdx];
            }
            ++confIdx;
        }
    }
    return configToLiteralsMap;
}

void encodeExactlyOneConfig(void *solver,
                            vector<StudentCluster> studentClusters,
                            unordered_map<int, vector<ConfDecisionVar>> confLiteralMap,
                            uint32_t literalCounter)
{

    for (StudentCluster cluster : studentClusters)
    {
        vector<ConfDecisionVar> confVars = confLiteralMap[cluster.id];
        if (opts.verbose)
            cout << "[opts.verbose] Encoding at-least-1 config constraint with literals [";

        for (ConfDecisionVar var : confVars)
        {
            int lit = var.literal;
            if (opts.verbose)
                cout << lit << ",";
            ipamir_add_hard(solver, lit);
        }
        ipamir_add_hard(solver, 0);
        if (opts.verbose)
            cout << "0] \n";

        if (opts.verbose)
            cout << "[opts.verbose] Adding am1 config constraints for literals [";
        AM1Encoder am1Encoder = AM1Encoder("pairwise");
        for (ConfDecisionVar var : confVars)
        {
            int lit = var.literal;
            if (opts.verbose)
                cout << lit << ",";
            am1Encoder.am1encoder_add(lit);
        }
        am1Encoder.am1encoder_encode(&literalCounter, ipamirClauseCollector, solver);
        if (opts.verbose)
            cout << "0] \n";
        am1Encoder.am1encoder_drop();
    }
}

void encodeClassCapacityConstraints(void *solver,
                                    vector<HierarchyCourse> &hierarchyVec,
                                    vector<StudentCluster> studentClusters,
                                    vector<vector<DecisionVar>> s,
                                    uint32_t literalCounter)
{

    for (HierarchyCourse &course : hierarchyVec)
    {
        for (HierarchyConfiguration &config : course.configs)
        {
            for (HierarchySubpart &subpart : config.subparts)
            {
                for (HierarchyClass &hClass : subpart.classes)
                {
                    string classId = hClass.id;
                    vector<DecisionVar> classVars;
                    for (vector<DecisionVar> clusterVars : s)
                    {
                        // Find clusters with class
                        for (DecisionVar var : clusterVars)
                        {
                            if (var.classId == classId)
                            {
                                classVars.push_back(var);
                                break;
                            }
                        }
                    }

                    if (classVars.empty())
                        continue;

                    GeneralizedTotalizer *genTot = gte_new();
                    for (DecisionVar var : classVars)
                    {
                        int literal = var.literal;
                        int size = var.cluster.students.size();
                        gte_add(genTot, literal, size);
                        verboseLog("Adding literal: " + to_string(literal) + " with weight: " + to_string(size) + " to classId: " + classId + " gentot");
                    }
                    uint32_t n_vars_used = literalCounter - 1;
                    gte_reserve(genTot, &n_vars_used);
                    gte_encode_ub(genTot, hClass.limit, hClass.limit, &n_vars_used, ipamirClauseCollector, solver);
                    gte_enforce_ub(genTot, hClass.limit, ipamirGteClauseCollector, solver);
                    gte_drop(genTot);
                    literalCounter = n_vars_used;
                }
            }
        }
    }
}

void encodeClassParentConstraints(void *solver,
                                  vector<HierarchyCourse> &hierarchyVec,
                                  vector<vector<DecisionVar>> s)
{
    for (HierarchyCourse &course : hierarchyVec)
    {
        for (HierarchyConfiguration &config : course.configs)
        {
            for (HierarchySubpart &subpart : config.subparts)
            {
                for (HierarchyClass &hClass : subpart.classes)
                {
                    if (hClass.parentId.empty())
                        continue;

                    for (vector<DecisionVar> clusterClasses : s)
                    {
                        int classLiteral = 0;
                        int parentLiteral = 0;
                        for (DecisionVar var : clusterClasses)
                        {
                            if (var.classId == hClass.id)
                            {
                                classLiteral = var.literal;
                            }
                            if (var.classId == hClass.parentId)
                            {
                                parentLiteral = var.literal;
                            }
                        }
                        if (classLiteral != 0 && parentLiteral != 0)
                        {
                            ipamir_add_hard(solver, -classLiteral);
                            ipamir_add_hard(solver, parentLiteral);
                            ipamir_add_hard(solver, 0);
                            verboseLog("Added parent constraint [" + to_string(-classLiteral) + ", " + to_string(parentLiteral) + ", 0 ] ");
                        }
                    }
                }
            }
        }
    }
}

void encodeExactlyOneClassPerSubpartConstraint(void *solver,
                                               vector<HierarchyCourse> &hierarchyVec,
                                               vector<StudentCluster> studentClusters,
                                               vector<vector<DecisionVar>> s,
                                               unordered_map<string, vector<DecisionVar>> classToDecisionVars,
                                               unordered_map<int, vector<ConfDecisionVar>> confLiteralMap)
{

    for (HierarchyCourse &course : hierarchyVec)
    {
        for (HierarchyConfiguration &config : course.configs)
        {
            string configId = config.id;
            for (HierarchySubpart &subpart : config.subparts)
            {
                vector<string> subpartClasses;
                for (HierarchyClass &hClass : subpart.classes)
                {
                    subpartClasses.push_back(hClass.id);
                }

                for (StudentCluster cluster : studentClusters)
                {
                    int clusterId = cluster.id;
                    vector<ConfDecisionVar> confDecisionVarVec = confLiteralMap[clusterId];
                    int configLit = 0;
                    for (ConfDecisionVar decisionVar : confDecisionVarVec)
                    {
                        if (decisionVar.configId == configId)
                        {
                            configLit = decisionVar.literal;
                            break;
                        }
                    }
                    if (configLit == 0)
                        continue;

                    // At-most-one encoding
                    for (size_t class1Index = 0; class1Index < subpartClasses.size(); class1Index++)
                    {
                        for (size_t class2Index = class1Index + 1; class2Index < subpartClasses.size(); class2Index++)
                        {
                            string class1Id = subpartClasses[class1Index];
                            string class2Id = subpartClasses[class2Index];
                            vector<DecisionVar> cluster1Classes = classToDecisionVars[class1Id];
                            vector<DecisionVar> cluster2Classes = classToDecisionVars[class2Id];
                            int lit1 = 0;
                            int lit2 = 0;

                            for (DecisionVar var : cluster1Classes)
                            {
                                if (var.cluster.id == cluster.id)
                                {
                                    lit1 = var.literal;
                                    break;
                                }
                            }

                            for (DecisionVar var : cluster2Classes)
                            {
                                if (var.cluster.id == cluster.id)
                                {
                                    lit2 = var.literal;
                                    break;
                                }
                            }

                            if (lit1 != 0 && lit2 != 0)
                            {
                                ipamir_add_hard(solver, -configLit);
                                ipamir_add_hard(solver, -lit1);
                                ipamir_add_hard(solver, -lit2);
                                ipamir_add_hard(solver, 0);
                                verboseLog("Added at-most-one class assignment per config constraint: [ " +
                                           to_string(-configLit) + ", " +
                                           to_string(-lit1) + ", " +
                                           to_string(-lit2) + ", 0 ]");
                            }
                        }
                    }

                    // At-least-one encoding
                    vector<int> literals;
                    for (string classId : subpartClasses)
                    {
                        vector<DecisionVar> clusterClasses = classToDecisionVars[classId];
                        for (DecisionVar var : clusterClasses)
                        {
                            if (var.cluster.id == cluster.id)
                                literals.push_back(var.literal);
                        }
                    }
                    ipamir_add_hard(solver, -configLit);
                    if (opts.verbose)
                        cout << "[opts.verbose] Added at-least-one class assignment per config constraint: [ " << -configLit;
                    for (int lit : literals)
                    {
                        ipamir_add_hard(solver, lit);
                        if (opts.verbose)
                            cout << ", " << lit;
                    }
                    ipamir_add_hard(solver, 0);
                    if (opts.verbose)
                        cout << ", 0 ] \n";
                }
            }
        }
    }
}

void encodeConflictingSchedules(void *solver,
                                uint32_t literalCounter,
                                int weeks,
                                int days,
                                vector<vector<int>> t,
                                map<string, Class> classMap,
                                map<string, int> classIndexMap,
                                vector<HierarchyCourse> &hierarchyVec,
                                vector<StudentCluster> studentClusters,
                                vector<vector<DecisionVar>> s,
                                unordered_map<string, vector<DecisionVar>> classToDecisionVars,
                                unordered_map<int, vector<ConfDecisionVar>> confLiteralMap)
{
    // TODO: retrieve these dynamically
    bool required = true;
    int penalty = 0;

    for (vector<DecisionVar> clusterClasses : s)
    {
        for (size_t class1Index = 0; class1Index < clusterClasses.size(); class1Index++)
        {
            DecisionVar classDecisionVar1 = clusterClasses[class1Index];
            string class1Id = classDecisionVar1.classId;
            int class1DecisionLiteral = classDecisionVar1.literal;
            Class class1 = classMap[class1Id];
            int class1LiteralIndex = classIndexMap[class1.id];

            for (size_t class2Index = class1Index + 1; class2Index < clusterClasses.size(); class2Index++)
            {
                DecisionVar classDecisionVar2 = clusterClasses[class2Index];
                string class2Id = classDecisionVar2.classId;
                int class2DecisionLiteral = classDecisionVar2.literal;
                if (class1Id == class2Id)
                    continue;
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
                                            verboseLog("Adding conflicting schedule constraint: -" + to_string(periodLit1) +
                                                       ", -" + to_string(periodLit2) +
                                                       ", -" + to_string(class1DecisionLiteral) +
                                                       ", -" + to_string(class2DecisionLiteral) + ", 0");
                                            ipamirAddClause(solver,
                                                            {-periodLit1, -periodLit2, -class1DecisionLiteral, -class2DecisionLiteral},
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

StudentSectioningData encodeStudentSectioning(void *solver,
                                              uint32_t literalCounter,
                                              int weeks,
                                              int days,
                                              vector<vector<int>> t,
                                              map<string, Class> classMap,
                                              map<string, int> classIndexMap)
{

    vector<Student> students = getStudents();
    vector<HierarchyCourse> hierarchyVec = getClassHierarchy();
    vector<StudentCluster> studentClusters = createStudentClusters(students, hierarchyVec, literalCounter);

    // Cluster class assignment vector
    vector<vector<DecisionVar>> s = generateDecisionVariables(studentClusters, hierarchyVec, literalCounter);
    unordered_map<string, vector<DecisionVar>> classToDecisionVars = createClassLookupMap(s);

    // Cluster config assignment vector
    vector<ConfDecisionVar> conf = generateAuxiliaryConfigVariables(studentClusters, hierarchyVec, literalCounter);
    unordered_map<int, vector<ConfDecisionVar>> confLiteralMap = generateConfMap(conf);

    encodeExactlyOneConfig(solver, studentClusters, confLiteralMap, literalCounter);
    encodeClassCapacityConstraints(solver, hierarchyVec, studentClusters, s, literalCounter);
    encodeClassParentConstraints(solver, hierarchyVec, s);
    encodeExactlyOneClassPerSubpartConstraint(solver, hierarchyVec, studentClusters, s, classToDecisionVars, confLiteralMap);
    encodeConflictingSchedules(solver, literalCounter, weeks, days, t, classMap, classIndexMap, hierarchyVec, studentClusters, s, classToDecisionVars, confLiteralMap);

    StudentSectioningData data;
    data.s = s;
    data.conf = conf;

    return data;
}
#include <vector>
#include "../config/config.h"

using namespace std;

vector<vector<DecisionVar>> encodeStudentSectioning(void *solver,
                                                    uint32_t literalCounter,
                                                    int weeks,
                                                    int days,
                                                    vector<vector<int>> t,
                                                    map<string, Class> classMap,
                                                    map<string, int> classIndexMap);
#include <cstdint>
#include <vector>
#include <string>
#include "../../utils/config/config.h"

using namespace std;

void encodeConstraints(void *solver,
                       uint32_t literalCounter,
                       int weeks,
                       int days,
                       int hours,
                       int classes,
                       vector<vector<int>> *t,
                       vector<vector<int>> *r,
                       vector<Class> *classVec,
                       map<string, Class> *classMap,
                       map<string, int> *classIndexMap,
                       map<string, vector<DistributionVariant>> *distributionsMap);
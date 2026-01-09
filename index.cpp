#include <iostream>
#include "semantics/scheduling-semantics.h"
#include "input-parser/input-parser.h"
#include "scheduling-encoding-generator/scheduling-encoding-generator.h"
using namespace std;

int main(int argc, char **argv)
{
    parseInput(argc, argv);

    if (generate)
    {
        cout << "Generating... \n";
        vector<int> generationParameters = parseEncodingGenerationVariables(argc, argv);
        generateEncoding(generationParameters);
    }
    cout << filePath;
    if (execute)
    {
        cout << "Executing... \n";
        runBenchMark(filePath);
    }

    return 0;
}

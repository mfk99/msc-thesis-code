#include <iostream>
#include "semantics/scheduling-semantics.h"
#include "settings/settings.h"
using namespace std;

int main()
{
    while (true)
    {
        int x;
        cout << "Please insert command: \n"
             << "(1) Enter scheduling generator\n"
             << "(2) Enter ipamir scheduling generator\n"
             << "(3) Modify settings\n"
             << "(0) Exit\n";
        cin >> x; // Get user input from the keyboard
        cout << "Your number is: " << x << "\n";
        if (x == 0)
        {
            break;
        }
        else if (x == 1)
        {
            enterSchedulingGenerator();
        }
        else if (x == 2)
        {
            enterIpamirSchedulingGenerator();
        }
        else if (x == 3)
        {
            enterSettings();
        }
        else
        {
            cout << "Invalid option. Please try again.\n";
        }
    }
    cout << "Goodbye!" << "\n";
    return 0;
}

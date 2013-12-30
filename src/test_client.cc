#include "sls.h"
#include <iostream>

using namespace std;

int main(int argc, char *argv[]){
    cerr << "Test Client Started... " << endl;
    string test_key = "key";
    string test_val = "value";

    append(test_key.c_str(), test_val);
    append(test_key.c_str(), "1");
    append(test_key.c_str(), "2");

    list<string> *r = all(test_key.c_str());
    cerr << "Got: " << r->size() << endl;
    return 0;
}

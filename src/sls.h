#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<list>

using namespace std;

bool append(const char *key, string data);
list<string> *lastn(const char *key, int num_entries);
list<string> *all(const char *key);
list<string> *intervaln(const char *key, unsigned long long start, unsigned long long end);
list<string> *intervalt(const char *key, unsigned long long start, unsigned long long end);
string unwrap(const string value);
unsigned long long check_time(const string value);

#endif

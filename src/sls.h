#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<deque>
#include<mutex>
#include<stack>
#include<memory>
#include<chrono>
#include<smpl.h>

#include"sls.pb.h"

namespace sls{

    /* The address of the sls Server the following functions will use
     */
    extern std::shared_ptr<smpl::Remote_Address> global_server;

    /* Append data to the list indicated by key at some time (in milliseconds)
     * between the time this function is called and the time it returns.
     *
     * key: Any std::string such that key.empty() = false
     * data: Any string
     */
    bool append(const std::string &key, const std::string &data);

    /* Append data to the list indicated by key at time
     *
     * key: Any std::string such that key.empty() = false
     * time: Milliseconds since epoch such that (time >= any previous time)
     * data: Any string
     */
    bool append(const std::string &key, const std::chrono::milliseconds &time, const std::string &data);

    /* Returns a deque of at most the N most recent entries in the list for key
     * in chronological order.  If the list for key contains less than
     * num_entries, the entire list is returned. If the list for key is empty,
     * or num_entries is 0, a deque of size 0 is returned.
     *
     * key: Any std::string such that key.empty() = false
     * num_entries: >= 0
     */
    std::shared_ptr< std::deque<std::pair<std::chrono::milliseconds, std::string>> > lastn(const std::string &key, const unsigned long long &num_entries);

    /* Returns a deque contains all the entries in the list for key in
     * chronological order.  If the list is empty or no list exists, a deque
     * of size 0 is returned.
     *
     * key: Any std::string such that key.empty() = false
     */
    std::shared_ptr< std::deque<std::pair<std::chrono::milliseconds, std::string>> > all(const std::string &key);

    /* Returns a deque of all the values in the list for key between time
     * index start and end, inclusive.  start and end must be milliseconds
     * since epoch.
     *
     * key: Any std::string such that key.empty() = false
     * start: 0 <= end
     * end: 0 >= start
     */
    std::shared_ptr< std::deque<std::pair<std::chrono::milliseconds, std::string>> > intervalt(const std::string &key, const unsigned long long &start, const unsigned long long &end);

    /* Given an std::pair<std::chrono::milliseconds, std::string> value, return the data component without its
     * timestamp
     */
    std::string unwrap(const std::pair<std::chrono::milliseconds, std::string> &value);

    /* Given an std::pair<std::chrono::milliseconds, std::string> value, return the timestamp component without its
     * data
     */
    unsigned long long check_time(const std::pair<std::chrono::milliseconds, std::string> &value);

}

#endif

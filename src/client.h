#ifndef __SLS_CLIENT_H__
#define __SLS_CLIENT_H__

#include <smpl.h>
#include <vector>
#include <memory>

#include "sls.h"
#include "sls.pb.h"

namespace sls{

    extern std::shared_ptr<smpl::Remote_Address> global_server;

    class Client{
        private:
            std::unique_ptr<smpl::Channel> server_connection;
            std::pair<sls::Response, std::vector<std::pair<uint64_t, std::string>>> _request(const sls::Request &request);
            std::shared_ptr< std::deque<sls::Value> > _interval(const std::string &key, const unsigned long long &start, const unsigned long long &end, const bool &is_time);

        public:

            // IMPORTANT
            // All of the following transactions occur with respect to the set
            // of lists stored on the Server as indicated by server_connection

            /* Create a client connected to the server accepting connections
             * smpl::Remote_Address server .
             */
            Client(std::shared_ptr<smpl::Remote_Address> server);

            /* Append data to the list indicated by key at some time (in milliseconds)
            * between the time this function is called and the time it returns.
            *
            * key: Any std::string such that key.empty() = false
            * data: Any string
            */
            bool append(const std::string &key, const std::string &data);

            /* Returns a deque of at most the N most recent entries in the list for key
            * in chronological order.  If the list for key contains less than
            * num_entries, the entire list is returned. If the list for key is empty,
            * or num_entries is 0, a deque of size 0 is returned.
            *
            * key: Any std::string such that key.empty() = false
            * num_entries: >= 0
            */
            std::shared_ptr< std::deque<sls::Value> > lastn(const std::string &key, const unsigned long long &num_entries);

            /* Returns a deque contains all the entries in the list for key in
            * chronological order.  If the list is empty or no list exists, a deque
            * of size 0 is returned.
            *
            * key: Any std::string such that key.empty() = false
            */
            std::shared_ptr< std::deque<sls::Value> > all(const std::string &key);

            /* Returns a deque of all the values in the list for key between time
            * index start and end, inclusive.  start and end must be milliseconds
            * since epoch.
            *
            * key: Any std::string such that key.empty() = false
            * start: 0 <= end
            * end: 0 >= start
            */
            std::shared_ptr< std::deque<sls::Value> > intervalt(const std::string &key, const unsigned long long &start, const unsigned long long &end);
    };
}

#endif

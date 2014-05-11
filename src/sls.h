#ifndef __SLS_H__
#define __SLS_H__ 1

#include<string>
#include<list>
#include<mutex>
#include<stack>

#include"sls.pb.h"

namespace sls{
    class SLS_Error{
        public:
            std::string msg;
            SLS_Error(std::string message);
    };
    class Server{
        public:
            Server();
            ~Server();
            void handle(int sockfd);
            void handle_next_request();
            void sync();
        private:
            std::map<std::string, std::list<sls::Value> > cache;
            std::map<std::string, std::mutex> locks;
            std::mutex incoming_lock;
            std::stack<int> incoming;
            sls::Value wrap(std::string payload);
            void _page_out(std::string key, unsigned int skip);
            void _file_lookup(std::string key, std::string filename, sls::Archive *archive);
            void file_lookup(std::string key, std::string filename, std::list<sls::Value> *r);
            std::string next_lookup(std::string key, std::string filename);
            unsigned long long pick_time(const std::list<sls::Value> &d, unsigned long long start, unsigned long long end, std::list<sls::Value> *result);
            unsigned long long pick(const std::list<sls::Value> &d, unsigned long long current, unsigned long long start, unsigned long long end, std::list<sls::Value> *result);
            void _lookup(int client_sock, sls::Request *request);
    };
    void set_local_sls(bool new_val);
    bool append(const char *key, std::string data);
    std::list<sls::Value> *lastn(const char *key, unsigned long long num_entries);
    std::list<sls::Value> *all(const char *key);
    std::list<sls::Value> *intervalt(const char *key, unsigned long long start, unsigned long long end);
    std::string unwrap(sls::Value value);
    unsigned long long check_time(sls::Value value);
}

#endif

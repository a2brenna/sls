#include "config.h"
#include <limits.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <map>
#include <list>
#include <mutex>
#include <pthread.h>
#include <sys/time.h>
#include <dirent.h>
#include <algorithm>
#include <hgutil.h>
#include <hgutil/raii.h>
#include <hgutil/debug.h>
#include <cstdlib>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sls.h"
#include "sls.pb.h"
#include <memory>
#include <netdb.h>

using namespace std;

map<string, list<sls::Value> > cache;
map<string, mutex> locks;
int inet_sock; //network socket using tcp
int unix_sock; //unix domain socket using udp

sls::Value wrap(string payload){
    sls::Value r;
    r.set_time(milli_time());
    r.set_data(payload);
    return r;
}

string get_canonical_filename(string path){
    char head[256];
    bzero(head, 256);

    if( readlink(path.c_str(), head, 256) < 0 ){
        DEBUG "Could not get current head file" << endl;
        return string("");
    }

    return string(head);
}

void _page_out(string key, unsigned int skip){
    ERROR "Attempting to page out: " << key << endl;
    list<sls::Value>::iterator i = (cache[key]).begin();
    unsigned int j = 0;
    unsigned int size = cache[key].size();
    for(; (j < size) && (j < skip); ++j, ++i);
    list<sls::Value>::iterator new_end = i;

    //pack into archive
    unique_ptr<sls::Archive> archive(new sls::Archive);
    for(; i != cache[key].end(); ++i){
        sls::Value *v = archive->add_values();
        //maybe can eliminate this copy and use a pointer instead?
        v->CopyFrom(*i);
    }

    string head_link = disk_dir;
    head_link.append(key);
    head_link.append("/head");
    archive->set_next_archive(get_canonical_filename(head_link));

    unique_ptr<string> outfile(new string);
    archive->SerializeToString(outfile.get());

    //write new file
    string directory = disk_dir;
    directory.append(key);
    directory.append("/");

    mkdir(directory.c_str(), 0755);
    string new_file_name = RandomString(32);
    string new_file = directory + "/" + new_file_name;
    if (writefile(new_file, *(outfile.get()))){

    //set symlink from directory/head -> new_file_name
        remove(head_link.c_str());
        symlink(new_file_name.c_str(), head_link.c_str());

        //delete from new_end to end()
        cache[key].erase(new_end, cache[key].end());
    }
    else{
        ERROR "Failed to page out: " << key << endl;
    }
}

void shutdown(int signo){
    DEBUG "Attempting shutdown" << endl;
    if(signo == SIGSEGV){
        close(inet_sock);
        exit(1);
    }
    close(inet_sock);

    for(auto& cached: cache){
        //we want to "leak" this lock so no more data can be appended... but we don't want to deadlock here, unable to page out to disk because we're stuck in the middle of an append operation
        locks[cached.first].try_lock();
        _page_out(cached.first, 0);
    }

    exit(0);
}

void _file_lookup(string key, string filename, sls::Archive *archive){
    if(filename == ""){
        return;
    }
    string filepath = (disk_dir + key + "/" + filename);
    unique_ptr<string> s(new string);
    readfile(filepath, s.get());
    try{
        archive->ParseFromString(*(s.get()));
    }
    catch(...){
        ERROR "Could not read from: " << filepath << endl;
    }
    return;
}

void file_lookup(string key, string filename, list<sls::Value> *r){
    DEBUG "Performing file lookup: " << filename << endl;
    unique_ptr<sls::Archive> archive(new sls::Archive);
    _file_lookup(key, filename, archive.get());

    r->clear();

    if(archive != NULL){
        for(int i = 0; i < archive->values_size(); i++){
            r->push_back(archive->values(i));
        }
    }
    DEBUG "Got: " << r->size() << " values" << endl;
    return;
}

string next_lookup(string key, string filename){
    unique_ptr<sls::Archive> archive(new sls::Archive);
    _file_lookup(key, filename, archive.get());
    string next_archive;

    if(archive != NULL){
        if(archive->has_next_archive()){
            next_archive = archive->next_archive();
        }
    }
    return next_archive;
}

unsigned long long pick_time(const list<sls::Value> &d, unsigned long long start, unsigned long long end, list<sls::Value> *result){
    DEBUG "In pick_time()" << endl;
    DEBUG "Picking from: " << d.size() << endl;
    unsigned long long earliest = ULLONG_MAX;
    for(auto &value: d){
        if( (value.time() > start) && (value.time() < end) ){
            result->push_front(value);
        }
        earliest = min(earliest, (unsigned long long)value.time());
    }
    return earliest;
}

unsigned long long pick(const list<sls::Value> &d, unsigned long long current, unsigned long long start, unsigned long long end, list<sls::Value> *result){
    DEBUG "In pick()" << endl;
    DEBUG "Picking from: " << d.size() << endl;
    for (auto &value: d){
        if( (current >= start) && (current <= end) ){
            result->push_back(value);
        }
        current++;
    }
    return current;
}

void _lookup(int client_sock, sls::Request *request){
    DEBUG "Performing lookup..." << endl;
    unique_ptr<sls::Response> response(new sls::Response);
    response->set_success(false);

    if(request->mutable_req_range()->IsInitialized()){
        unique_ptr<list<sls::Value> > values(new list<sls::Value>);
        string key = request->key();
        unsigned long long start = request->mutable_req_range()->start();
        unsigned long long end = request->mutable_req_range()->end();
        string next_file;
        unique_ptr<list<sls::Value> > d(new list<sls::Value>);
        {
            lock_guard<mutex> guard(locks[key]);
            *(d.get()) = cache[key];
            next_file = get_canonical_filename(disk_dir + key + string("/head"));
        }

        DEBUG "Got: " << d->size() << " from cache..." << endl;

        unsigned long long current = 0;

        bool has_next = true;
        do{
            if( request->mutable_req_range()->is_time() ){
                DEBUG "Performing time based lookup..." << endl;
                auto earliest_seen = pick_time(*(d.get()), start, end, values.get());
                DEBUG "Earliest seen: " << earliest_seen << endl;
                if( earliest_seen < start ){
                    DEBUG "Earliest data seen: " << earliest_seen << endl;
                    break;
                }
            }
            else{
                DEBUG "Performing interval based lookup..." << endl;
                current = pick(*(d.get()), current, start, end, values.get());
                if(current >= end ){
                    DEBUG "Current data index: " << current << endl;
                    break;
                }
            }
            do{
                DEBUG "Looking in file: " << next_file << endl;
                file_lookup(key, next_file, d.get());
                DEBUG next_file << "has: " << d->size() << endl;
                if(next_file == ""){
                    has_next = false;
                    break;
                }
                next_file = next_lookup(key, next_file);
            }
            while(d->size() == 0);

        }while(has_next);

        for(sls::Value &v: *values){
            sls::Data *datum = response->add_data();
            string s;
            v.SerializeToString(&s);
            datum->set_data(s);
        }
        DEBUG "Total fetched: " << response->data_size() << endl;
        response->set_success(true);
    }
    else{
        ERROR "Range not initialized" << endl;
    }

    unique_ptr<string> r(new string);
    response->SerializeToString(r.get());
    if(!send_string(client_sock, *r)){
        ERROR "Failed to send entire response" << endl;
    }
}

void *handle_request(void *foo){
    try{
        raii::FD ready(*((int *)foo));
        free(foo);
        pthread_detach(pthread_self());

        DEBUG "Attempting to read socket" << endl;
        string incoming;
        if(recv_string(ready.get(), incoming)){
            if (incoming.size() > 0){
                unique_ptr<sls::Request> request(new sls::Request);
                try{
                    request->ParseFromString(incoming);
                }
                catch(...){
                    ERROR "Malformed request" << endl;
                }
                sls::Response response;
                response.set_success(false);

                if(request->IsInitialized()){

                    if (request->has_req_append()){
                        sls::Append a = request->req_append();
                        if(a.IsInitialized()){
                            list<sls::Value> *l;
                            //acquire lock
                            {
                                DEBUG "Attempting to acquire lock: " << a.key() << endl;
                                lock_guard<mutex> guard(locks[a.key()]);
                                DEBUG "Lock acquired: " << a.key() << endl;
                                l = &(cache[a.key()]);
                                string d = a.data();
                                l->push_front(wrap(d));
                            }
                            //release lock

                            response.set_success(true);
                            string r;
                            response.SerializeToString(&r);
                            if(!send_string(ready.get(), r)){
                                ERROR "Failed to send response" << endl;
                            }

                            if(l->size() > cache_max){
                                //page out
                                DEBUG "Attempting to acquire lock: " << a.key() << endl;
                                lock_guard<mutex> guard((locks[a.key()]));
                                DEBUG "Lock acquired: " << a.key() << endl;
                                _page_out(a.key(), cache_min);
                            }
                        }
                        else{
                            ERROR "Append request not initialized" << endl;
                        }
                    }
                    else if (request->has_req_range()){
                        _lookup(ready.get(), request.get());
                    }
                    else{
                        ERROR "Cannot handle request" << endl;
                    }
                }
                else{
                    ERROR "Request is not properly initialized" << endl;
                }
            }
            else{
                ERROR "Request is empty" << endl;
            }
        }
        else{
            ERROR "Could not get request" << endl;
        }
    }
    catch(...){
        ERROR "Handler thread caught exception, exiting" << endl;
    }
    pthread_exit(NULL);
}

int main(){
    debug_set(true);
    error_set(true);
    DEBUG "Starting sls..." << endl;
    srand(time(0));

    inet_sock = listen_on(port, false);
    if (inet_sock < 0){
        ERROR "Could not open network socket" << endl;
        return -1;
    }

    unix_sock = listen_on(unix_domain_file.c_str(), false);
    if (unix_sock < 0){
        ERROR "Could not open unix domain socket" << endl;
        return -1;
    }

    signal(SIGINT, shutdown);

    fd_set set;
    FD_ZERO(&set); //ABSOLUTELY ESSENTIAL

    FD_SET(inet_sock, &set);
    FD_SET(unix_sock, &set);

    auto nfds = max(unix_sock, inet_sock) + 1;

    while (true){
        int *ready = (int *)(malloc (sizeof(int)));

        auto ret = select(nfds, &set, NULL, NULL, NULL);
        if(ret < 0){
            std::cerr << "Could not get next socket to read" << std::endl;
            return -1;
        }

        int incoming = -1;
        if FD_ISSET(inet_sock, &set){
            incoming = inet_sock;
        }
        else if FD_ISSET(unix_sock, &set){
            incoming = unix_sock;
        }

        DEBUG "Calling accept..." << endl;

        *ready = accept(incoming, NULL, NULL);
        DEBUG "Accepted socket.. " << *ready << endl;

        DEBUG "Spawning thread" << endl;
        pthread_t thread;
        pthread_create(&thread, NULL, handle_request, ready);
        DEBUG "Thread spawned" << endl;
    }
    return 0;
}

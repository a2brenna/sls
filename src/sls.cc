#include "config.h"
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netdb.h>
#include <map>
#include <list>
#include <mutex>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <hgutil.h>
#include <cstdlib>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sls.h"
#include "sls.pb.h"
#include <memory>

#define DEBUG if(true) cerr <<

using namespace std;

map<string, list<sls::Value> > cache;
map<string, mutex> locks;
int sock; //main socket

sls::Value wrap(string payload){
    sls::Value r;
    r.set_time(hires_time());
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
    DEBUG "Attempting to page out: " << key << endl;
    lock_guard<mutex> g(locks[key]);
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
        DEBUG "Failed to page out: " << key << endl;
    }
}

void shutdown(int signo){
    DEBUG "Attempting shutdown" << endl;
    if(signo == SIGSEGV){
        for(int ret = close(sock); ret != 0; ret = close(sock));
        exit(1);
    }
    shutdown(sock, 0);

    for(map<string, list<sls::Value> >::iterator i = cache.begin(); i != cache.end(); ++i){
        //we want to "leak" this lock so no more data can be appended... but we don't want to deadlock here, unable to page out to disk because we're stuck in the middle of an append operation
        locks[(*i).first].try_lock();
        _page_out((*i).first, 0);
    }

    DEBUG "Closing socket" << endl;
    for(int ret = close(sock); ret != 0; ret = close(sock));
    exit(0);
}

void _file_lookup(string key, string filename, sls::Archive *archive){
    string filepath = (disk_dir + key + "/" + filename);
    unique_ptr<string> s(new string);
    readfile(filepath, s.get());
    try{
        archive->ParseFromString(*(s.get()));
    }
    catch(...){
        DEBUG "Could not read from: " << filepath << endl;
    }
    return;
}

void file_lookup(string key, string filename, list<sls::Value> *r){
    unique_ptr<sls::Archive> archive(new sls::Archive);
    _file_lookup(key, filename, archive.get());

    r->clear();

    if(archive != NULL){
        for(int i = 0; i < archive->values_size(); i++){
            r->push_back(archive->values(i));
        }
    }
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
    auto position = d.begin();

    for(; ((*position).time() > end ) && (position != d.end()); position++);

    unsigned long long earliest = ((*position).time());
    for(; (earliest > start) && (position != d.end()); position++){
        result->push_front(*position);
        earliest = ((*position).time());
    }
    return earliest;
}

unsigned long long pick(const list<sls::Value> &d, unsigned long long current, unsigned long long start, unsigned long long end, list<sls::Value> *result){
    auto position = d.begin();
    for(; (current < start) && (position != d.end()); position++, ++current);
    for(; (current < end) && (position != d.end()); position++, ++current){
        result->push_back(*position);
    }
    return current;
}

void _lookup(int client_sock, sls::Request *request){
    unique_ptr<sls::Response> response(new sls::Response);
    response->set_success(false);

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

    unsigned long long current = 0;

    do{
        if( request->mutable_req_range()->is_time() ){
            auto earliest_seen = pick_time(*(d.get()), start, end, values.get());
            if( earliest_seen < start ){
                break;
            }
        }
        else{
            current = pick(*(d.get()), current, start, end, values.get());
            if(current >= end ){
                break;
            }
        }
        file_lookup(key, next_file, d.get());
        next_file = next_lookup(key, next_file);
    }while(true);

    for(sls::Value v: *values){
        sls::Data *datum = response->add_data();
        string s;
        v.SerializeToString(&s);
        datum->set_data(s);
    }

    unique_ptr<string> r(new string);
    response->SerializeToString(r.get());

    DEBUG "Total fetched: " << response->data_size() << endl;

    size_t sent = send(client_sock,r->c_str(), r->length(), MSG_NOSIGNAL);
    if( sent != r->length()){
        DEBUG "Failed to send entire response" << endl;
    }
}

void *handle_request(void *foo){
    int ready = *((int *)foo);
    free(foo);
    pthread_detach(pthread_self());

    string incoming = read_sock(ready);
    if (incoming.size() > 0){
        unique_ptr<sls::Request> request(new sls::Request);
        try{
            request->ParseFromString(incoming);
        }
        catch(...){
            DEBUG "Malformed request" << endl;
        }
        sls::Response response;
        response.set_success(false);

        if (request->has_req_append()){
            sls::Append a = request->req_append();
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
            DEBUG "Attempting to send response: " << ready << endl;
            send(ready, (const void *)r.c_str(), r.length(), MSG_NOSIGNAL);
            DEBUG "Sent response: " << ready << endl;

            if(l->size() > cache_max){
                //page out
                DEBUG "Attempting to acquire lock: " << a.key() << endl;
                lock_guard<mutex> guard((locks[a.key()]));
                DEBUG "Lock acquired: " << a.key() << endl;
                _page_out(a.key(), cache_min);
            }
        }
        else if (request->has_req_range()){
            _lookup(ready, request.get());
        }
        else{
            DEBUG "Cannot handle request" << endl;
        }
    }
    close(ready);
    pthread_exit(NULL);
}

int main(){
    DEBUG "Starting sls..." << endl;
    srand(time(0));

    sock = listen_on(port);
    if (sock < 0){
        DEBUG "Could not open socket" << endl;
        return -1;
    }

    signal(SIGINT, shutdown);

    while (true){
        int *ready = (int *)(malloc (sizeof(int)));
        DEBUG "Calling accept..." << endl;
        *ready = accept(sock, NULL, NULL);
        DEBUG "Accepted socket.. " << *ready << endl;

        DEBUG "Spawning thread" << endl;
        pthread_t thread;
        pthread_create(&thread, NULL, handle_request, ready);
        DEBUG "Thread spawned" << endl;
    }
    return 0;
}

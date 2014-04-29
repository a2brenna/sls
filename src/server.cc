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
#include <cstdlib>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "sls.h"
#include "sls.pb.h"
#include <memory>
#include <netdb.h>

namespace sls{

std::map<std::string, std::list<sls::Value> > cache;
std::map<std::string, std::mutex> locks;

sls::Value wrap(std::string payload){
    sls::Value r;
    r.set_time(milli_time());
    r.set_data(payload);
    return r;
}

void _page_out(std::string key, unsigned int skip){
    std::cerr << "Attempting to page out: " << key << std::endl;
    std::list<sls::Value>::iterator i = (cache[key]).begin();
    unsigned int j = 0;
    unsigned int size = cache[key].size();
    for(; (j < size) && (j < skip); ++j, ++i);
    std::list<sls::Value>::iterator new_end = i;

    //pack into archive
    std::unique_ptr<sls::Archive> archive(new sls::Archive);
    for(; i != cache[key].end(); ++i){
        sls::Value *v = archive->add_values();
        //maybe can eliminate this copy and use a pointer instead?
        v->CopyFrom(*i);
    }

    std::string head_link = disk_dir;
    head_link.append(key);
    head_link.append("/head");
    archive->set_next_archive(get_canonical_filename(head_link));

    std::unique_ptr<std::string> outfile(new std::string);
    archive->SerializeToString(outfile.get());

    //write new file
    std::string directory = disk_dir;
    directory.append(key);
    directory.append("/");

    mkdir(directory.c_str(), 0755);
    std::string new_file_name = RandomString(32);
    std::string new_file = directory + "/" + new_file_name;
    if (writefile(new_file, *(outfile.get()))){

    //set symlink from directory/head -> new_file_name
        remove(head_link.c_str());
        symlink(new_file_name.c_str(), head_link.c_str());

        //delete from new_end to end()
        cache[key].erase(new_end, cache[key].end());
    }
    else{
        std::cerr << "Failed to page out: " << key << std::endl;
    }
}

void _file_lookup(std::string key, std::string filename, sls::Archive *archive){
    if(filename == ""){
        return;
    }
    std::string filepath = (disk_dir + key + "/" + filename);
    std::unique_ptr<std::string> s(new std::string);
    readfile(filepath, s.get());
    try{
        archive->ParseFromString(*(s.get()));
    }
    catch(...){
        std::cerr << "Could not read from: " << filepath << std::endl;
    }
    return;
}

void file_lookup(std::string key, std::string filename, std::list<sls::Value> *r){
    std::unique_ptr<sls::Archive> archive(new sls::Archive);
    _file_lookup(key, filename, archive.get());

    r->clear();

    if(archive != NULL){
        for(int i = 0; i < archive->values_size(); i++){
            r->push_back(archive->values(i));
        }
    }
    return;
}

std::string next_lookup(std::string key, std::string filename){
    std::unique_ptr<sls::Archive> archive(new sls::Archive);
    _file_lookup(key, filename, archive.get());
    std::string next_archive;

    if(archive != NULL){
        if(archive->has_next_archive()){
            next_archive = archive->next_archive();
        }
    }
    return next_archive;
}

unsigned long long pick_time(const std::list<sls::Value> &d, unsigned long long start, unsigned long long end, std::list<sls::Value> *result){
    unsigned long long earliest = ULLONG_MAX;
    for(auto &value: d){
        if( (value.time() > start) && (value.time() < end) ){
            result->push_front(value);
        }
        earliest = std::min(earliest, (unsigned long long)value.time());
    }
    return earliest;
}

unsigned long long pick(const std::list<sls::Value> &d, unsigned long long current, unsigned long long start, unsigned long long end, std::list<sls::Value> *result){
    for (auto &value: d){
        if( (current >= start) && (current <= end) ){
            result->push_back(value);
        }
        current++;
    }
    return current;
}

void _lookup(int client_sock, sls::Request *request){
    std::unique_ptr<sls::Response> response(new sls::Response);
    response->set_success(false);

    if(request->mutable_req_range()->IsInitialized()){
        std::unique_ptr<std::list<sls::Value> > values(new std::list<sls::Value>);
        std::string key = request->key();
        unsigned long long start = request->mutable_req_range()->start();
        unsigned long long end = request->mutable_req_range()->end();
        std::string next_file;
        std::unique_ptr<std::list<sls::Value> > d(new std::list<sls::Value>);
        {
            std::lock_guard<std::mutex> guard(locks[key]);
            *(d.get()) = cache[key];
            next_file = get_canonical_filename(disk_dir + key + std::string("/head"));
        }


        unsigned long long current = 0;

        bool has_next = true;
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
            do{
                file_lookup(key, next_file, d.get());
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
            std::string s;
            v.SerializeToString(&s);
            datum->set_data(s);
        }
        response->set_success(true);
    }
    else{
        std::cerr << "Range not initialized" << std::endl;
    }

    std::unique_ptr<std::string> r(new std::string);
    response->SerializeToString(r.get());
    if(!send_string(client_sock, *r)){
        std::cerr << "Failed to send entire response" << std::endl;
    }
}

void *handle_request(void *foo){
    try{
        raii::FD ready(*((int *)foo));
        free(foo);
        pthread_detach(pthread_self());

        std::string incoming;
        if(recv_string(ready.get(), incoming)){
            if (incoming.size() > 0){
                std::unique_ptr<sls::Request> request(new sls::Request);
                try{
                    request->ParseFromString(incoming);
                    std::cout << request->DebugString();
                }
                catch(...){
                    std::cerr << "Malformed request" << std::endl;
                }
                sls::Response response;
                response.set_success(false);

                if(request->IsInitialized()){

                    if (request->has_req_append()){
                        sls::Append a = request->req_append();
                        if(a.IsInitialized()){
                            std::list<sls::Value> *l;
                            //acquire lock
                            {
                                std::lock_guard<std::mutex> guard(locks[a.key()]);
                                l = &(cache[a.key()]);
                                std::string d = a.data();
                                l->push_front(wrap(d));
                            }
                            //release lock

                            response.set_success(true);
                            std::string r;
                            response.SerializeToString(&r);
                            if(!send_string(ready.get(), r)){
                                std::cerr << "Failed to send response" << std::endl;
                            }

                            if(l->size() > cache_max){
                                //page out
                                std::lock_guard<std::mutex> guard((locks[a.key()]));
                                _page_out(a.key(), cache_min);
                            }
                        }
                        else{
                            std::cerr << "Append request not initialized" << std::endl;
                        }
                    }
                    else if (request->has_req_range()){
                        _lookup(ready.get(), request.get());
                    }
                    else{
                        std::cerr << "Cannot handle request" << std::endl;
                    }
                }
                else{
                    std::cerr << "Request is not properly initialized" << std::endl;
                }
            }
            else{
                std::cerr << "Request is empty" << std::endl;
            }
        }
        else{
            std::cerr << "Could not get request" << std::endl;
        }
    }
    catch(...){
        std::cerr << "Handler thread caught exception, exiting" << std::endl;
    }
    pthread_exit(NULL);
}

Server::Server(){
}

Server::~Server(){
}

void Server::handle(int sockfd){
    int *ready = (int *)(malloc (sizeof(int)));
    *ready = sockfd;
    pthread_t thread;
    pthread_create(&thread, NULL, handle_request, ready);
}

}

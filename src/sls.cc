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

struct Page_Out{
    char key[256];
};

void _page_out(string key, unsigned int skip){
    list<sls::Value>::iterator i = (cache[key]).begin();
    unsigned int j = 0;
    unsigned int size = cache[key].size();
    for(; (j < size) && (j < skip); ++j, ++i);
    list<sls::Value>::iterator new_end = i;

    //pack into archive
    sls::Archive *archive = new sls::Archive;
    for(; i != cache[key].end(); ++i){
        sls::Value *v = archive->add_values();
        //maybe can eliminate this copy and use a pointer instead?
        v->CopyFrom(*i);
    }

    char head[256];
    bzero(head, 256);
    string head_link = disk_dir;
    head_link.append(key);
    head_link.append("/head");

    if( readlink(head_link.c_str(), head, 256) < 0 ){
        DEBUG "Could not get current head file" << endl;
    }
    else{
        archive->set_next_archive(head);
    }

    string *outfile = new string;
    archive->SerializeToString(outfile);

    //write new file
    string directory = disk_dir;
    directory.append(key);
    directory.append("/");

    mkdir(directory.c_str(), 0755);
    string new_file_name = RandomString(32);
    string new_file = directory + "/" + new_file_name;
    std::ofstream fs;
    fs.open (new_file.c_str(), std::ofstream::in | std::ofstream::out | std::ofstream::trunc);
    if(fs.is_open()){
        fs.write(outfile->c_str(), outfile->length());
        fs.close();
    }
    else{
        DEBUG "Error opening new file" << endl;
    }

    //set symlink from directory/head -> new_file_name
    remove(head_link.c_str());
    symlink(new_file_name.c_str(), head_link.c_str());

    //delete from new_end to end()
    cache[key].erase(new_end, cache[key].end());

}

void *page_out(void *foo){
    struct Page_Out *bar;
    string key;
    try{
        bar = (struct Page_Out *)foo;
        key = string(bar->key);
    }
    catch(...){
    }

    lock_guard<mutex> guard((locks[key]));
    _page_out(key, cache_min);
    pthread_exit(NULL);
}

void shutdown(int signo){
    if(signo == SIGSEGV){
        for(int ret = close(sock); ret != 0; ret = close(sock));
        exit(1);
    }
    shutdown(sock, 0);

    for(map<string, list<sls::Value> >::iterator i = cache.begin(); i != cache.end(); ++i){
        lock_guard<mutex> guard((locks[(*i).first]));
        _page_out((*i).first, 0);
    }

    DEBUG "Closing socket" << endl;
    for(int ret = close(sock); ret != 0; ret = close(sock));
    exit(0);
}

struct Lookup{
    int sockfd;
    sls::Request *request;
};

sls::Archive *_file_lookup(string key, string filename){
    string filepath = (disk_dir + key + "/" + filename);
    DEBUG "attempting to open: " << filepath << endl;
    auto fd = open(filepath.c_str(), O_RDONLY);
    if( fd > 0){
        string *s = new string();
        read_sock(fd, s);
        if( s->size() == 0 ){
            DEBUG "String is empty: retrying" << endl;
            return _file_lookup(key, filename);
        }

        sls::Archive *archive = new sls::Archive;
        archive->ParseFromString(*s);
        close(fd);
        return archive;
    }
    else{
        DEBUG "Got a bad file descriptor" << endl;
        DEBUG "errno: " << errno << endl;
        return _file_lookup(key, filename);
    }
}

list<sls::Value> *file_lookup(string key, string filename){
    sls::Archive *archive = _file_lookup(key, filename);

    list<sls::Value> *r = new list<sls::Value>;
    if(archive == NULL){
        return r;
    }
    for(int i = 0; i < archive->values_size(); i++){
        r->push_back(archive->values(i));
    }

    return r;
}

string next_lookup(string key, string filename){
    sls::Archive *archive = _file_lookup(key, filename);

    if(archive == NULL){
        return string("");
    }

    if(archive->has_next_archive()){
        string next_archive = archive->next_archive();
        return next_archive;
    }
    else{
        return string("");
    }
}

void _lookup(int client_sock, sls::Request *request){
    sls::Response *response = new sls::Response;
    response->set_success(false);

    //need to verify some sanity here...

    string key = request->key();
    //acquire lock
    {
        lock_guard<mutex> guard(locks[key]);
        list<sls::Value> *d = &(cache[key]);
        list<sls::Value>::iterator i = d->begin();

        string next_file = string("head");
        if( !request->mutable_req_range()->is_time() ){
            //advance iterator to start of interval
            unsigned long long j = 0;
            unsigned long long fetched = 0;
            do{
                for(; (j < request->mutable_req_range()->start()) && (i != d->end()); ++j, ++i);
                for(; (j < request->mutable_req_range()->end()) && i != d->end(); ++j, ++i){
                    string r;
                    (*i).SerializeToString(&r);
                    response->add_data()->set_data(r);
                    fetched++;
                }

                DEBUG "Fetched: " << fetched << endl;
                if (next_file != "head"){
                    //operating off of a file... need to free it
                }
                if (next_file == ""){
                    DEBUG "No next file" << endl;
                    break;
                }

                d = file_lookup(key, next_file);
                if( d == NULL){
                    break;
                }
                i = d->begin();
                next_file = next_lookup(key, next_file);
            }
            while(j < request->mutable_req_range()->end()); //and while we have another file...
        }
        else{
            do{
                for(; ((*i).time() > request->mutable_req_range()->end()) && (i != d->end()); ++i);
                for(; ((*i).time() > request->mutable_req_range()->start()) && i != d->end(); ++i){
                    string r;
                    (*i).SerializeToString(&r);
                    response->add_data()->set_data(r);
                }
                if (next_file != "head"){
                    //operating off of a file... need to free it
                }
                if (next_file == ""){
                    DEBUG "No next file" << endl;
                    break;
                }

                d = file_lookup(key, next_file);
                if( d == NULL){
                    break;
                }
                i = d->begin();
                next_file = next_lookup(key, next_file);
            }
            //replace i with next_file.start()
            while( (*i).time() > request->mutable_req_range()->start()); //and while we have another file...
        }
    }

    string *r = new string;
    response->SerializeToString(r);

    DEBUG "Total fetched: " << response->data_size() << endl;

    size_t sent = send(client_sock,r->c_str(), r->length(), MSG_NOSIGNAL);
    if( sent != r->length()){
        DEBUG "Failed to send entire response" << endl;
    }
}

void *lookup(void *foo){
    struct Lookup *lstruct = (struct Lookup *)(foo);
    _lookup(lstruct->sockfd, (sls::Request *)(lstruct->request));

    close(lstruct->sockfd);
    pthread_exit(NULL);
}

void *handle_request(void *foo){
    int ready = *((int *)foo);
    free(foo);

    string incoming = read_sock(ready);
    if (incoming.size() > 0){
        sls::Request *request = new sls::Request;
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
                lock_guard<mutex> guard(locks[a.key()]);
                l = &(cache[a.key()]);
                string d = a.data();
                l->push_front(wrap(d));
            }
            //release lock

            response.set_success(true);
            string r;
            response.SerializeToString(&r);
            send(ready, (const void *)r.c_str(), r.length(), MSG_NOSIGNAL);

            if(l->size() > cache_max){
                //page out
                lock_guard<mutex> guard((locks[a.key()]));
                _page_out(a.key(), cache_min);
            }
            close(ready);
        }
        else if (request->has_req_range()){
            //spawn thread
            struct Lookup *job = (struct Lookup *)(malloc (sizeof(Lookup)));
            job->sockfd = ready;
            job->request = request;

            pthread_t thread;
            pthread_create(&thread, NULL, lookup, job);
        }
        else{
            DEBUG "Cannot handle request" << endl;
            close(ready);
        }
    }
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
        *ready = accept(sock, NULL, NULL);

        pthread_t thread;
        pthread_create(&thread, NULL, handle_request, ready);
    }
    return 0;
}

#include<iostream>

//For listen_on
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netdb.h>
#include "sls.h"

#include<map>
#include<list>

#include<pthread.h>

#include"sls.pb.h"

#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>

#include <algorithm>

#include <hgutil.h>
#include "config.h"

#include <cstdlib>
#include <signal.h>

using namespace std;

map<string, list<sls::Value> > cache;
map<string, pthread_mutex_t> locks;
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
        cerr << "Could not get current head file" << endl;
    }
    else{
        archive->set_next_archive(head);
    }

    string *outfile = new string;
    archive->SerializeToString(outfile);
    delete archive;

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
        cerr << "Error opening new file" << endl;
    }
    delete outfile;

    //set symlink from directory/head -> new_file_name
    remove(head_link.c_str());
    symlink(new_file_name.c_str(), head_link.c_str());

    //delete from new_end to end()
    cache[key].erase(new_end, cache[key].end());

}

void *page_out(void *foo){
    struct Page_Out *bar = (struct Page_Out *)foo;
    string key = string(bar->key);
    free(bar);
    pthread_mutex_lock(&(locks[key]));
    _page_out(key, cache_min);
    pthread_mutex_unlock(&(locks[key]));
    pthread_exit(NULL);
}

void shutdown(int signo){
    if(signo == SIGSEGV){
        for(int ret = close(sock); ret != 0; ret = close(sock));
        exit(1);
    }
    shutdown(sock, 0);

    for(map<string, list<sls::Value> >::iterator i = cache.begin(); i != cache.end(); ++i){
        pthread_mutex_lock(&(locks[(*i).first]));
        _page_out((*i).first, 0);
    }

    cerr << "Closing socket" << endl;
    for(int ret = close(sock); ret != 0; ret = close(sock));
    exit(0);
}

struct Lookup{
    int sockfd;
    sls::Request *request;
};

sls::Archive *_file_lookup(string key, string filename){
    cerr << "attempting to open: " << (disk_dir + key + "/" + filename) << endl;
    ifstream in((disk_dir + key + "/" + filename));
    string *s = new string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    if( s->size() == 0 ){
        delete s;
        cerr << "String is empty" << endl;
        return NULL;
    }

    sls::Archive *archive = new sls::Archive;
    archive->ParseFromString(*s);
    delete s;
    return archive;
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
    delete archive;

    return r;
}

string next_lookup(string key, string filename){
    sls::Archive *archive = _file_lookup(key, filename);

    if(archive == NULL){
        return string("");
    }

    if(archive->has_next_archive()){
        string next_archive = archive->next_archive();
        delete archive;
        return next_archive;
    }
    else{
        delete archive;
        return string("");
    }
}

void *lookup(void *foo){
    struct Lookup *lstruct = (struct Lookup *)(foo);
    int client_sock = lstruct->sockfd;
    sls::Request *request = (sls::Request *)(lstruct->request);

    sls::Response *response = new sls::Response;
    response->set_success(false);

    //need to verify some sanity here...

    string key = request->key();
    //acquire lock
    pthread_mutex_t *lock = &(locks[key]);
    pthread_mutex_lock(lock);
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

            cerr << "Fetched: " << fetched << endl;
            if (next_file != "head"){
                //operating off of a file... need to free it
                delete d;
            }
            if (next_file == ""){
                cerr << "No next file" << endl;
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
                delete d;
            }
            if (next_file == ""){
                cerr << "No next file" << endl;
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

    //release lock
    pthread_mutex_unlock(lock);

    string *r = new string;
    response->SerializeToString(r);

    cerr << "Total fetched: " << response->data_size() << endl;

    send(client_sock,r->c_str(), r->length(), MSG_NOSIGNAL);

    delete request;
    free(foo);
    delete response;
    close(client_sock);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]){
    srand(time(0));
    if(argc > 1){
        cerr << "Unknown arguments:";
        for(int i = 1; i < argc; i++){
            cerr << " " << argv[i];
        }
        return -1;
    }
    cerr << "Starting sls..." << endl;
    sock = listen_on(port);
    if (sock < 0){
        cerr << "Could not open socket" << endl;
        return -1;
    }

    signal(SIGINT, shutdown);
#ifndef DEBUG
    signal(SIGSEGV, shutdown);
#endif

    while (true){
        struct sockaddr addr;
        socklen_t addr_len = sizeof (struct sockaddr);
        int ready = accept(sock, &addr, &addr_len);

        char buffer[4096];
        bzero(buffer, 4096);
        int message_len = read(ready, &buffer, 4096);
        if (message_len > 0){
            sls::Request *request = new sls::Request;
            string incoming;
            incoming.assign(buffer, message_len);
            try{
                request->ParseFromString(incoming);
            }
            catch(...){
                cerr << "Malformed request" << endl;
            }
            sls::Response response;
            response.set_success(false);

            if (request->has_req_append()){
                sls::Append a = request->req_append();
                list<sls::Value> *l;
                //acquire lock
                pthread_mutex_t *lock = &(locks[a.key()]);
                pthread_mutex_lock(lock);
                l = &(cache[a.key()]);
                string d = a.data();
                l->push_front(wrap(d));
                //release lock
                pthread_mutex_unlock(lock);

                response.set_success(true);
                string r;
                response.SerializeToString(&r);
                send(ready, (const void *)r.c_str(), r.length(), MSG_NOSIGNAL);

                if(l->size() > cache_max){
                    struct Page_Out *p = (struct Page_Out *)malloc(sizeof (struct Page_Out));
                    strcpy(p->key, a.key().c_str());
                    pthread_t thread;
                    pthread_create(&thread, NULL, page_out, p);
                }
                //cerr << a.key().c_str() << " has " << cache[a.key()].size() << " elements" << endl;
                close(ready);
                delete request;
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
                cerr << "Cannot handle request" << endl;
                delete request;
                close(ready);
            }
        }
    }
    return 0;
}

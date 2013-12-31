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

using namespace std;

map<string, list<sls::Value> > cache;
map<string, pthread_mutex_t> locks;

sls::Value wrap(string payload){
    sls::Value r;
    r.set_time(hires_time());
    r.set_data(payload);
    return r;
}

struct Page_Out{
    char key[256];
};

void *page_out(void *foo){
    struct Page_Out *bar = (struct Page_Out *)foo;
    string key = string(bar->key);
    free(bar);
    pthread_mutex_lock(&(locks[key]));
    list<sls::Value>::iterator i = (cache[key]).begin();
    for(int j = 0; i != cache[key].end(), j < cache_min; ++j, ++i);
    list<sls::Value>::iterator new_end = i;

    //pack into archive
    sls::Archive *archive = new sls::Archive;
    for(; i != cache[key].end(); ++i){
        sls::Value *v = archive->add_values();
        v->CopyFrom(*i);
    }

    string *outfile = new string;
    archive->SerializeToString(outfile);
    cerr << "Outfile is: " << outfile->length();
    delete archive;

    //rotate files
    vector<string> files;
    string directory = disk_dir;
    directory.append(key);
    directory.append("/");
    int retval = getdir(directory, files);
    if(retval != 0){
        if (mkdir(directory.c_str(), S_IXUSR | S_IXGRP | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) != 0){
            cerr << "Could not make directory" << endl;
            delete outfile;
        }
    }

    sort(files.begin(), files.end());

    for(vector<string>::reverse_iterator i = files.rbegin(); i != files.rend(); i++){
        int num = stoi((*i));
        num++;
        rename((directory + *i).c_str(), (directory + to_string(num)).c_str());
    }

    //write new file
    string new_file = directory + "/0";
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
    //delete from new_end to end()
    cache[key].erase(new_end, cache[key].end());

    pthread_mutex_unlock(&(locks[key]));
}

struct Lookup{
    int sockfd;
    sls::Request *request;
};

list<sls::Value> *file_lookup(string key, int fileno){
    list<sls::Value> *r = new list<sls::Value>;

    cerr << "attempting to open: " << (disk_dir + key + "/" + to_string(fileno).c_str()) << endl;
    ifstream in((disk_dir + key + "/" + to_string(fileno).c_str()));
    string *s = new string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
    if( s->size() == 0 ){
        delete s;
        return NULL;
    }

    sls::Archive *archive = new sls::Archive;
    archive->ParseFromString(*s);
    delete s;

    for(int i = 0; i < archive->values_size(); i++){
        r->push_back(archive->values(i));
    }

    delete archive;
    return r;
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

    int next_file_index = 0;
    if( !request->mutable_req_range()->is_time() ){
        //advance iterator to start of interval
        unsigned long long j = 0;
        do{
            for(; (j < request->mutable_req_range()->start()) && (i != d->end()); ++j, ++i);
            for(; (j < request->mutable_req_range()->end()) && i != d->end(); ++j, ++i){
                string r;
                (*i).SerializeToString(&r);
                response->add_data()->set_data(r);
            }
            if (next_file_index != 0){
                //operating off of a file... need to free it
                delete d;
            }

            d = file_lookup(key, next_file_index++);
            if( d == NULL){
                break;
            }
            i = d->begin();
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
            if (next_file_index != 0){
                //operating off of a file... need to free it
                delete d;
            }

            d = file_lookup(key, next_file_index++);
            if( d == NULL){
                break;
            }
            i = d->begin();
        }
        //replace i with next_file.start()
        while( (*i).time() > request->mutable_req_range()->start()); //and while we have another file...
    }

    //release lock
    pthread_mutex_unlock(lock);

    string *r = new string;
    response->SerializeToString(r);

    send(client_sock,r->c_str(), r->length(), MSG_NOSIGNAL);

    delete request;
    free(foo);
    delete response;
    close(client_sock);
}

int main(int argc, char *argv[]){
    cerr << "Starting sls..." << endl;
    int sock = listen_on(port);
    if (sock < 0){
        cerr << "Could not open socket" << endl;
        return -1;
    }

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

                if(l->size() > cache_max){
                    struct Page_Out *p = (struct Page_Out *)malloc(sizeof (struct Page_Out));
                    strcpy(p->key, a.key().c_str());
                    pthread_t thread;
                    pthread_create(&thread, NULL, page_out, p);
                }
                string r;
                response.SerializeToString(&r);
                send(ready, (const void *)r.c_str(), r.length(), MSG_NOSIGNAL);
                cerr << a.key().c_str() << " has " << cache[a.key()].size() << " elements" << endl;
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

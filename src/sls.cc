#include<iostream>

//For listen_on
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>

#include<map>
#include<list>

#include<pthread.h>

#include"sls.pb.h"

#include <sys/time.h>

using namespace std;

map<string, list<sls::Value> > cache;

const char *port = "6998";

unsigned long long hires_time(){
    struct timeval tv;
    gettimeofday(&tv, NULL);

    unsigned long long millisecondsSinceEpoch =
    (unsigned long long)(tv.tv_sec) * 1000 +
    (unsigned long long)(tv.tv_usec) / 1000;

    return millisecondsSinceEpoch;
}

sls::Value wrap(string payload){
    sls::Value r;
    r.set_time(hires_time());
    r.set_data(payload);
    return r;
}

int listen_on(const char *port){
    struct addrinfo *res = (struct addrinfo *) malloc(sizeof (struct addrinfo));
    getaddrinfo("127.0.0.1", port, NULL, &res);

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0){
        cerr << "Could not open socket\n";
        return -1;
    }

    // bind it to the port we passed in to getaddrinfo():
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) != -1){
    }
    else{
        return -1;
    }
    if (listen(sockfd, 20) == -1){
        return -1;
    }
    return sockfd;
}

struct Lookup{
    int sockfd;
    sls::Request *request;
};

void *lookup(void *foo){
    struct Lookup *lstruct = (struct Lookup *)(foo);
    int client_sock = lstruct->sockfd;
    sls::Request *request = (sls::Request *)(lstruct->request);

    sls::Response *response = new sls::Response;
    response->set_success(false);

    //need to verify some sanity here...

    string key = request->key();
    list<sls::Value> *d = &(cache[key]);
    list<sls::Value>::iterator i = d->begin();

    if( !request->mutable_req_range()->is_time() ){
        //advance iterator to start of interval
        unsigned long long j = 0;
        for(; (j < request->mutable_req_range()->start()) && (i != d->end()); ++j, ++i);
        for(; (j < request->mutable_req_range()->end()) && i != d->end(); ++j, ++i){
            string r;
            (*i).SerializeToString(&r);
            response->add_data()->set_data(r);
        }
    }
    else{
        cerr << "Handling time interval" << endl;

    }

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
                l = &(cache[a.key()]);
                string d = a.data();
                l->push_front(wrap(d));
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

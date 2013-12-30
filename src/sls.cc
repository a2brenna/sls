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

using namespace std;

map<const char *, list<string> > cache;

const char *port = "6998";

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
    cerr << "Spawned lookup job" << endl;
    struct Lookup *lstruct = (struct Lookup *)(foo);
    int client_sock = lstruct->sockfd;
    sls::Request *request = (sls::Request *)(lstruct->request);

    sls::Response *response = new sls::Response;
    response->set_success(false);

    //need to verify some sanity here...

    string key = request->key();
    unsigned long long start = request->mutable_req_range()->start();
    unsigned long long end = request->mutable_req_range()->end();
    bool is_time = request->mutable_req_range()->is_time();

    cerr << "Doing NOT time interval lookup" << endl;
    unsigned long long index = 0;
    list<string> *d = &(cache[key.c_str()]);
    list<string>::iterator i = d->begin();
    for(; (index < start) && (index < d->size()); index++,  i++){
        cerr << "Skipping" << endl;
    }
    for(; (index < end) && (index < d->size()); index++,  i++){
        cerr << "Appending data" << endl;
        response->add_data()->set_data(*i);
    }

    string *r = new string;
    response->SerializeToString(r);

    send(client_sock,r->c_str(), r->length(), MSG_NOSIGNAL);

    delete request;
    free(foo);
    delete response;
    close(client_sock);
    cerr << "Finished lookup job" << endl;
}

int main(int argc, char *argv[]){
    cerr << "Starting sls..." << endl;
    int sock = listen_on(port);
    if (sock < 0){
        cerr << "Could not open socket" << endl;
    }

    while (true){
        struct sockaddr addr;
        socklen_t addr_len = sizeof (struct sockaddr);
        cerr << "Waiting for connection" << endl;
        int ready = accept(sock, &addr, &addr_len);
        cerr << "Connection received on socket " << ready << endl;

        char buffer[4096];
        bzero(buffer, 4096);
        cerr << "Got connection" << endl;
        int message_len = read(ready, &buffer, 4096);
        if (message_len > 0){
            sls::Request *request = new sls::Request;
            string incoming;
            incoming.assign(buffer, message_len);
            cerr << incoming << endl;
            try{
                request->ParseFromString(incoming);
            }
            catch(...){
                cerr << "Malformed request" << endl;
            }
            sls::Response response;
            response.set_success(false);

            if (request->has_req_append()){
                cerr << "Got append" << endl;
                sls::Append a = request->req_append();
                list<string> *l = &(cache[a.key().c_str()]);
                string d = a.data();
                l->push_front(d);
                cerr << "Key: " << a.key() << " now has " << l->size() << " values" << endl;
                string r;
                response.SerializeToString(&r);
                send(ready, (const void *)r.c_str(), r.length(), MSG_NOSIGNAL);
                close(ready);
                delete request;
            }
            else if (request->has_req_range()){
                cerr << "Got data request" << endl;
                //spawn thread
                struct Lookup *job = (struct Lookup *)(malloc (sizeof(Lookup)));
                job->sockfd = ready;
                job->request = request;

                cerr << job->request->DebugString() << endl;

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

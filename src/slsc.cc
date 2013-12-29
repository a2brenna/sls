#include"sls.h"

#include <string>
#include <list>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <iostream>
#include "sls.pb.h"

#include <limits.h>

using namespace std;

sls::Response *sls_send(sls::Request request){
    int sockfd = 0;
    struct sockaddr_in serv_addr;

    sls::Response *retval = (sls::Response *)(malloc (sizeof (sls::Response)));
    retval->set_success(false);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "Error : Could not create socket" << endl;
        return retval;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(6998);

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       cerr << "Error : Connect Failed" << endl;
       return retval;
    }

    string *rstring = (string *) malloc(sizeof (string));

    try{
        request.SerializeToString(rstring);
    }
    catch(...){
        retval->set_success(false);
        return retval;
    }

    if (send(sockfd, rstring, rstring->length(), 0) == rstring->length()){
        string *returned = (string *)(malloc (sizeof(string)));
        int i = 0;
        char b[512];
        do{
            bzero(b,512);
            i = recv(sockfd, b, 512, 0);
            returned->append(b, i);
        }
        while(i == 512);

        retval->ParseFromString(*returned);
        free(returned);
    }
    else{
        cerr << "Failed to send entire request" << endl;
    }
    free(rstring);

    return retval;
}

bool append(const char *key, string data){
    bool retval;
    try{
        sls::Request *request = (sls::Request *)(malloc (sizeof(sls::Request)));
        sls::Append *req_append = request->mutable_req_append();

        req_append->set_data(data);
        req_append->set_key(key);
        retval = sls_send(*request);
    }
    catch(...){
        return false;
    }

    return retval;
}

list<string> *_interval(const char *key, unsigned long long start, unsigned long long end, bool is_time){
    list<string> *r = (list<string> *)(malloc (sizeof(list<string>)));;
    //magic sauce
    return r;
}

list<string> *lastn(const char *key, int num_entries){
    return intervaln(key, 0, num_entries - 1);
}

list<string> *all(const char *key){
    return intervaln(key, 0, ULLONG_MAX);
}

list<string> *intervaln(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, false);
}

list<string> *intervalt(const char *key, unsigned long long start, unsigned long long end){
    return _interval(key, start, end, true);
}

string unwrap(const string value){
    string r;
    return r;
}

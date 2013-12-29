#include"sls.h"

#include <string>
#include <list>

#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <iostream>
#include "sls.pb.h"

using namespace std;

bool sls_send(sls::Request request){
    int sockfd = 0;
    struct sockaddr_in serv_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        cerr << "Error : Could not create socket" << endl;
        return false;
    }

    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(6998);

    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       cerr << "Error : Connect Failed" << endl;
       return false;
    }

    string *rstring = (string *) malloc(sizeof (string));

    request.SerializeToString(rstring);

    bool retval;
    if (send(sockfd, rstring, rstring->length(), 0) == rstring->length()){
        retval = true;
    }
    else{
        cerr << "Failed to send entire request" << endl;
        retval = false;
    }

    free(rstring);
    return retval;
}

bool append(const char *key, string data){
    return false;
}

list<string> lastn(const char *key, int num_entries){
    list<string> r;
    return r;
}

list<string> all(const char *key){
    list<string> r;
    return r;
}

string unwrap(const string value){
    string r;
    return r;
}

unsigned long long check_time(const string value){
    long long r;
    return r;
}

list<string> intervaln(const char *key, int start, int end){
    list<string> r;
    return r;
}

list<string> intervalt(const char *key, unsigned long long start, unsigned long long end){
    list<string> r;
    return r;
}

#include<iostream>

//For listen_on
#include <unistd.h>
#include <iostream>
#include <sys/socket.h>
#include <netdb.h>

using namespace std;

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

int main(int argc, char *argv[]){
    cerr << "Starting sls..." << endl;
    int sock = listen_on(port);
    if (sock < 0){
        cerr << "Could not open socket" << endl;
    }
    return 0;
}

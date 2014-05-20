#include <hgutil.h>
#include <iostream>
#include "sls.h"
#include "sls.pb.h"
#include <string>
#include <signal.h>
#include "config.h"

sls::Server *s;
Connection_Factory *connections;

void shutdown(int signal){
    //shutdown connections
    delete connections;
    //shutdown backend server
    delete s;
    //exit gracefully
    exit(0);
}

int main(){
    srand(time(0));

    s = new sls::Server(disk_dir, cache_min, cache_max);
    connections = new Connection_Factory();

    signal(SIGINT, shutdown);

    try{
        connections->add_socket(listen_on(port, false));
    }
    catch(Network_Error e){
        std::cerr << "Could not setup inet domain socket";
        std::cerr << e.msg << " : " << e.error_number << std::endl;
        return -1;
    }

    try{
        connections->add_socket(listen_on(unix_domain_file.c_str(), false));
    }
    catch(Network_Error e){
        std::cerr << "Could not setup unix domain socket";
        std::cerr << e.msg << " : " << e.error_number << std::endl;
        return -1;
    }

    while (true){
        s->handle(connections->next_connection());
    }
    return 0;
}

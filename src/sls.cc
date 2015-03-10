#include <iostream>
#include <string>
#include <signal.h>
#include <syslog.h>
#include <hgutil/socket.h>
#include <hgutil/fd.h>

#include "server.h"
#include "sls.h"
#include "sls.pb.h"
#include "config.h"

sls::Server *s;
Connection_Factory *connections;

void shutdown(int signal){
    (void)signal;
    //shutdown connections
    delete connections;
    //shutdown backend server
    delete s;
    //exit gracefully
    exit(0);
}

int main(){
    openlog("sls", LOG_NDELAY, LOG_LOCAL1);
    setlogmask(LOG_UPTO(LOG_INFO));
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
        syslog(LOG_DEBUG, "Got new connection");
        std::shared_ptr<Task> t(new sls::Incoming_Connection(connections->next_connection()));
        s->queue_task(t);
        s->handle_next();
    }
    return 0;
}

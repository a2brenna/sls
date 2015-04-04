#include <iostream>
#include <string>
#include <signal.h>
#include <syslog.h>

#include "server.h"
#include "sls.h"
#include "sls.pb.h"
#include "config.h"

#include <smpl.h>
#include <smplsocket.h>

sls::Server *s;

void shutdown(int signal){
    (void)signal;
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

    signal(SIGINT, shutdown);
/*
    connections->add_socket(listen_on(port, false));
    }
    catch(Network_Error e){
        std::cerr << "Could not setup inet domain socket";
        std::cerr << e.msg << " : " << e.error_number << std::endl;
        return -1;
    }
*/

    std::unique_ptr<smpl::Local_Address> incoming(new smpl::Local_UDS(CONFIG_UNIX_DOMAIN_FILE));

    while (true){
        std::shared_ptr<Task> t(new sls::Incoming_Connection( std::shared_ptr<smpl::Channel>(incoming->listen()) ));
        s->queue_task(t);
        s->handle_next();
    }
    return 0;
}

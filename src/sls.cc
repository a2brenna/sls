#include <hgutil.h>
#include <iostream>
#include "sls.h"
#include "sls.pb.h"
#include <string>

int port = 6998;
std::string unix_domain_file = "/tmp/sls.sock";
sls::Server s;

int main(){
    srand(time(0));

    Connection_Factory connections;
    try{
        connections.add_socket(listen_on(port, false));
    }
    catch(Network_Error e){
        std::cerr << "Could not setup inet domain socket";
        std::cerr << e.msg << " : " << e.error_number << std::endl;
        return -1;
    }

    try{
        connections.add_socket(listen_on(unix_domain_file.c_str(), false));
    }
    catch(Network_Error e){
        std::cerr << "Could not setup unix domain socket";
        std::cerr << e.msg << " : " << e.error_number << std::endl;
        return -1;
    }

    while (true){
        s.handle(connections.next_connection());
    }
    return 0;
}

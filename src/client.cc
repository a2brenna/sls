#include "client.h"

#include <hgutil/address.h>
#include <hgutil/socket.h>
#include <hgutil/fd.h>

Client::Client(){

    //TODO: Maybe use unique_ptr for this and avoid this check?
    if(global_server != NULL){
        server_connection(new Raw_Socket(connect_to(global_server)));
    }
    else{
        throw SLS_No_Server();
    }

}

Client::Client(Address *server){
    server_connection(new Raw_Socket(connect_to(global_server)));
}

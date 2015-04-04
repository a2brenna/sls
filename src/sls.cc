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

//TODO: THIS IS NOT SAFE
void shutdown(int signal){
    (void)signal;
    //shutdown backend server
    delete s;
    //exit gracefully
    exit(0);
}

void handle_channel(std::shared_ptr<smpl::Channel> client){
    for(;;){
        std::string request_string;
        try{
            request_string = client->recv();
        }
        catch(smpl::Error e){
            break;
        }

        sls::Request request;
        request.ParseFromString(request_string);

        sls::Response response;

        if(request.IsInitialized()){
            const auto key = request.key();
            if(request.has_req_append()){
                //decode values
                sls::Append a = request.req_append();
                const auto key = a.key();
                const auto data = a.data();

                //s->append(...);
                try{
                    s->append(key, data);
                    response.set_success(true);
                }
                catch(...){
                    response.set_success(false);
                }
            }
            else if(request.has_req_range()){
                try{
                    //decode values
                    const unsigned long long start = request.mutable_req_range()->start();
                    const unsigned long long end = request.mutable_req_range()->end();
                    const bool is_time = request.mutable_req_range()->is_time();

                    std::deque<sls::Value> r;
                    if(is_time){
                        auto start_time = std::chrono::high_resolution_clock::time_point(std::chrono::milliseconds(start));
                        auto end_time = std::chrono::high_resolution_clock::time_point(std::chrono::milliseconds(end));
                        r = s->time_lookup(key, start_time, end_time);
                    }
                    else{
                        r = s->index_lookup(key, start, end);
                    }

                    for(const auto &d: r){
                        auto datum = response.add_data();
                        std::string s;
                        d.SerializeToString(&s);
                        datum->set_data(s);
                    }

                    response.set_success(true);
                }
                catch(...){
                    response.set_success(false);
                }

            }
            else{
                syslog(LOG_ERR, "Cannot handle request");
            }
        }
        else{
            syslog(LOG_ERR, "Got invalid request");
        }

        std::string response_string;
        response.SerializeToString(&response_string);
        client->send(response_string);
    }
}

int main(){
    openlog("sls", LOG_NDELAY, LOG_LOCAL1);
    setlogmask(LOG_UPTO(LOG_INFO));
    srand(time(0));

    s = new sls::Server(CONFIG_DISK_DIR, CONFIG_CACHE_MIN, CONFIG_CACHE_MAX);

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
        std::shared_ptr<smpl::Channel> new_client(incoming->listen());
        auto t = std::thread(std::bind(handle_channel, new_client));
        t.detach();
    }
    return 0;
}

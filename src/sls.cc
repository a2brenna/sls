#include <iostream>
#include <string>
#include <signal.h>

#include "server.h"
#include "sls.h"
#include "sls.pb.h"
#include "config.h"

#include <smpl.h>
#include <smplsocket.h>

#include <slog/slog.h>
#include <slog/syslog.h>

#include <thread>
#include <memory>

sls::SLS *s;

std::unique_ptr<slog::Log> Error;

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
        catch(smpl::Transport_Failed e){
            break;
        }

        sls::Request request;
        request.ParseFromString(request_string);

        sls::Response response;
        response.set_success(false);

        std::string data_string;

        if(request.IsInitialized()){
            const std::string key = request.key();
            assert(key.size() > 0);
            if(request.has_req_append()){
                sls::Append a = request.req_append();
                const std::string data = a.data();

                s->append(key, data);

                response.set_success(true);
            }
            else if(request.has_req_range()){
                const uint64_t start = request.mutable_req_range()->start();
                const uint64_t end = request.mutable_req_range()->end();
                const bool is_time = request.mutable_req_range()->is_time();

                if(is_time){
                    data_string = s->time_lookup(key, start, end);
                }
                else{
                    data_string = s->index_lookup(key, start, end);
                }

                if(!data_string.empty()){
                    response.set_data_to_follow(true);
                }
                response.set_success(true);

            }
            else if(request.has_last()){
                const unsigned long long max_values = request.last().max_values();
                data_string = s->last_lookup(key, max_values);
                if(!data_string.empty()){
                    response.set_data_to_follow(true);
                }
                response.set_success(true);
            }
            else{
                *Error << "Cannot handle request" << std::endl;;
            }
        }
        else{
            *Error << "Got invalid request" << std::endl;;
        }

        std::string response_string;
        response.SerializeToString(&response_string);
        client->send(response_string);
        if(response.data_to_follow()){
            client->send(data_string);
        }
    }
}

int main(int argc, char* argv[]){
    srand(time(0));
    slog::initialize_syslog("sls", LOG_USER);
    Error = std::unique_ptr<slog::Log>( new slog::Log(std::shared_ptr<slog::Syslog>(new slog::Syslog(slog::kLogErr)), slog::kLogErr, "ERROR"));
    get_config(argc, argv);

    s = new sls::SLS(CONFIG_DISK_DIR);

    signal(SIGINT, shutdown);

    std::unique_ptr<smpl::Local_Address> incoming(new smpl::Local_UDS(CONFIG_UNIX_DOMAIN_FILE));

    while (true){
        std::shared_ptr<smpl::Channel> new_client(incoming->listen());
        auto t = std::thread(std::bind(handle_channel, new_client));
        t.detach();
    }
    return 0;
}

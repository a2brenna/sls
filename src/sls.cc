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

#include <thread>
#include <memory>

sls::SLS *s;

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
                //decode values
                sls::Append a = request.req_append();
                const std::string data = a.data();

                s->append(key, data);

                response.set_success(true);
            }
            else if(request.has_req_range()){
                //decode values
                const unsigned long long start = request.mutable_req_range()->start();
                const unsigned long long end = request.mutable_req_range()->end();
                const bool is_time = request.mutable_req_range()->is_time();

                std::deque<std::pair<uint64_t, std::string>> r;
                if(is_time){
                    auto start_time = std::chrono::high_resolution_clock::time_point(std::chrono::milliseconds(start));
                    auto end_time = std::chrono::high_resolution_clock::time_point(std::chrono::milliseconds(end));
                    r = s->time_lookup(key, start_time, end_time);
                }
                else{
                    r = s->index_lookup(key, start, end);
                }

                for(const auto &p: r){
                    data_string.append( (char *)(&p.first), sizeof(uint64_t));

                    uint64_t size = p.second.size();
                    data_string.append( (char *)(&size), sizeof(uint64_t));

                    data_string.append(p.second);
                }
                if(!data_string.empty()){
                    response.set_data_to_follow(true);
                }
                response.set_success(true);

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
        if(response.data_to_follow()){
            client->send(data_string);
        }
    }
}

int main(){
    openlog("sls", LOG_NDELAY, LOG_LOCAL1);
    setlogmask(LOG_UPTO(LOG_INFO));
    srand(time(0));

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

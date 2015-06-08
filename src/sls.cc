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

        std::vector<sls::Response> responses;

        if(request.IsInitialized()){
            const std::string key = request.key();
            assert(key.size() > 0);
            if(request.has_req_append()){
                sls::Response response;
                response.set_success(false);
                //decode values
                sls::Append a = request.req_append();
                const std::string data = a.data();

                s->append(key, data);
                response.set_success(true);
                responses.push_back(response);
            }
            else if(request.has_req_range()){
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

                auto index = r.begin();
                for(;;){
                    sls::Response response;
                    response.set_success(false);
                    for(size_t i = 0; ( (i < CONFIG_MAX_RESPONSE_SIZE) && (index != r.end()) ); i++, index++){
                        auto datum = response.add_data();
                        std::string s;
                        index->SerializeToString(&s);
                        datum->set_data(s);
                    }
                    response.set_success(true);
                    responses.push_back(response);
                    if(index == r.end() ){
                        break;
                    }
                    else{
                        continue;
                    }
                }

                std::cerr << "Response values: " << r.size() << std::endl;
            }
            else{
                syslog(LOG_ERR, "Cannot handle request");
            }
        }
        else{
            syslog(LOG_ERR, "Got invalid request");
        }

        responses.back().set_end_of_response(true);

        for(const auto &response: responses){
            std::string response_string;
            response.SerializeToString(&response_string);
            client->send(response_string);
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

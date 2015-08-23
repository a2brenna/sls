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

std::mutex requests_in_progress_lock;
int requests_in_progress = 0;
bool shutting_down = false;

/*Need to make sure that no threads are manipulating state on disk during
 * shutdown. Keep track of the number of threads that could possibly be
 * manipulating state on disk with "requests_in_progress" (see
 * handle_channel(...)). All threads must increment "requests_in_progress"
 * before entering critical section. Additionally, threads check to see
 * if we're shutting down before incrementing, terminating if we are
 * (causing the client Channel to close, indicating request failure).
 * Once in the critical section, non-zero "requests_in_progress" prevent
 * shutdown from proceeding until disk is in a consistent state.  Leaking
 * the lock prevents additional threads from entering critical section once
 * the last thread has completed it.
 *
 * The "shutting_down" bool should prevent threads from waiting on
 * requests_in_progress_lock and beginning another critical section after
 * requests_in_progress goes to 0, but before the shutdown thread can acquire
 * requests_in_progress_lock and complete shutdown.
 */
void shutdown(int signal){
    //Main thread can no longer accept new connections...
    (void)signal;

    for(;;){
        //supposed to leak...
        requests_in_progress_lock.lock();
        assert(requests_in_progress >= 0);
        shutting_down = true;
        if(requests_in_progress == 0){
            //All threads dead or safe to kill
            break;
        }
        else{
            //Need to unlock so one or more thread(s) can complete event loop
            //(and decrement requests_in_progress)
            requests_in_progress_lock.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    //all handle_channel() threads should have terminated OR are blocked
    //in recv() or otherwise safe to kill.

    delete s; //Causes indices to be synced to disk

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

        {
            std::lock_guard<std::mutex> l(requests_in_progress_lock);
            if(shutting_down){
                break;
            }
            assert(requests_in_progress >= 0);
            requests_in_progress++;
        }
        //Very important that the following code completes and requests_in_progress
        //gets decremented.

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

        std::lock_guard<std::mutex> l(requests_in_progress_lock);
        assert(requests_in_progress > 0);
        requests_in_progress--;
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

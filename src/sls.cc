#include <iostream>
#include <string>
#include <signal.h>

#include "server.h"
#include "sls.h"
#include "sls.pb.h"
#include "config.h"
#include "archive.h"

#include <smpl.h>
#include <smplsocket.h>

#include <slog/slog.h>
#include <slog/syslog.h>

#include <thread>
#include <memory>

#include <regex>

sls::SLS *s;

std::unique_ptr<slog::Log> Error;
std::unique_ptr<slog::Log> Info;

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
void shutdown(int signal) {
  // Main thread can no longer accept new connections...
  (void)signal;

  for (;;) {
    // supposed to leak...
    requests_in_progress_lock.lock();
    assert(requests_in_progress >= 0);
    shutting_down = true;
    if (requests_in_progress == 0) {
      // All threads dead or safe to kill
      break;
    } else {
      // Need to unlock so one or more thread(s) can complete event loop
      //(and decrement requests_in_progress)
      requests_in_progress_lock.unlock();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }

  // all handle_channel() threads should have terminated OR are blocked
  // in recv() or otherwise safe to kill.

  delete s; // Causes indices to be synced to disk

  exit(0);
}

void handle_channel(std::shared_ptr<smpl::Channel> client) {
  for (;;) {
    std::string request_string;
    try {
      request_string = client->recv();
    } catch (smpl::Transport_Failed e) {
      break;
    }

    {
      std::lock_guard<std::mutex> l(requests_in_progress_lock);
      if (shutting_down) {
        break;
      }
      assert(requests_in_progress >= 0);
      requests_in_progress++;
    }
    // Very important that the following code completes and requests_in_progress
    // gets decremented.

    sls::Request request;
    request.ParseFromString(request_string);

    sls::Response response;
    response.set_success(false);

    bool response_sent = false;

    sls::Archive data_string;

    if (request.IsInitialized()) {
        const std::string key = request.key();
        assert(key.size() > 0);
        if (request.has_req_append()) {
            sls::Append a = request.req_append();
            const std::string data = a.data();

            try {
                if (a.has_time()) {
                    const std::chrono::milliseconds time(a.time());
                    s->append(key, time, data);
                } else {
                    s->append(key, data);
                }
                response.set_success(true);
            } catch (sls::Out_Of_Order) {
                response.set_success(false);
            }
        } else if (request.has_req_range()) {
            try{
                const uint64_t start = request.mutable_req_range()->start();
                const uint64_t end = request.mutable_req_range()->end();
                const bool is_time = request.mutable_req_range()->is_time();

                if (is_time) {
                    data_string = s->time_lookup(key, std::chrono::milliseconds(start),
                                                std::chrono::milliseconds(end));
                } else {
                    data_string = s->index_lookup(key, start, end);
                }

                if (!(data_string.size() == 0)) {
                    response.set_data_to_follow(true);
                }
                response.set_success(true);
            }
            catch(std::bad_alloc &b){
                response.set_success(false);
                *Error << "Could not allocate memory" << std::endl;
            }

        } else if (request.has_last()) {
            try{
                const unsigned long long max_values = request.last().max_values();
                data_string = s->last_lookup(key, max_values);
                if (!(data_string.size() == 0)) {
                    response.set_data_to_follow(true);
                }
                response.set_success(true);
            }
            catch(std::bad_alloc &b){
                response.set_success(false);
                *Error << "Could not allocate memory" << std::endl;
            }
        } else if (request.has_packed_archive()) {
            try {
                s->append_archive(key, sls::Archive(request.packed_archive()));
                response.set_success(true);
            } catch (sls::Bad_Archive e) {
                response.set_success(false);
            } catch (sls::Out_Of_Order e) {
                response.set_success(false);
            }
        } else if (request.has_dump_regex()) {
            const auto dump_end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
            const auto dump_start = std::chrono::milliseconds(0);
            const std::regex key_filter(request.dump_regex());
            const auto all_keys = s->get_all_keys();

            std::vector<std::string> filtered_keys;
            for(const auto &k: all_keys){
                if(std::regex_match(k, key_filter)){
                    filtered_keys.push_back(k);
                }
            }

            response.set_dumped_keys(filtered_keys.size());
            response.set_success(true);
            std::string response_string;
            response.SerializeToString(&response_string);
            client->send(response_string);
            response_sent = true;

            for(const auto &k: filtered_keys){
                sls::Archive key_dump = s->time_lookup(key, dump_start, dump_end);

                sls::Archive_Header header;
                header.set_key(k);
                header.set_archive_size(key_dump.size());
                std::string header_string;
                header.SerializeToString(&header_string);

                client->send(header_string);
                client->send(key_dump.buffer(), key_dump.size());
            }
        } else {
            *Error << "Cannot handle request" << std::endl;
        }
    }

    try{
        if(!response_sent){
            std::string response_string;
            response.SerializeToString(&response_string);
            client->send(response_string);
            if (response.data_to_follow() && response.success()) {
                client->send(data_string.buffer(), data_string.size());
            }
        }
    }
    catch(smpl::Transport_Failed e){
        std::lock_guard<std::mutex> l(requests_in_progress_lock);
        assert(requests_in_progress > 0);
        requests_in_progress--;
        break;
    }

    std::lock_guard<std::mutex> l(requests_in_progress_lock);
    assert(requests_in_progress > 0);
    requests_in_progress--;
  }
}

void handle_local_address(std::shared_ptr<smpl::Local_Address> incoming){
  while (true) {
    std::shared_ptr<smpl::Channel> new_client(incoming->listen());
    auto t = std::thread(std::bind(handle_channel, new_client));
    t.detach();
  }
}

int main(int argc, char *argv[]) {
  srand(time(0));
  slog::initialize_syslog("sls", LOG_USER);
  Error = std::unique_ptr<slog::Log>(new slog::Log(
      std::shared_ptr<slog::Syslog>(new slog::Syslog(slog::kLogErr)),
      slog::kLogErr, "ERROR"));
  Info = std::unique_ptr<slog::Log>(new slog::Log(
      std::shared_ptr<slog::Syslog>(new slog::Syslog(slog::kLogInfo)),
      slog::kLogErr, "INFO"));
  get_config(argc, argv);

  s = new sls::SLS(CONFIG_DISK_DIR);

  signal(SIGINT, shutdown);

  std::shared_ptr<smpl::Local_Address> unix_domain_socket(
      new smpl::Local_UDS(CONFIG_UNIX_DOMAIN_FILE));

  std::shared_ptr<smpl::Local_Address> network_socket(
      new smpl::Local_Port("0.0.0.0", CONFIG_PORT));

    auto unix_domain_handler = std::thread(std::bind(handle_local_address, unix_domain_socket));
    unix_domain_handler.detach();

    auto network_domain_handler = std::thread(std::bind(handle_local_address, network_socket));
    network_domain_handler.detach();

  while (true){
      std::this_thread::sleep_for(std::chrono::seconds(3600));
  }

  return 0;
}

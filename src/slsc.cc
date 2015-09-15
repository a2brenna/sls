#include <vector>
#include <string>

#include "client.h"
#include "sls.h"
#include "sls.pb.h"

namespace sls {

bool append(const std::string &key, const std::string &data) {
  assert(!key.empty());

  try{
    Client c(global_server);
    return c.append(key, data);
  }
  catch(smpl::Connection_Failed e){
      return false;
  }
  catch(smpl::Transport_Failed e){
      return false;
  }
}

bool append(const std::string &key, const std::chrono::milliseconds &time,
            const std::string &data) {
  assert(!key.empty());

  try{
    Client c(global_server);
    return c.append(key, time, data);
  }
  catch(smpl::Connection_Failed e){
      return false;
  }
  catch(smpl::Transport_Failed e){
      return false;
  }
}

bool append_archive(const std::string &key, const Archive &archive){
  assert(!key.empty());

  try{
    Client c(global_server);
    return c.append_archive(key, archive);
  }
  catch(smpl::Connection_Failed e){
      return false;
  }
  catch(smpl::Transport_Failed e){
      return false;
  }
}

Archive lastn(const std::string &key, const unsigned long long &num_entries) {
  assert(!key.empty());

  if (num_entries == 0) {
    return Archive("");
  }

  Client c(global_server);
  return c.lastn(key, num_entries);
}

Archive all(const std::string &key) {
  assert(!key.empty());

  Client c(global_server);
  return c.all(key);
}

Archive intervalt(const std::string &key, const unsigned long long &start,
          const unsigned long long &end) {
  assert(!key.empty());
  assert(start <= end);

  Client c(global_server);
  return c.intervalt(key, start, end);
}

std::string
unwrap(const std::pair<std::chrono::milliseconds, std::string> &value) {
  return value.second;
}

unsigned long long
check_time(const std::pair<std::chrono::milliseconds, std::string> &value) {
  return value.first.count();
}
}

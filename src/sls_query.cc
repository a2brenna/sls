#include "sls.h"
#include <smpl.h>
#include <smplsocket.h>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

std::string KEY = "";
bool ALL = false;
bool DISCARD = false;
size_t LAST = 0;
size_t INDEX_START = 0;
size_t INDEX_END = 0;
uint64_t TIME_START = 0;
uint64_t TIME_END = 0;

std::string VALUE = "";
int64_t TIME = -1;
std::string CONFIG_SERVER = "/tmp/sls.sock";

void config(int argc, char *argv[]) {
  po::options_description desc("Options");

  desc.add_options()("help", "Produce help messagwe")
      ("key", po::value<std::string>(&KEY), "Key to retrieve values for")
      ("append", po::value<std::string>(&VALUE), "Value to append")
      ("time", po::value<int64_t>(&TIME), "Time (milliseconds since epoch) to append at")
      ("last", po::value<size_t>(&LAST), "Number of most recent elements to retrieve")
      ("start", po::value<uint64_t>(&TIME_START), "Time to start retrieving elements")
      ("end", po::value<uint64_t>(&TIME_END), "Time to start retrieving elements")
      ("all", po::bool_switch(&ALL), "Retrieve all elements")
      ("discard", po::bool_switch(&DISCARD), "Discard results after fetching. Useful for testing.")
      ("unix_domain_file", po::value<std::string>(&CONFIG_SERVER), "Server unix domain file");

  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (...) {
    std::cout << desc << std::endl;
    exit(1);
  }

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    exit(0);
  }

  if (KEY == "") {
    std::cout << desc << std::endl;
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  config(argc, argv);

  sls::global_server = std::shared_ptr<smpl::Remote_Address>(
      new smpl::Remote_UDS(CONFIG_SERVER));

  if (VALUE != "") {
    if (TIME < 0) {
      const auto r = sls::append(KEY, VALUE);
      if (!r) {
        return -1;
      }
    } else {
      const std::chrono::milliseconds time(TIME);
      const auto r = sls::append(KEY, time, VALUE);
      if (!r) {
        return -1;
      }
    }
  } else {

    if(!DISCARD){
        std::vector<std::pair<std::chrono::milliseconds, std::string>> result;
        if (LAST > 0) {
        result = sls::lastn(KEY, LAST).unpack();
        } else if (TIME_END > TIME_START) {
        result = sls::intervalt(KEY, std::chrono::milliseconds(TIME_START), std::chrono::milliseconds(TIME_END)).unpack();
        } else if (ALL) {
        result = sls::all(KEY).unpack();
        } else {
        return -1;
        }
        for (const auto &r : result) {
            std::cout << r.first.count() << " " << r.second << std::endl;
        }
    }
    else{
        size_t size;
        if (LAST > 0) {
        size = sls::lastn(KEY, LAST).size();
        } else if (TIME_END > TIME_START) {
        size = sls::intervalt(KEY, std::chrono::milliseconds(TIME_START), std::chrono::milliseconds(TIME_END)).size();
        } else if (ALL) {
        size = sls::all(KEY).size();
        } else {
        return -1;
        }
        std::cerr << "Fetched: " << size << std::endl;
    }
  }

  return 0;
}

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
uint64_t START_TIME = 0;
uint64_t END_TIME = 0;
uint64_t AGE = 0;

std::string VALUE = "";
int64_t TIME = -1;
std::string CONFIG_SERVER = "/tmp/sls.sock";

void config(int argc, char *argv[]) {
  po::options_description desc("Options");

  desc.add_options()("help", "Produce help messagwe")
      ("key", po::value<std::string>(&KEY), "Key to retrieve values for")
      ("value", po::value<std::string>(&VALUE), "Value to append")
      ("time", po::value<int64_t>(&TIME), "Time (milliseconds since epoch) to append at")
      ("last", po::value<size_t>(&LAST), "Number of most recent elements to retrieve")
      ("start_time", po::value<uint64_t>(&START_TIME), "Time to start retrieving elements")
      ("end_time", po::value<uint64_t>(&END_TIME), "Time to start retrieving elements")
      ("max_age", po::value<uint64_t>(&AGE), "Age of oldest element to fetch")
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

  if(AGE != 0){
      END_TIME = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      START_TIME = END_TIME - AGE;
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
        } else if (END_TIME > START_TIME) {
        result = sls::intervalt(KEY, std::chrono::milliseconds(START_TIME), std::chrono::milliseconds(END_TIME)).unpack();
        } else if (ALL) {
        result = sls::all(KEY).unpack();
        } else {
        return -1;
        }
        std::cerr << "Elements Retreieved: " << result.size() << std::endl;
        for (const auto &r : result) {
            std::cout << r.first.count() << " " << r.second << std::endl;
        }
    }
    else{
        uint64_t checksum;
        if (LAST > 0) {
        checksum = sls::lastn(KEY, LAST).checksum();
        } else if (END_TIME > START_TIME) {
        checksum = sls::intervalt(KEY, std::chrono::milliseconds(START_TIME), std::chrono::milliseconds(END_TIME)).checksum();
        } else if (ALL) {
        checksum = sls::all(KEY).checksum();
        } else {
        return -1;
        }
        std::cerr << "Checksum: " << checksum << std::endl;
    }
  }

  return 0;
}

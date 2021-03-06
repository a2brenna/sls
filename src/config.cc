#include "config.h"
#include <boost/program_options.hpp>
#include <fstream>
#include <iostream>

namespace po = boost::program_options;

std::string global_config_file = "/etc/sls.conf";

std::string CONFIG_DISK_DIR = "/pool/sls/buckets/";
int CONFIG_PORT = 6998;
std::string CONFIG_UNIX_DOMAIN_FILE = "/tmp/sls.sock";
size_t CONFIG_RESOLUTION = 1000;
size_t CONFIG_NUM_BUCKETS = 2048;

void get_config(int argc, char *argv[]) {
  po::options_description desc("Options");
  desc.add_options()
      ("CONFIG_PORT", po::value<int>(&CONFIG_PORT), "Specify network CONFIG_PORT to listen on")
      ("unix_domain_file", po::value<std::string>(&CONFIG_UNIX_DOMAIN_FILE), "Path to open unix domain socket on")
      ("dir", po::value<std::string>(&CONFIG_DISK_DIR), "Root directory of backend file storage")
      ("resolution", po::value<size_t>(&CONFIG_RESOLUTION), "Indexing resolution")
      ("num_buckets", po::value<size_t>(&CONFIG_NUM_BUCKETS), "Number of buckets to group key directories in")
      ;

  try {
    assert(CONFIG_RESOLUTION > 0);
    assert(CONFIG_NUM_BUCKETS > 0);

    std::ifstream global(global_config_file, std::ios_base::in);
    std::string user_config_file = getenv("HOME");
    std::ifstream user(user_config_file, std::ios_base::in);

    po::variables_map vm;
    po::store(po::parse_config_file(global, desc), vm);
    po::store(po::parse_config_file(user, desc), vm);
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
  } catch (...) {
    std::cerr << desc << std::endl;
    exit(-1);
  }

  return;
}

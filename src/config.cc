#include "config.h"
#include <boost/program_options.hpp>
#include <fstream>

namespace po = boost::program_options;

std::string global_config_file = "/etc/sls.conf";

size_t CONFIG_MAX_RESPONSE_SIZE = 1000000;
std::string CONFIG_DISK_DIR = "/tmp/sls/";
int port = 6998;
std::string CONFIG_UNIX_DOMAIN_FILE = "/tmp/sls.sock";

void get_config(int argc, char *argv[]){
    po::options_description desc("Options");
    desc.add_options()
        ("port", po::value<int>(&port), "Specify network port to listen on")
        ("unix_domain_file", po::value<std::string>(&CONFIG_UNIX_DOMAIN_FILE), "Path to open unix domain socket on")
        ("max_response_size", po::value<size_t>(&CONFIG_MAX_RESPONSE_SIZE), "Maximum number of elements in an individual response")
        ("dir", po::value<std::string>(&CONFIG_DISK_DIR), "Root directory of bargckend file storage")
        ;

    std::ifstream global(global_config_file, std::ios_base::in);
    std::string user_config_file = getenv("HOME");
    std::ifstream user(user_config_file, std::ios_base::in);

    po::variables_map vm;
    po::store(po::parse_config_file(global, desc), vm);
    po::store(po::parse_config_file(user, desc), vm);
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    return;
}

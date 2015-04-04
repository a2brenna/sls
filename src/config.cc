#include "config.h"
#include <boost/program_options.hpp>
#include <fstream>

namespace po = boost::program_options;

std::string global_config_file = "/etc/sls.conf";

unsigned long cache_min = 3600 * 1;
unsigned long cache_max = 3600 * 2;
std::string disk_dir = "/pool/sls/";
int port = 6998;
std::string unix_domain_file = "/tmp/sls.sock";

void get_config(int argc, char *argv[]){
    po::options_description desc("Options");
    desc.add_options()
        ("port", po::value<int>(&port), "Specify network port to listen on")
        ("unix_domain_file", po::value<std::string>(&unix_domain_file), "Path to open unix domain socket on")
        ("cache_min", po::value<unsigned long>(&cache_min), "Minimum number of most recent elements to keep in cache")
        ("cache_max", po::value<unsigned long>(&cache_max), "Maximum number of elements to store in ram before flushing to disk")
        ("dir", po::value<std::string>(&disk_dir), "Root directory of bargckend file storage")
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

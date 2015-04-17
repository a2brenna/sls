#include <iostream>
#include <sys/time.h>
#include <chrono>
#include <boost/program_options.hpp>
#include <vector>
#include <random>

#include <smpl.h>
#include <smplsocket.h>

#include "sls.h"

namespace po = boost::program_options;

std::string unix_domain_file;

const std::string key_alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
size_t CONFIG_MAX_KEYS = 1000;
size_t CONFIG_MAX_ELEMENTS = 100000;

void get_config(int argc, char* argv[]){
    po::options_description desc("Options");
    desc.add_options()
        ("unix_domain_file", po::value<std::string>(&unix_domain_file), "Path to connect to server on")
        ("max_keys", po::value<size_t>(&CONFIG_MAX_KEYS), "Maximum number of test keys")
        ("max_elements", po::value<size_t>(&CONFIG_MAX_ELEMENTS), "Maximum number of test elements per key")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    return;
}


int main(int argc, char* argv[]){
    std::cerr << "Test Client Started... " << std::endl;
    get_config(argc, argv);

    sls::global_server = std::shared_ptr<smpl::Remote_Address>(new smpl::Remote_UDS(unix_domain_file));
    sls::append("TEST", "VALUE");

    auto r = std::minstd_rand(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::map<std::string, std::vector<std::string>> test_data;

    const unsigned long long num_keys = r() % CONFIG_MAX_KEYS; //up to 100 random keys

    const size_t alphabet_size = key_alphabet.size();

    size_t total_elements = 0;
    for(unsigned long long i = 0; i < num_keys; i++){

        std::string key;
        for(size_t i = 0; i < ( r() % 64 ); i++){
            key.append(1, key_alphabet[ r() & alphabet_size ]);
        }

        for(size_t i = 0; i < ( r() % CONFIG_MAX_ELEMENTS ); i++){
            test_data[key].push_back( std::to_string(r()) );
        }

        total_elements += test_data[key].size();

    }

    const auto start_time = std::chrono::high_resolution_clock::now();

    for(const auto &k: test_data){
        const std::string key = k.first;
        const std::vector<std::string> data = k.second;

        for(const auto &d: data){
            sls::append(key.c_str(), d);
        }

    }
    const auto end_time = std::chrono::high_resolution_clock::now();

    std::cerr << "TEST_COMPLETE"
        << " nanos " << (end_time - start_time).count()
        << " elements " << total_elements
        << std::endl;

    return 0;
}

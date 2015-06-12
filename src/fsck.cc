#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <hgutil/files.h>
#include <boost/program_options.hpp>
#include <stdio.h>
#include <fstream>
#include <regex>
#include "sls.pb.h"

std::string CONFIG_ROOT_DIR;
bool CONFIG_FIX;
std::string CONFIG_KEY_FILTER = ".*";

size_t total_discards = 0;
size_t fix = 0;
size_t head_fix = 0;

namespace po = boost::program_options;

void config(int argc, char *argv[]){

    po::options_description desc("Options");

    try{
        desc.add_options()
            ("help", "Produce help message")
            ("root_dir", po::value<std::string>(&CONFIG_ROOT_DIR), "Absolute PATH to root of sls storage directory")
            ("filter", po::value<std::string>(&CONFIG_KEY_FILTER), "Regex filter")
            ("fix", po::bool_switch(&CONFIG_FIX), "Attempt to fix problems")
            ;
    }
    catch(...){
        throw;
    }

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(0);

    }

    if(CONFIG_ROOT_DIR.empty()){
        std::cout << "You must specify a root directory [--root_dir=...]" << std::endl;
        exit(-1);
    }
}

int main(int argc, char *argv[]){
    config(argc, argv);

    std::vector<std::string> raw_keys;

    try{
        getdir( CONFIG_ROOT_DIR, raw_keys );
    }
    catch(...){
        std::cerr << "Could not list keys in " << CONFIG_ROOT_DIR << std::endl;
        exit(-1);
    }

    std::regex f(CONFIG_KEY_FILTER);
    std::vector<std::string> keys;
    for(const auto &rk: raw_keys){
        if(std::regex_match(rk, f)){
            keys.push_back(rk);
        }
        else{
            std::cerr << "Filtering out: " << rk << std::endl;
        }
    }

    for(const auto &k: keys){
        const std::string key_path = CONFIG_ROOT_DIR + "/" + k;

        std::set<std::string> chunk_files;
        {
            try{
                std::vector<std::string> chunks;
                getdir( key_path, chunks );
                for(const auto &c: chunks){
                    if(c != "index"){
                        chunk_files.insert(key_path + "/" + c);
                    }
                }
            }
            catch(...){
                std::cerr << "Could not list chunks in " << key_path << std::endl;
                continue;
            }
        }
    }

    std::cout << "Total Discarded: " << total_discards << std::endl;
    std::cout << "Fixed: " << fix << std::endl;
    std::cout << "Head Fixed: " << head_fix << std::endl;

    return 0;
};

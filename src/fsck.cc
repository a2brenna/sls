#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <algorithm>
#include <hgutil/files.h>
#include <boost/program_options.hpp>
#include <fstream>
#include <regex>
#include "sls.pb.h"
#include "file.h"
#include "index.h"
#include "archive.h"

std::string CONFIG_ROOT_DIR;
bool CONFIG_FIX;
std::string CONFIG_KEY_FILTER = ".*";

size_t total_discards = 0;
size_t fix = 0;
size_t head_fix = 0;

namespace po = boost::program_options;

void config(int argc, char *argv[]){

    po::options_description desc("Options");

    desc.add_options()
        ("help", "Produce help message")
        ("root_dir", po::value<std::string>(&CONFIG_ROOT_DIR), "Absolute PATH to root of sls storage directory")
        ("filter", po::value<std::string>(&CONFIG_KEY_FILTER), "Regex filter")
        ("fix", po::bool_switch(&CONFIG_FIX), "Attempt to fix problems")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(0);

    }

    if(CONFIG_ROOT_DIR.empty()){
        std::cout << "You must specify a root directory [--root_dir=...]" << std::endl;
        exit(1);
    }
}

int main(int argc, char *argv[]){
    config(argc, argv);

    //get list of all keys
    std::vector<std::string> raw_keys;
    try{
        getdir( CONFIG_ROOT_DIR, raw_keys );
    }
    catch(...){
        std::cerr << "Could not list keys in " << CONFIG_ROOT_DIR << std::endl;
        exit(1);
    }

    //filter keys that don't match provided regex
    std::regex f(CONFIG_KEY_FILTER);
    std::vector<std::string> keys;
    for(const auto &rk: raw_keys){
        if(std::regex_match(rk, f)){
            keys.push_back(rk);
        }
    }

    for(const auto &k: keys){
        const Path key_path(CONFIG_ROOT_DIR + "/" + k);

        const Path index_path(key_path.str() + "/index");
        Index index(index_path);

        std::set<std::string> chunk_files;
        {
            try{
                std::vector<std::string> chunks;
                getdir( key_path.str(), chunks );
                for(const auto &c: chunks){
                    if(c != "index"){
                        chunk_files.insert(key_path.str() + "/" + c);
                    }
                }
            }
            catch(...){
                std::cerr << "Could not list chunks in " << key_path.str() << std::endl;
                continue;
            }
        }

        //Check files in directory vs. index
        std::set<std::string> missing_but_indexed;
        std::set<std::string> not_indexed = chunk_files;
        for(const auto &f: index.index()){
            if( chunk_files.find(f.filename()) == chunk_files.end() ){
                missing_but_indexed.insert(f.filename());
            }

            not_indexed.erase(f.filename());
        }

        for(const auto &f: missing_but_indexed){
            std::cerr << "File missing: " << (key_path.str() + "/" + f) << std::endl;
        }

        for(const auto &f: not_indexed){
            std::cerr << "File not indexed: " << (key_path.str() + "/" + f) << std::endl;
        }

        if( !not_indexed.empty() || !missing_but_indexed.empty() ){
            //rebuild index
            try{
                index = build_index(key_path.str());
            }
            catch(...){
                std::cerr << "Error, failure to build_index for: " << key_path.str() << std::endl;
                continue;
            }

            std::ofstream o(index_path.str(), std::ofstream::out | std::ofstream::trunc );
            if(o){
                std::cerr << "Error, unable to open file for writing: " << index_path.str() << std::endl;
                continue;
            }
            else{
                o << index;
                o.close();
            }
        }
    }

    std::cout << "Total Discarded: " << total_discards << std::endl;
    std::cout << "Fixed: " << fix << std::endl;
    std::cout << "Head Fixed: " << head_fix << std::endl;

    return 0;
};

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <hgutil/files.h>
#include <boost/program_options.hpp>
#include "sls.pb.h"

std::string CONFIG_ROOT_DIR = "/pool/sls";
bool CONFIG_FIX = false;

namespace po = boost::program_options;

void config(const int &argc, const char *argv[]){

    po::options_description desc("Options");

    try{
        desc.add_options()
            ("help", "Produce help message")
            ("root_dir", po::value<std::string>(&CONFIG_ROOT_DIR), "Absolute PATH to root of sls storage directory")
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
        std::cout << desc << std::endl;;
        exit(0);

    }
}

std::map< std::string, std::pair<unsigned long long, unsigned long long> > build_file_index( const std::set<std::string> &chunk_files ){
    std::map< std::string, std::pair<unsigned long long, unsigned long long> > index;
    for (const auto &c: chunk_files){
        const std::string chunk = readfile(c);
        sls::Archive arch;
        arch.ParseFromString(chunk);

        const auto num_values = arch.values_size();

        if(num_values > 0){
            const unsigned long long earliest_timestamp = arch.values(num_values - 1).time();
            const unsigned long long latest_timestamp = arch.values(0).time();
            index[c] = std::pair<unsigned long long, unsigned long long>(earliest_timestamp, latest_timestamp);
        }
    }
    return index;
}

std::map< std::string, std::pair<unsigned long long, unsigned long long> > build_index( const std::set<std::string> &chunk_files ){
    std::map< std::string, std::pair<unsigned long long, unsigned long long> > index;
    for (const auto &c: chunk_files){
        const std::string chunk = readfile(c);
        sls::Archive arch;
        arch.ParseFromString(chunk);

        const auto num_values = arch.values_size();

        if(num_values > 0){
            const unsigned long long earliest_timestamp = arch.values(num_values - 1).time();
            const unsigned long long latest_timestamp = arch.values(0).time();
            index[c] = std::pair<unsigned long long, unsigned long long>(earliest_timestamp, latest_timestamp);
        }
    }
    return index;
}

std::string get_next_archive(const std::string &chunk_path){
    const std::string chunk = readfile(chunk_path);
    sls::Archive arch;
    arch.ParseFromString(chunk);

    if(arch.has_next_archive()){
        return arch.next_archive();
    }
    else{
        return "";
    }
}

std::string get_first_chunk( const std::map< std::string, std::pair< unsigned long long, unsigned long long> > &index){
    std::string first_chunk = "";
    if( index.size() > 0){
        unsigned long long highest_index = index.begin()->second.second;
        for(const auto &c: index){
            if( c.second.second > highest_index){
                highest_index = c.second.second;
                first_chunk = c.first;
            }
        }
    }
    return first_chunk;
}

int main(int argc, char *argv[]){
    (void)argc;
    (void)argv;

    std::vector<std::string> keys;

    try{
        getdir( CONFIG_ROOT_DIR, keys );
    }
    catch(...){
        std::cerr << "Could not list keys in " << CONFIG_ROOT_DIR << std::endl;
        exit(-1);
    }

    for(const auto &k: keys){
        const std::string key_path = CONFIG_ROOT_DIR + "/" + k;

        std::set<std::string> chunk_files;
        {
            try{
                std::vector<std::string> chunks;
                getdir( key_path, chunks );
                for(const auto &c: chunks){
                    if(c != "head"){
                        chunk_files.insert(key_path + "/" + c);
                    }
                }
            }
            catch(...){
                std::cerr << "Could not list chunks in " << key_path << std::endl;
                continue;
            }
        }

        const std::map< std::string, std::pair< unsigned long long, unsigned long long> > file_index = build_file_index(chunk_files);
        for(const auto &f: file_index){
            std::cout << f.first << std::endl;
        }

        const std::string head_path = key_path + "/head";
        {
            std::string head_chunk = get_canonical_filename(head_path);
            std::string head_chunk_path = key_path + "/" + head_chunk;
            if(head_chunk == ""){
                head_chunk_path = get_first_chunk(file_index);
                size_t slash_position = head_chunk_path.rfind('/') + 1;
                std::string stripped_path = head_chunk_path;
                stripped_path.erase(0, slash_position);
                std::cout << head_chunk_path << " disconnected" << std::endl;
                if(CONFIG_FIX){
                    remove(head_path.c_str());
                    symlink(stripped_path.c_str(), head_path.c_str());
                    std::cout << head_path << " -> " << stripped_path << std::endl;
                }

            }
            chunk_files.erase(head_chunk_path);

        }

        std::string current_chunk;
        try{
            current_chunk = get_canonical_filename(head_path);
        }
        catch(...){
            std::cout << "CAUGHT EXCEPTION CANONICALIZTING " << head_path << std::endl;
        }

        for(;;){
            const std::string current_chunk_file = key_path + "/" + current_chunk;
            const std::string next_chunk= get_next_archive(current_chunk_file);
            if(next_chunk == ""){
                if(chunk_files.empty()){
                    std::cout << "Chain terminates @ " << current_chunk_file << std::endl;
                }
                else{
                    std::cout << "Chain broken @ " << current_chunk_file << std::endl;
                    //find next and rewrite chunk
                    break;
                }
            }
            else{
                chunk_files.erase(current_chunk_file);
                current_chunk = next_chunk;
            }
        }
    }

    return 0;
};

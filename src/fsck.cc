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

void config(int argc, char *argv[]){

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
    std::cout << "Args parsed" << std::endl;
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
        unsigned long long highest_stamp = index.begin()->second.second;
        for(const auto &c: index){
            if( c.second.second >= highest_stamp){
                highest_stamp = c.second.second;
                first_chunk = c.first;
            }
        }
    }
    return first_chunk;
}

std::string get_next_chunk( const std::map< std::string, std::pair< unsigned long long, unsigned long long> > &index, const unsigned long long &max_stamp){
    std::string first_chunk = "";
    if( index.size() > 0){
        unsigned long long highest_stamp = 0;
        for(const auto &c: index){
            if( (c.second.second > highest_stamp) && (c.second.second < max_stamp) ){
                highest_stamp = c.second.second;
                first_chunk = c.first;
            }
            else{
                std::cerr << "current_stamp " << c.second.second
                    << " highest_stamp " << highest_stamp
                    << " max_stamp " << max_stamp
                    << std::endl;
            }
        }
    }
    else{
        std::cerr << "Index is empty" << std::endl;
    }
    return first_chunk;
}

std::pair<unsigned long long, unsigned long long> get_bounds(const std::string &chunk_file){
    try{
        std::cerr << "Reading chunk_file " << chunk_file << std::endl;
        const std::string chunk = readfile(chunk_file);
        sls::Archive arch;
        arch.ParseFromString(chunk);

        const auto num_values = arch.values_size();

        if(num_values > 0){
            const unsigned long long earliest_timestamp = arch.values(num_values - 1).time();
            const unsigned long long latest_timestamp = arch.values(0).time();
            return std::pair<unsigned long long, unsigned long long>(earliest_timestamp, latest_timestamp);
        }
        else{
            std::cerr << "Got no values!" << std::endl;
        }
    }
    catch(...){
        std::cerr << "Caught exception" << std::endl;
    }
    return std::pair<unsigned long long, unsigned long long>(0,0);
}

std::string reset_head(const std::map< std::string, std::pair< unsigned long long, unsigned long long> > &file_index, const std::string &head_path){
    std::string head_chunk_path = get_first_chunk(file_index);
    size_t slash_position = head_chunk_path.rfind('/') + 1;
    std::string stripped_path = head_chunk_path;
    stripped_path.erase(0, slash_position);
    std::cout << head_chunk_path << " disconnected" << std::endl;
    if(CONFIG_FIX){
        remove(head_path.c_str());
        symlink(stripped_path.c_str(), head_path.c_str());
        std::cout << head_path << " -> " << stripped_path << std::endl;
    }
    return head_chunk_path;
}

int main(int argc, char *argv[]){
    config(argc, argv);

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
        /*
        for(const auto &f: file_index){
            std::cout << f.first << " " << f.second.first << " " << f.second.second << std::endl;
        }
        */

        std::cout << "Indexing: " << file_index.size() << std::endl;

        if(file_index.size() == 0){
            std::cerr << "No valid data: " << k << std::endl;
            continue;
        }

        const std::string head_path = key_path + "/head";
        {
            std::string head_chunk;
            std::string head_chunk_path;
            try{
                head_chunk = get_canonical_filename(head_path);
                head_chunk_path = key_path + "/" + head_chunk;
            }
            catch(...){
                std::string old_head_chunk_path = head_chunk_path;
                head_chunk_path = reset_head(file_index, head_path);
                std::cerr << "old_head_chunk_path " << old_head_chunk_path << " head_chunk_path " << head_chunk_path << std::endl;
            }
            if(head_chunk == ""){
                std::string old_head_chunk_path = head_chunk_path;
                head_chunk_path = reset_head(file_index, head_path);
                std::cerr << "old_head_chunk_path " << old_head_chunk_path << " head_chunk_path " << head_chunk_path << std::endl;
            }
            else if( get_bounds(head_chunk_path).second == 0){
                std::string old_head_chunk_path = head_chunk_path;
                head_chunk_path = reset_head(file_index, head_path);
                std::cerr << "old_head_chunk_path " << old_head_chunk_path << " head_chunk_path " << head_chunk_path << std::endl;
            }
            else{
                std::cerr << "head_chunk_path " << head_chunk_path << std::endl;
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

        unsigned long long last_good_timestamp = get_bounds(key_path + "/" + current_chunk).first;
        std::cout << "Head oldest: " << last_good_timestamp << std::endl;
    }

    return 0;
};

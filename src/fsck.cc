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

std::string CONFIG_ROOT_DIR = "/pool/sls";
bool CONFIG_FIX = false;
std::string CONFIG_FILTER = ".*";

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
            ("filter", po::value<std::string>(&CONFIG_FILTER), "Regex filter")
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
    size_t discarded = 0;
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
        else{
            discarded++;
        }
    }
    if(discarded > 0){
        total_discards += discarded;
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
        }
    }
    return first_chunk;
}

std::pair<unsigned long long, unsigned long long> get_bounds(const std::string &chunk_file){
    try{
        const std::string chunk = readfile(chunk_file);
        sls::Archive arch;
        arch.ParseFromString(chunk);

        const auto num_values = arch.values_size();

        if(num_values > 0){
            const unsigned long long earliest_timestamp = arch.values(num_values - 1).time();
            const unsigned long long latest_timestamp = arch.values(0).time();
            return std::pair<unsigned long long, unsigned long long>(earliest_timestamp, latest_timestamp);
        }
    }
    catch(...){
        throw;
    }
    return std::pair<unsigned long long, unsigned long long>(0,0);
}

std::string strip_to_last(const std::string &foo, const char limit){
    size_t posn = foo.rfind(limit) + 1;
    std::string result = foo;
    result.erase(0,posn);
    return result;
}

std::string reset_head(const std::map< std::string, std::pair< unsigned long long, unsigned long long> > &file_index, const std::string &head_path){
    std::string head_chunk_path = get_first_chunk(file_index);
    std::string stripped_path = strip_to_last(head_chunk_path, '/');
    if(CONFIG_FIX){
        remove(head_path.c_str());
        symlink(stripped_path.c_str(), head_path.c_str());
        std::cout << head_path << " -> " << stripped_path << std::endl;
        head_fix++;
    }
    return head_chunk_path;
}

std::vector<std::string> generate_chain( const std::map< std::string, std::pair< unsigned long long, unsigned long long> > &index){
    std::vector< std::pair<std::pair<unsigned long long, unsigned long long>, std::string >> chain;
    for(const auto &i: index){
        chain.push_back(std::pair< std::pair<unsigned long long, unsigned long long>, std::string >(i.second, i.first));
    }

    std::sort(chain.begin(), chain.end());
    std::reverse(chain.begin(), chain.end());

    std::vector<std::string> result;
    for(const auto &c: chain){
        result.push_back(c.second);
    }
    return result;
}

std::vector<std::string> traverse_chain( const std::string &root, const std::string &head_chunk){
    std::vector<std::string> result;
    std::string current_file = root + head_chunk;
    for(;;){
        try{
            result.push_back(current_file);
            const auto foo = get_next_archive(current_file);
            if(foo == ""){
                break;
            }
            current_file = root + foo;
        }
        catch(...){
            break;
        }
    }
    return result;
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

    std::regex f(CONFIG_FILTER);
    std::vector<std::string> keys;
    for(const auto &rk: raw_keys){
        if(std::regex_match(rk, f)){
            keys.push_back(rk);
        }
        else{
            std::cerr << "Rejecting: " << rk << std::endl;
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


        const auto traversed_chain = traverse_chain(key_path + "/", current_chunk);

        std::deque<std::string> index_chain;
        for(const auto &c: generate_chain(file_index)){
            index_chain.push_back(c);
        }

        while(index_chain.size() > 1){
            std::string current = index_chain.front();
            index_chain.pop_front();

            const std::string next_archive = get_next_archive(current);
            const std::string reported_next_archive = key_path + "/" + next_archive;
            if(reported_next_archive != index_chain.front()){

                std::string chunk = readfile(current);
                sls::Archive arch;
                arch.ParseFromString(chunk);
                std::cout << "OLD " <<  arch.next_archive() << std::endl;
                std::string new_next_archive = strip_to_last(index_chain.front(), '/');
                std::cout << "NEW_NEXT " << new_next_archive << std::endl;
                arch.set_next_archive(new_next_archive);
                std::cout << "NEW " <<  arch.next_archive() << std::endl;
                std::string new_chunk;
                arch.SerializeToString(&new_chunk);

                if(CONFIG_FIX){
                    std::cout << "Removing: " << current << std::endl;
                    std::ofstream file(current, std::ios_base::trunc);
                    file << new_chunk;
                    file.close();

                    fix++;
                }
                else{
                    std::cout << "Would replace " << current << std::endl;
                }
            }
        }

    }
    std::cout << "Total Discarded: " << total_discards << std::endl;
    std::cout << "Fixed: " << fix << std::endl;
    std::cout << "Head Fixed: " << head_fix << std::endl;

    return 0;
};

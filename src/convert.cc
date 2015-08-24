#include <boost/program_options.hpp>
#include <cassert>
#include <sys/stat.h>//for mkdir()
#include <sys/types.h>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <set>

#include "legacy.pb.h"
#include "file.h"

std::string CONFIG_SOURCE_DIR;
std::string CONFIG_TARGET_DIR;
std::string CONFIG_ACCEPT = ".*";

namespace po = boost::program_options;

void config(int argc, char *argv[]){

    po::options_description desc("Options");

    desc.add_options()
        ("help", "Produce help message")
        ("src_dir", po::value<std::string>(&CONFIG_SOURCE_DIR), "Directory to read legacy data from")
        ("tgt_dir", po::value<std::string>(&CONFIG_TARGET_DIR), "Directory to assemble new store in")
        ("accept", po::value<std::string>(&CONFIG_ACCEPT), "Accept only keys matching")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        exit(0);
    }

    if(CONFIG_SOURCE_DIR.empty() || CONFIG_TARGET_DIR.empty()){
        std::cerr << "You must specify both src_dir and tgt_dir" << std::endl;
        std::cerr << desc << std::endl;
        exit(1);
    }

}

int main(int argc, char *argv[]){
    config(argc, argv);
    std::vector<std::string> keys;

    const auto g = getdir(CONFIG_SOURCE_DIR, keys);
    assert( g == 0 );

    std::map<std::string, std::set<std::string>> files;
    for(const auto &k: keys){
        std::vector<std::string> _f;

        const std::string directory = CONFIG_SOURCE_DIR + "/" + k;
        const auto g = getdir(directory, _f);
        if( g != 0 ){
            std::cerr << "Error processing: " << directory << std::endl;
            continue;
        }

        for(const auto &f: _f){
            files[k].insert(f);
        }
    }

    for(const auto &file_list: files){
        //mkdir in target
        const std::string source_path = CONFIG_SOURCE_DIR + "/" + file_list.first;
        const std::string target_path = CONFIG_TARGET_DIR + "/" + file_list.first;

        //Can fail if directory already exists
        mkdir(target_path.c_str(), 0755);

        for(const auto &file: file_list.second){
            //read old file
            const std::string source_file = source_path + "/" + file;
            const std::string target_file = target_path + "/" + file;

            std::cerr << "Reading: " << source_file << std::endl;
            const std::string data = readfile(source_file, 0, 0);
            std::cerr << "Read: " << data.size() << std::endl;

            //parse
            legacy::Archive old_archive;
            old_archive.ParseFromString(data);

            std::cerr << "Writing file: " << target_file << std::endl;
            std::ofstream o(target_file, std::ofstream::binary);
            assert(o);

            for(const auto &value: old_archive.values()){
                const uint64_t timestamp = value.time();
                const std::string datagram = value.data();
                const uint64_t datagram_length = datagram.size();

                assert(o.write((char *)&timestamp, sizeof(uint64_t)));
                assert(o.write((char *)&datagram_length, sizeof(uint64_t)));
                assert(o.write(datagram.c_str(), datagram_length));

            }
            o.close();
        }
    }
    return 0;
}

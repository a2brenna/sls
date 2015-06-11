#include "index.h"
#include <boost/program_options.hpp>
#include <hgutil/files.h>

#include <vector>
#include <string>
#include <fstream>

std::string CONFIG_SLS_DIR;

namespace po = boost::program_options;

void config(int argc, char *argv[]){

    po::options_description desc("Options");

    try{
        desc.add_options()
            ("help", "Produce help message")
            ("sls_dir", po::value<std::string>(&CONFIG_SLS_DIR), "Directory to database from ")
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

    if(CONFIG_SLS_DIR.empty()){
        std::cerr << "You must specify an sls directory tree to operate on" << std::endl;
        std::cerr << desc << std::endl;
        exit(1);
    }

}

int main(int argc, char *argv[]){
    config(argc, argv);

    std::vector<std::string> keys;
    const auto m = getdir(CONFIG_SLS_DIR, keys);
    assert(m == 0);

    for(const auto k: keys){
        Index index = build_index(CONFIG_SLS_DIR + "/" + k);

        std::ofstream o(CONFIG_SLS_DIR + "/index", std::ofstream::out | std::ofstream::trunc );
        assert(o);

        o << index;
        o.close();
    }

    return 0;
}

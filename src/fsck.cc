#include <iostream>
#include <string>
#include <boost/program_options.hpp>

std::string CONFIG_ROOT_DIR = "/pool/sls";

namespace po = boost::program_options;

void config(const int &argc, const char *argv[]){
    try{
        po::options_description desc("Options");
        desc.add_options()
            ("help", "Produce help message")
            ("root_dir", po::value<std::string>(&CONFIG_ROOT_DIR), "Absolute PATH to root of sls storage directory")
            ;

    }
    catch(...){
        throw;
    }

}

int main(int argc, char *argv[]){
    (void)argc;
    (void)argv;

    return 0;
};

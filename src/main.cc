#include <iostream>
#include <info.hpp>
#include <argparse.hpp>
#include <appContext.hpp>
#include <gst/gst.h>

// cxt of the application
appContext::context cxt;


void parse_args(const int argc, char **argv) {

}


int main(int argc, char* argv[]) {
    std::cout << "VERSION: " << VERSION << std::endl;
    gst_init(&argc,&argv);
    parse_args(argc,argv);
}

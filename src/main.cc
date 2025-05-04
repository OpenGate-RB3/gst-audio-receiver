#include <iostream>
#include <info.hpp>
#include <argparse.hpp>
#include <appContext.hpp>
#include <gst/gst.h>


// cxt of the application
appContext::context cxt;


void parse_args(const int argc, char **argv) {
    argparse::ArgumentParser program("gst-audio-receiver",VERSION);
    program.add_description("utility to receive in an RTSP audio stream, and play it");
    program.add_argument("-location").required().help("Location of the stream: example rtsp://ip:port/ending");
    program.add_argument("-format").default_value("AAC").help("The format of which the audio stream is encoded in");
}


int main(int argc, char* argv[]) {
    std::cout << "VERSION: " << VERSION << std::endl;
    gst_init(&argc,&argv);
    parse_args(argc,argv);
}

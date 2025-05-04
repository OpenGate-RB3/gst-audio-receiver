//
// Created by andrew on 5/3/25.
//

#pragma once
#include <string>
#include <gst/gstelement.h>

namespace appContext {

    struct context {
        std::string streamLocation; // url of the stream location
        std::string inFormat; // MPEG, OPUS, etc
        GstElement * pipeLine;
        std::vector<GstElement *> elements;
    };
}

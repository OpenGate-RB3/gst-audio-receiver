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
    try {
        program.parse_args(argc, argv);
        cxt.streamLocation = program.get("-location");
    }catch(const std::exception& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << program << std::endl;
        exit(1);
    }
}

// pipeliner rtspsrc --> bindecode --> pulsesink

bool setup_pipeline(appContext::context &ctx) {
    GstElement *rtpsrc, *bindecode, *pulsesink;

    rtpsrc = gst_element_factory_make("rtspsrc","source");
    bindecode = gst_element_factory_make("bindecode","decoder");
    pulsesink = gst_element_factory_make("pulsesink","pulsesink");

    if (!rtpsrc || !bindecode || !pulsesink) {
        std::cerr << "Not all elements could be created" << std::endl;
        g_object_unref(cxt.pipeLine);
        return false;
    }
    g_object_set(G_OBJECT(rtpsrc),"location",cxt.streamLocation.c_str(),"latency",0,NULL);

    // add items to pipeline
    gst_bin_add_many(GST_BIN(cxt.pipeLine), rtpsrc, bindecode, pulsesink, NULL);

    // link items now
    if (!gst_element_link_many (rtpsrc, bindecode, pulsesink, NULL)) {
        std::cerr << "Could not link elements" << std::endl;
        g_object_unref(cxt.pipeLine);
        return false;
    }

    // for clean up later
    cxt.elements.push_back(rtpsrc);
    cxt.elements.push_back(bindecode);
    cxt.elements.push_back(pulsesink);
    return true;
}

static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    GMainLoop *loop = (GMainLoop *)data;
    // swap this to a switch for more message coverage like eos, warning, etc
    if ( GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
        gchar  *debug;
        GError *error;
        gst_message_parse_error(msg, &error, &debug);
        g_free(debug);
        g_printerr("Error: %s\n", error->message);
        g_error_free(error);
        g_main_loop_quit(loop);
    }
    return TRUE;
}

int main(int argc, char* argv[]) {
    std::cout << "VERSION: " << VERSION << std::endl;
    gst_init(&argc,&argv);
    parse_args(argc,argv);

    cxt.pipeLine = gst_pipeline_new("pipeline");
    setup_pipeline(cxt);

}

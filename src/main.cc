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

static void pad_added_handler(GstElement * src, GstPad * pad, gpointer user_data ) {
    std::cout << "Pad linking started" << std::endl;
    GstPad * sink_pad = gst_element_get_static_pad(cxt.elements[1], "sink");
    if (!g_str_has_prefix(GST_PAD_NAME(pad), "recv_rtp_src_")) {
        std::cerr << "Pad linking aborted not right type" << std::endl;
        gst_object_unref(sink_pad);
        return;
    }

    if (gst_pad_is_linked(sink_pad)) {
        std::cerr << "Pad linking , pad already linked" << std::endl;
        gst_object_unref(sink_pad);
        return;
    }

    auto ret = gst_pad_link(pad,sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        std::cerr << "Pad linking failed" << std::endl;
    }else {
        std::cout << "Pad link success" << std::endl;
    }
    gst_object_unref(sink_pad);
}


static void pad_added_decodebin_handler(GstElement * src, GstPad * pad, gpointer user_data ) {
    std::cout << "Pad linking from decodebin to pulse sink started" << std::endl;
    GstCaps *caps = gst_pad_get_current_caps(pad);
    const gchar *name = gst_structure_get_name(gst_caps_get_structure(caps, 0));
    gst_caps_unref(caps);

    // Only link audio
    if (!g_str_has_prefix(name, "audio/")) {
        std::cerr << "Decodebin pad is not audio, skipping" << std::endl;
        return;
    }
    GstPad * sink_pad = gst_element_get_static_pad(cxt.elements[3], "sink");
    if (gst_pad_is_linked(sink_pad)) {
        std::cerr << "Pad linking failed" << std::endl;
        gst_object_unref(sink_pad);
        return;
    }
    auto ret = gst_pad_link(pad,sink_pad);
    if (GST_PAD_LINK_FAILED(ret)) {
        std::cerr << "Failed to link decodebin to audioconvert" << std::endl;
    }else {
        std::cout << "Decodebin successfully linked to audioconvert" << std::endl;
    }
    gst_object_unref(sink_pad);
}

// pipeliner rtspsrc --> bindecode --> pulsesink
bool setup_pipeline(appContext::context &ctx) {
    GstElement *rtpsrc, *bindecode, *pulsesink, *audioconvert, *audioresample;

    rtpsrc = gst_element_factory_make("rtspsrc","source");
    bindecode = gst_element_factory_make("decodebin","decoder");
    pulsesink = gst_element_factory_make("pulsesink","pulsesink");
    audioconvert = gst_element_factory_make("audioconvert","converter");
    audioresample = gst_element_factory_make("audioresample","resample");

    if (!rtpsrc || !bindecode || !pulsesink || !audioconvert || !audioresample) {
        std::cerr << "Not all elements could be created" << std::endl;
        g_object_unref(cxt.pipeLine);
        return false;
    }
    g_object_set(G_OBJECT(rtpsrc),"location",cxt.streamLocation.c_str(),"latency",0,NULL);

    // add items to pipeline
    gst_bin_add_many(GST_BIN(cxt.pipeLine), rtpsrc, bindecode, audioconvert, audioresample, pulsesink, NULL);
    if (!gst_element_link_many (audioconvert,audioresample,pulsesink, NULL)) {
        std::cerr << "Failed to link audio chain" << std::endl;
        return false;
    };

    g_signal_connect(rtpsrc,"pad-added",G_CALLBACK(pad_added_handler),NULL);
    g_signal_connect(bindecode,"pad-added",G_CALLBACK(pad_added_decodebin_handler),NULL);

    // for clean up later
    cxt.elements.push_back(rtpsrc);
    cxt.elements.push_back(bindecode);
    cxt.elements.push_back(pulsesink);
    cxt.elements.push_back(audioconvert);
    cxt.elements.push_back(audioresample);
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

    // Create a GLib Main Loop
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    GstBus *bus = gst_element_get_bus(cxt.pipeLine);
    gst_bus_add_watch(bus, bus_callback, loop);
    gst_object_unref(bus); // we can unref now, watch holds it
    gst_element_set_state(cxt.pipeLine, GST_STATE_READY);
    GstStateChangeReturn ret = gst_element_set_state(cxt.pipeLine, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Unable to set the pipeline to the playing state." << std::endl;
        gst_object_unref(cxt.pipeLine);
        g_main_loop_unref(loop);
        return 1;
    }
    std::cout << "Running pipeline..." << std::endl;
    g_main_loop_run(loop);

    std::cout << "Pipeline finished, cleaning up..." << std::endl;
    gst_element_set_state(cxt.pipeLine, GST_STATE_NULL);
    gst_object_unref(cxt.pipeLine);
    for (const auto val : cxt.elements) {
        gst_object_unref(val);
    }
    cxt.elements.clear();
    g_main_loop_unref(loop);
    return 0;
}

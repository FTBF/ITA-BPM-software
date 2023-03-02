#include <zmq.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include "yaml-cpp/yaml.h"

int main () 
{
    YAML::Node config = YAML::LoadFile("config.yaml");

    std::string ip = config["ip"].as<std::string>();
    std::string ctrl_port = config["ctrl_port"].as<std::string>();
    std::string daq_port = config["daq_port"].as<std::string>();
    std::string file_name = config["file_name"].as<std::string>();
    int nEvts = config["num_triggers"].as<int>();
    int evts_per_trigger = config["reads_per_trigger"].as<int>();

    FILE *out_file = fopen(file_name.c_str(), "wb");

    //  Prepare our context and socket
    zmq::context_t context (2);
    zmq::socket_t ctrl_socket (context, zmq::socket_type::req);
    ctrl_socket.connect ("tcp://" + ip + ":" + ctrl_port);

    zmq::socket_t daq_socket (context, zmq::socket_type::pull);
    daq_socket.connect ("tcp://" + ip + ":" + daq_port);

    
    //send configuration file to daq board
    zmq::message_t initiateConfig ("config", 6);
    ctrl_socket.send (initiateConfig, zmq::send_flags::none);

    zmq::message_t ready_msg;
    //  Wait for next request from server
    ctrl_socket.recv (ready_msg, zmq::recv_flags::none);
    std::cout << "Received " << ready_msg << std::endl;
    
    std::stringstream cfg_ss;
    cfg_ss << config;
    zmq::message_t cfg_msg (cfg_ss.str().size());
    memcpy (cfg_msg.data(), cfg_ss.str().c_str(), cfg_ss.str().size());
    ctrl_socket.send (cfg_msg, zmq::send_flags::none);

    zmq::message_t configed_msg;
    //  Wait for next request from server
    ctrl_socket.recv (configed_msg, zmq::recv_flags::none);
    std::cout << "Received " << configed_msg << std::endl;

    //  Start daq taking based on current settings
    zmq::message_t reply (5);
    memcpy (reply.data (), "start", 5);
    ctrl_socket.send (reply, zmq::send_flags::none);

    zmq::message_t request;

    //  Wait for next request from client
    ctrl_socket.recv (request, zmq::recv_flags::none);
    std::cout << "Received " << request << std::endl;

    for(int i = 0; i < nEvts; ++i)
    {
        //  Wait for next request from client
        daq_socket.recv (request, zmq::recv_flags::none);

        fwrite(request.data(), sizeof(char), request.size(), out_file);
        fputc('\n', out_file);
    }

    fclose(out_file);

    return 0;
}

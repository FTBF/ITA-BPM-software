#include <zmq.hpp>
#include <string>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include "yaml-cpp/yaml.h"

namespace std
{
    template <>
    struct default_delete<FILE>
    {
        void operator () (FILE* file) const
            {
                fclose(file);
            }
    };
}

class DAQCtrl
{
public:
    DAQCtrl(zmq::context_t& context, YAML::Node config) : ctrl_socket_ (context, zmq::socket_type::req), daq_socket_ (context, zmq::socket_type::pull), out_file_(nullptr)
    {
        loadConfig(config);
        std::string ip = config["ip"].as<std::string>();
        std::string ctrl_port = config["ctrl_port"].as<std::string>();
        std::string daq_port = config["daq_port"].as<std::string>();

        ctrl_socket_.connect ("tcp://" + ip + ":" + ctrl_port);
        daq_socket_.connect ("tcp://" + ip + ":" + daq_port);
    }

    void loadConfig(YAML::Node config)
    {
        config_ = config;
        file_name_ = config["file_name"].as<std::string>();
        nEvts_ = config["num_triggers"].as<int>();
        evts_per_trigger_ = config["reads_per_trigger"].as<int>();
    }

    float readTemp(int chan)
    {
        if(chan < 1 || chan > 2)
        {
            std::cout << "Illegal temperature channel \"" << chan << "\"" << std::endl;
        }
        //read temperature
        zmq::message_t tempMsg ("temp", 4);
        ctrl_socket_.send (tempMsg, zmq::send_flags::none);
    
        zmq::message_t tempReply;
        ctrl_socket_.recv (tempReply, zmq::recv_flags::none);
        std::cout << "Received " << tempReply << std::endl;

        if(chan == 1) tempMsg.rebuild("1", 1);
        else          tempMsg.rebuild("2", 1);
        ctrl_socket_.send (tempMsg, zmq::send_flags::none);
    
        tempReply.rebuild();
        ctrl_socket_.recv (tempReply, zmq::recv_flags::none);
        std::cout << "Received " << tempReply << std::endl;

        //calculate temperature here
        float temperature = 1.0;

        return temperature;
    }

    void sendConfig()
    {
        //send configuration file to daq board
        zmq::message_t initiateConfig ("config", 6);
        ctrl_socket_.send (initiateConfig, zmq::send_flags::none);

        zmq::message_t ready_msg;
        //  Wait for next request from server
        ctrl_socket_.recv (ready_msg, zmq::recv_flags::none);
        std::cout << "Received " << ready_msg << std::endl;
    
        std::stringstream cfg_ss;
        cfg_ss << config_;
        zmq::message_t cfg_msg (cfg_ss.str().size());
        memcpy (cfg_msg.data(), cfg_ss.str().c_str(), cfg_ss.str().size());
        ctrl_socket_.send (cfg_msg, zmq::send_flags::none);

        zmq::message_t configed_msg;
        //  Wait for next request from server
        ctrl_socket_.recv (configed_msg, zmq::recv_flags::none);
        std::cout << "Received " << configed_msg << std::endl;
    }

    void startRun()
    {
        out_file_.reset(fopen(file_name_.c_str(), "wb"));
    
        //  Start daq taking based on current settings
        zmq::message_t reply (5);
        memcpy (reply.data (), "start", 5);
        ctrl_socket_.send (reply, zmq::send_flags::none);

        zmq::message_t request;

        //  Wait for next request from client
        ctrl_socket_.recv (request, zmq::recv_flags::none);
        std::cout << "Received " << request << std::endl;

        for(int i = 0; i < nEvts_; ++i)
        {
            //  Wait for next request from client
            daq_socket_.recv (request, zmq::recv_flags::none);

            fwrite(request.data(), sizeof(char), request.size(), out_file_.get());
            fputc('\n', out_file_.get());
        }

    }

    void stopRun()
    {
        //  Start daq taking based on current settings
        zmq::message_t reply ("stop", 4);
        ctrl_socket_.send (reply, zmq::send_flags::none);

        zmq::message_t request;

        //  Wait for next request from client
        ctrl_socket_.recv (request, zmq::recv_flags::none);
        std::cout << "Received " << request << std::endl;

    }

private:
    zmq::socket_t ctrl_socket_;
    zmq::socket_t daq_socket_;
    YAML::Node config_;
    std::string file_name_;
    int nEvts_;
    int evts_per_trigger_;
    std::unique_ptr<FILE> out_file_;
};

int main () 
{
    YAML::Node config = YAML::LoadFile("config.yaml");
    zmq::context_t context (2);

    DAQCtrl daq(context, config);

    daq.sendConfig();
    daq.startRun();

    return 0;
}

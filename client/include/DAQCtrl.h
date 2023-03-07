#ifndef DAQCtrl_H
#define DAQCtrl_H

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
    DAQCtrl(zmq::context_t& context, YAML::Node config) : ctrl_socket_ (context, zmq::socket_type::req), daq_socket_ (context, zmq::socket_type::pull), out_file_(nullptr), g_stop_(nullptr)
    {
        maxFileSize_ = 400000000;

        loadConfig(config);
        std::string ip = config["ip"].as<std::string>();
        std::string ctrl_port = config["ctrl_port"].as<std::string>();
        std::string daq_port = config["daq_port"].as<std::string>();

        ctrl_socket_.connect ("tcp://" + ip + ":" + ctrl_port);
        daq_socket_.connect ("tcp://" + ip + ":" + daq_port);
    }

    void setGStop(bool* g_stop)
    {
        g_stop_ = g_stop;
    }

    void loadConfig(YAML::Node config)
    {
        config_ = config;
        file_name_ = config["file_name"].as<std::string>();
        nEvts_ = config["num_triggers"].as<int>();
        evts_per_trigger_ = config["reads_per_trigger"].as<int>();
        if(config["maxFileSize"]) maxFileSize_ = config["maxFileSize"].as<int>();
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

    void setBias()
    {
        //read temperature
        zmq::message_t msg ("bias", 4);
        ctrl_socket_.send (msg, zmq::send_flags::none);

        zmq::message_t reply;

        //  Wait for next request from client
        ctrl_socket_.recv (reply, zmq::recv_flags::none);
        std::cout << "Received " << reply << std::endl;
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
        int file_size = 0;
        int file_num = 0;

        out_file_.reset(fopen((file_name_ + "_" + std::to_string(file_num) + ".dat").c_str(), "wb"));
        ++file_num;
    
        //  Start daq taking based on current settings
        zmq::message_t reply ("start", 5);
        ctrl_socket_.send (reply, zmq::send_flags::none);

        zmq::message_t request;

        //  Wait for next request from client
        ctrl_socket_.recv (request, zmq::recv_flags::none);
        std::cout << "Received " << request << std::endl;

        for(int i = 0; i < nEvts_ || nEvts_ < 0; ++i)
        {
            //  Wait for next request from client
            try
            {
                daq_socket_.recv (request, zmq::recv_flags::none);
            }
            catch(zmq::error_t e)
            {
                if(g_stop_ && !*g_stop_)
                {
                    std::cout << e.what() << std::endl;
                    continue;
                }
            }

            if(g_stop_ && *g_stop_) 
            {
                stopRun();
                break;
            }
            else
            {
                file_size += request.size();
                fwrite(request.data(), sizeof(char), request.size(), out_file_.get());
                fputc('\n', out_file_.get());

                if(file_size > maxFileSize_)
                {
                    out_file_.reset(fopen((file_name_ + "_" + std::to_string(file_num) + ".dat").c_str(), "wb"));
                    ++file_num;
                    file_size = 0;
                }
            }
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
    bool *g_stop_;
    int maxFileSize_;
};

#endif

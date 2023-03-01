#include "UIO.h"
#include "LTC2333.h"

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <thread>
#include <memory>
#include <unordered_map>
#include <functional>


class DAQ_Thread
{
public:
    DAQ_Thread(zmq::context_t& context) : data_socket_(context, zmq::socket_type::push), reads_per_trigger_(1024+1), num_triggers_(1000), nChan_(0), daq_thread_(nullptr)
    {
        data_socket_.bind ("tcp://*:5556");
    }

    void configure(uint8_t chipMask = 0xff, uint32_t read_period = 0, uint8_t range = 0, uint8_t chanMask = 0xff, uint32_t reads_per_trigger = 1024+1)
    {
        join_daq_thread();
        
        nChan_ = 0;
        for(int i = 0; i < 8; ++i)
        {
            if(chipMask & (1 << i)) nChan_ += 1;
        }

        reads_per_trigger_ = reads_per_trigger;

        ltc_.configure(chipMask, chanMask, range, 0, reads_per_trigger_);
        ltc_.setReadDepth(reads_per_trigger_);
        ltc_.setReadPeriod(read_period);
    }
    
    void start_daq_thread()
    {
        if(daq_thread_ && daq_thread_->joinable()) daq_thread_->join();
        daq_thread_.reset(new std::thread(daq_thread, this));
    }

    void join_daq_thread()
    {
        if(daq_thread_ && daq_thread_->joinable()) daq_thread_->join();
    }

private:
    LTC2333 ltc_;
    zmq::socket_t data_socket_;
    int reads_per_trigger_;
    int num_triggers_ = 1000;
    uint32_t chipMask_ = 0xff;
    int nChan_;

    std::unique_ptr<std::thread> daq_thread_;

    static void daq_thread(DAQ_Thread* self)
    {
        //  Do some 'work'
        while(self->ltc_.writeInProgress()) usleep(10);

        self->ltc_.reset();

        self->ltc_.enableRead(true);
    
        self->ltc_.trigger();
        while(!self->ltc_.fifoReady(0, self->reads_per_trigger_)) usleep(10);

        //DAQ loop
        for(int iTrig = 0; iTrig < self->num_triggers_; ++iTrig)
        {
            while(self->ltc_.writeInProgress());
            self->ltc_.trigger();

            self->ltc_.wait();

            //  read data and send to client
            zmq::message_t data (self->reads_per_trigger_*(self->nChan_+2)*sizeof(uint32_t));
            self->ltc_.read(reinterpret_cast<uint32_t*>(data.data())); 

            self->data_socket_.send (data, zmq::send_flags::none);
        }

        for(int i = 0; i < 9; ++i)
        {
            std::cout << "FIFOOcc: " << i << "\t" << self->ltc_.getFIFOOcc(i) << std::endl;
        }

        self->ltc_.enableRead(false);
    }

};

class Ctrl_Thread
{
public:
    Ctrl_Thread(zmq::context_t& context) : control_socket_ (context, zmq::socket_type::rep), daq_(context)
    {
        control_socket_.bind ("tcp://*:5555");

        chipMask_ = 0xff;
        read_period_ = 0;
        range_ = 0;
        chanMask_ = 0xff;
        reads_per_trigger_ = 1024+1;

        cmdMap_.emplace("start",  cmd_start);
        cmdMap_.emplace("stop",   cmd_stop);
        cmdMap_.emplace("config", cmd_config);
    }

    void parse_config(const YAML::Node& config)
    {
        if(config["chipMask"])          chipMask_          = config["chipMask"]          .as< decltype(chipMask_)          >();
        if(config["read_period"])       read_period_       = config["read_period"]       .as< decltype(read_period_)       >();
        if(config["range"])             range_             = config["range"]             .as< decltype(range_)             >();
        if(config["chanMask"])          chanMask_          = config["chanMask"]          .as< decltype(chanMask_)          >();
        if(config["reads_per_trigger"]) reads_per_trigger_ = config["reads_per_trigger"] .as< decltype(reads_per_trigger_) >();
    }

    void recieveCommand()
    {
        zmq::message_t request;

        //  Wait for next request from client
        control_socket_.recv (request, zmq::recv_flags::none);

        auto iter = cmdMap_.find(request.to_string());
        if(iter != cmdMap_.end())
        {
            iter->second(this);
        }
        else
        {
            std::cout << "Unknown command " << request.to_string() << std::endl;
            //  Send reply back to client
            zmq::message_t reply ("Unknowncmd", 10);
            control_socket_.send (reply, zmq::send_flags::none);
        }

    }
private:
    zmq::socket_t control_socket_;
    uint8_t chipMask_;
    uint32_t read_period_;
    uint8_t range_;
    uint8_t chanMask_;
    uint32_t reads_per_trigger_;
    std::unordered_map<std::string, std::function<void(Ctrl_Thread*)>> cmdMap_;
    DAQ_Thread daq_;

    static void cmd_start(Ctrl_Thread* self)
    {
        //don't configure until ongoing run is finished
        self->daq_.join_daq_thread();
        self->daq_.configure(self->chipMask_, self->read_period_, self->range_, self->chanMask_, self->reads_per_trigger_);

        //  Send reply back to client
        zmq::message_t reply ("Runing", 6);
        self->control_socket_.send (reply, zmq::send_flags::none);

        self->daq_.start_daq_thread();
    }

    static void cmd_stop(Ctrl_Thread* self)
    {
        //  Send reply back to client
        zmq::message_t reply ("Stopped", 7);
        self->control_socket_.send (reply, zmq::send_flags::none);
        
    }

    static void cmd_config(Ctrl_Thread* self)
    {
        //  Send reply back to client
        zmq::message_t reply ("Ready", 5);
        self->control_socket_.send (reply, zmq::send_flags::none);

        zmq::message_t request;

        //  Wait for config dataw from client
        self->control_socket_.recv (request, zmq::recv_flags::none);

        self->parse_config(YAML::Load(request.to_string()));

        //  Send reply back to client
        zmq::message_t reply2 ("Configured", 10);
        self->control_socket_.send (reply2, zmq::send_flags::none);

    }
};

int main () 
{
    //LED control gpio
    UIO gpio("axi-gpio-0");
    gpio[0]=000;

    //  Prepare our context and socket
    zmq::context_t context (2);

    Ctrl_Thread daq(context);

    while (true) 
    {
        daq.recieveCommand();
    }
    return 0;
}

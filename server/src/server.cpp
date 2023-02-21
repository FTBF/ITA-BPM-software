#include "UIO.h"
#include "LTC2333.h"
#include "i2c.h"

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <unistd.h>
#include <yaml-cpp/yaml.h>

#include <thread>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <functional>

class DAQ_Thread
{
public:
    DAQ_Thread(zmq::context_t& context) : data_socket_(context, zmq::socket_type::push), reads_per_trigger_(1024+1), num_triggers_(1000), nChan_(0), daq_thread_(nullptr), stop_(false)
    {
        data_socket_.bind ("tcp://*:5556");
    }

    void configure(uint32_t numTrigger = 1000, uint8_t chipMask = 0xff, uint32_t read_period = 0, uint8_t range = 0, uint8_t chanMask = 0xff, uint32_t reads_per_trigger = 1024+1)
    {
        join_daq_thread();
        
        nChan_ = 0;
        for(int i = 0; i < 8; ++i)
        {
            if(chipMask & (1 << i)) nChan_ += 1;
        }

        reads_per_trigger_ = reads_per_trigger;
        num_triggers_ = numTrigger;

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

    void stop_daq_thread()
    {
        std::scoped_lock(stop_mtx_);
        stop_ = true;
    }

private:
    LTC2333 ltc_;
    zmq::socket_t data_socket_;
    int reads_per_trigger_;
    int num_triggers_ = 1000;
    uint32_t chipMask_ = 0xff;
    int nChan_;

    std::unique_ptr<std::thread> daq_thread_;
    std::mutex stop_mtx_;
    bool stop_;

    static void daq_thread(DAQ_Thread* self)
    {
        //  Do some 'work'
        while(self->ltc_.writeInProgress()) usleep(10);

        self->ltc_.reset();

        self->ltc_.enableRead(true);
    
        self->ltc_.trigger();
        while(!self->ltc_.fifoReady(0, self->reads_per_trigger_)) usleep(10);

        //DAQ loop
        std::cout << "N events: " << self->num_triggers_ << std::endl;
        for(int iTrig = 0; iTrig < self->num_triggers_; ++iTrig)
        {
            {
                std::scoped_lock(self->stop_mtx_);
                self->stop_ = false;
                if(self->stop_) break;
            }
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
    Ctrl_Thread(zmq::context_t& context) : control_socket_ (context, zmq::socket_type::rep), daq_(context), i2c_1("/dev/i2c-1"), i2c_2("/dev/i2c-2")
    {
        control_socket_.bind ("tcp://*:5555");

        chipMask_ = 0xff;
        read_period_ = 0;
        range_ = 0;
        chanMask_ = 0xff;
        reads_per_trigger_ = 1024+1;
        num_triggers_ = 1000;
    
        cmdMap_.emplace("start",  cmd_start);
        cmdMap_.emplace("stop",   cmd_stop);
        cmdMap_.emplace("config", cmd_config);
        cmdMap_.emplace("temp",   cmd_readTemp);
    }

    void parse_config(const YAML::Node& config)
    {
        if(config["chipMask"])          chipMask_          = config["chipMask"]          .as< decltype(chipMask_)          >();
        if(config["read_period"])       read_period_       = config["read_period"]       .as< decltype(read_period_)       >();
        if(config["range"])             range_             = config["range"]             .as< decltype(range_)             >();
        if(config["chanMask"])          chanMask_          = config["chanMask"]          .as< decltype(chanMask_)          >();
        if(config["reads_per_trigger"]) reads_per_trigger_ = config["reads_per_trigger"] .as< decltype(reads_per_trigger_) >();
        if(config["num_triggers"])      num_triggers_      = config["num_triggers"]      .as< decltype(num_triggers_)      >();
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
            std::cout << "Unknown command \"" << request.to_string() << "\"" << std::endl;
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
    uint32_t num_triggers_;
    std::unordered_map<std::string, std::function<void(Ctrl_Thread*)>> cmdMap_;
    DAQ_Thread daq_;
    I2C i2c_1, i2c_2;

    static void cmd_start(Ctrl_Thread* self)
    {
        //don't configure until ongoing run is finished
        self->daq_.join_daq_thread();
        self->daq_.configure(self->num_triggers_, self->chipMask_, self->read_period_, self->range_, self->chanMask_, self->reads_per_trigger_);

        self->daq_.start_daq_thread();

        //  Send reply back to client
        zmq::message_t reply ("Runing", 6);
        self->control_socket_.send (reply, zmq::send_flags::none);
    }

    static void cmd_stop(Ctrl_Thread* self)
    {
        self->daq_.stop_daq_thread();
        self->daq_.join_daq_thread();

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

    static void cmd_readTemp(Ctrl_Thread* self)
    {
        //  Send reply back to client
        zmq::message_t reply ("Ready", 5);
        self->control_socket_.send (reply, zmq::send_flags::none);

        zmq::message_t message;

        //  Wait for channel number from client
        self->control_socket_.recv (message, zmq::recv_flags::none);

        
        if(message.data<char>()[0] != '1' && message.data<char>()[0] != '2')
        {
            std::cout << "Invalid temperature channel number: " << message.data<char>()[0] << std::endl;
            zmq::message_t reply ("Error", 5);
            self->control_socket_.send (reply, zmq::send_flags::none);
        }

        //read temperature sensors via i2c
        const uint8_t addr = 0x68;

        I2C *i2c = nullptr;

        if(message.data<char>()[0] != '1')
        {
            i2c = &self->i2c_1;
        }
        else if(message.data<char>()[0] != '2')
        {
            i2c = &self->i2c_2;
        }

        //read first value
        //write config byte and start conversion
        i2c->write(addr, {0x8c});

        std::vector<uint8_t> result;
        do
        {
            result = i2c->read(addr, 4);
        } while(result[3] & 0x80); //check for ~Ready bit

        for(const auto& r : result) printf("%x ", r); printf("\n");
        float voltage1 = float(int( ((result[0]&0x2)?0xfffe0000:0) | uint32_t((result[0]&0x1) << 16) | uint32_t(result[1] << 8) | uint32_t(result[2])))*2.048/(1<<17);

        //read second value
        //write config byte and start conversion
        i2c->write(addr, {0xac});

        result;
        do
        {
            result = i2c->read(addr, 4);
        } while(result[3] & 0x80); //check for ~Ready bit

        for(const auto& r : result) printf("%x ", r); printf("\n");
        float voltage2 = float(int( ((result[0]&0x2)?0xfffe0000:0) | uint32_t((result[0]&0x1) << 16) | uint32_t(result[1] << 8) | uint32_t(result[2])))*2.048/(1<<17);

        //  Send reply back to client
        std::string voltageStr = std::to_string(voltage1) + " " + std::to_string(voltage2);
        zmq::message_t voltageMsg (voltageStr.c_str(), voltageStr.size());
        self->control_socket_.send (voltageMsg, zmq::send_flags::none);
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

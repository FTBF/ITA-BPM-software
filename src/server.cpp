#include "UIO.h"
#include "LTC2333.h"

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <unistd.h>


#include <thread>
#include <memory>

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

int main () 
{
    //LED control gpio
    UIO gpio("axi-gpio-0");
    gpio[0]=000;

    //  Prepare our context and socket
    zmq::context_t context (2);
    zmq::socket_t control_socket (context, zmq::socket_type::rep);
    control_socket.bind ("tcp://*:5555");

    DAQ_Thread daq(context);

    while (true) 
    {
        zmq::message_t request;

        //  Wait for next request from client
        control_socket.recv (request, zmq::recv_flags::none);
        std::cout << "Received Hello" << std::endl;

        daq.configure();

        //  Send reply back to client
        zmq::message_t reply (5);
        memcpy (reply.data (), "World", 5);
        control_socket.send (reply, zmq::send_flags::none);

        daq.start_daq_thread();

        daq.join_daq_thread();
    }
    return 0;
}

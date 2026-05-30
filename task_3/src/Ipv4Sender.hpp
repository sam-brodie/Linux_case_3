#ifndef INTERVIEW_UDP_TASK_HPP
#define INTERVIEW_UDP_TASK_HPP

#include <cstdint>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <iostream>

class Ipv4Sender {
public:
    static const int ERR_DATA_TOO_LONG = -1;
    static const int ERR_INVALID_ADDRESS = -2;
    static const int ERR_INVALID_PERIOD = -3;
    static const int ERR_NULL_BUFFER = -4;
    static const int ERR_SOCKET_ERROR = -5;
    static const int STATUS_OK = 0;

    Ipv4Sender(const size_t max_buf_size);
    ~Ipv4Sender();

    // send emmediatly a measege over udp to specific ip&port
    ssize_t send_immediately_ipv4(
        std::shared_ptr<const std::vector<uint8_t>> databuf,
        const std::string& dest_ip, 
        const uint16_t dest_port);

    // send with a non blocking request a udp message to specific ip&port after X seconds' delay (where X could be any value between 1s and 255s)
    int send_after_delay_nonblocking_ipv4(
        std::shared_ptr<const std::vector<uint8_t>> databuf,
        const std::string dest_ip,
        const uint16_t dest_port,
        const uint8_t delay);

    //request periodic sending of a udp message to specific ip& port every  X seconds. (where X could be any value between 1s and 255s)
    int send_periodic_nonblocking_ipv4(
        std::shared_ptr<const std::vector<uint8_t>> databuf,
        const std::string dest_ip,
        const uint16_t dest_port,
        const uint8_t period);

private:
    int sockfd = -1;
    const size_t max_buf_size;
    std::atomic<bool> running = { false };
    std::thread scheduling_thread;
    std::mutex scheduled_msgs_mutex;
    std::condition_variable queue_cond_var;

    struct UdpMsg {
        std::shared_ptr <const std::vector<uint8_t>> databuf; //data to send
        std::string dest_ip;
        uint16_t dest_port;
        uint8_t repeat_interval_sec; // how many seconds to wait between repeating sends. 0 for no repeat
        std::chrono::steady_clock::time_point trigger_time; // when should this message be sent
    };

    struct CompareMsgPriority {
        bool operator()(const UdpMsg& a, const UdpMsg& b) const {
            return a.trigger_time > b.trigger_time;
        }// returns the most urgent message 
    };

    std::priority_queue<UdpMsg,
        std::vector<UdpMsg>,
        CompareMsgPriority> scheduled_msgs;

    void add_to_schedule_queue(UdpMsg msg);
    void scheduler_loop();
};


#endif // INTERVIEW_UDP_TASK_HPP

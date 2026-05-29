#ifndef INTERVIEW_UDP_TASK_HPP
#define INTERVIEW_UDP_TASK_HPP

#include <cstdint>
#include <iostream>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdio>



class Ipv4Sender {
public:
    static const int ERR_DATA_TOO_LONG = -1;
    static const int ERR_INVALID_ADDRESS = -2;
    static const int ERR_INVALID_PERIOD = -3;
    static const int STATUS_OK = 0;

    Ipv4Sender(const size_t max_buf_size);
    ~Ipv4Sender();

    // send emmediatly a measege over udp to specific ip&port
    size_t send_immediately_ipv4(std::shared_ptr<const std::vector<uint8_t>> databuf, const std::string& dest_ip, const uint16_t dest_port);

    // send with a non blocking request a udp message to specific ip&port after X seconds' delay (where X could be any value between 1s and 255s)
    int send_after_delay_nonblocking_ipv4(std::shared_ptr<const std::vector<uint8_t>> databuf, const std::string dest_ip, const uint16_t dest_port, const uint8_t delay);

    //request periodic sending of a udp message to specific ip& port every  X seconds. (where X could be any value between 1s and 255s)
    int send_periodic_nonblocking_ipv4(std::shared_ptr<const std::vector<uint8_t>> databuf, const std::string dest_ip, const uint16_t dest_port, const uint8_t period);

private:
    int sockfd = -1;
    const size_t max_buf_size;
    std::atomic<bool> running = { false };
    std::vector<std::thread> periodic_threads;
    std::vector<std::thread> delayed_send_threads;
};


#endif // INTERVIEW_UDP_TASK_HPP

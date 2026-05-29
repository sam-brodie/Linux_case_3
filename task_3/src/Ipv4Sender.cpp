#include"Ipv4Sender.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>


Ipv4Sender::Ipv4Sender(const size_t max_buf_size)
    : max_buf_size(max_buf_size)
{
    sockfd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
    if (sockfd < 0)
    {
        fprintf(stderr, "Could not create socket\n");
        throw std::runtime_error("Could not create socket");
    }
    running = true;
}

Ipv4Sender::~Ipv4Sender()
{
    // join all threads, close socket, etc
    running = false;
    for (auto& t : periodic_threads)
        if (t.joinable())
            t.join();

    for (auto& t : delayed_send_threads)
        if (t.joinable())
            t.join();

    if (sockfd >= 0)
        close(sockfd);
}


// send emmediatly a measege over udp to specific ip&port
size_t Ipv4Sender::send_immediately_ipv4(std::shared_ptr<const std::vector<uint8_t>> databuf, const std::string& dest_ip, const uint16_t dest_port)
{
    if (databuf->size() > max_buf_size) {
        return ERR_DATA_TOO_LONG;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);

    if (inet_pton(AF_INET, dest_ip.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid IP address\n";
        return ERR_INVALID_ADDRESS;
    }

    int flags = 0;
    size_t sent = sendto(sockfd, databuf->data(), databuf->size(), flags,
        reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    if (sent < 0)
    {
        std::cerr << "Failed to send UDP packet\n";
        return -1;
    }

    return sent;
}


// send with a non blocking request a udp message to specific ip&port after X seconds' delay (where X could be any value between 1s and 255s)
int Ipv4Sender::send_after_delay_nonblocking_ipv4(std::shared_ptr<const std::vector<uint8_t>> databuf, const std::string dest_ip, const uint16_t dest_port, const uint8_t delay)
{
    // start a thread that will wait X seconds, send the data, then exit
    delayed_send_threads.push_back(std::thread(
        [this, databuf, dest_ip, dest_port, delay]()
        {
            std::this_thread::sleep_for(std::chrono::seconds(delay));
            this->send_immediately_ipv4(databuf, dest_ip, dest_port);
        }));

    return STATUS_OK;
}

//request periodic sending of a udp message to specific ip& port every  X seconds. (where X could be any value between 1s and 255s)
int Ipv4Sender::send_periodic_nonblocking_ipv4(std::shared_ptr<const std::vector<uint8_t>> databuf, const std::string dest_ip, const uint16_t dest_port, const uint8_t period)
{
    if (period == 0)
        return ERR_INVALID_PERIOD;

    // start a thread that will wait X seconds, send the data, then repeat
    periodic_threads.push_back(std::thread(
        [this, databuf, dest_ip, dest_port, period]()
        {
            while (this->running)
            {
                this->send_after_delay_nonblocking_ipv4(databuf, dest_ip, dest_port, period);
                std::this_thread::sleep_for(std::chrono::seconds(period));
            }

        }));

    return STATUS_OK;
}
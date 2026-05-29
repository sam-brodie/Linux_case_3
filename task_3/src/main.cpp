#include "Ipv4Sender.hpp"

#include <string>
#include <chrono>
#include <thread>
#include <vector>


int main(int argc, char* argv[])
{
    std::string dest_ip = "127.0.0.1";
    uint16_t dest_port = 4321;
    auto data = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{1 ,2 , 3, 4 , 5});

    auto udp_sender = Ipv4Sender(500);

    udp_sender.send_immediately_ipv4(data, dest_ip, dest_port);
    udp_sender.send_after_delay_nonblocking_ipv4(data, dest_ip, dest_port, 2);
    udp_sender.send_periodic_nonblocking_ipv4(data , dest_ip, dest_port, 1);

    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}

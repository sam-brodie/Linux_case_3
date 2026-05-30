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
    scheduling_thread = std::thread(&Ipv4Sender::scheduler_loop, this);
}

Ipv4Sender::~Ipv4Sender()
{
    // join all threads, close socket, etc
    running = false;
    queue_cond_var.notify_one();

    if (scheduling_thread.joinable())
        scheduling_thread.join();

    if (sockfd >= 0)
        close(sockfd);
}


// send emmediatly a measege over udp to specific ip&port
ssize_t Ipv4Sender::send_immediately_ipv4(
    std::shared_ptr<const std::vector<uint8_t>> databuf,
    const std::string& dest_ip,
    const uint16_t dest_port)
{
    if (!databuf)
        return ERR_NULL_BUFFER;
    if (databuf->size() > max_buf_size)
        return ERR_DATA_TOO_LONG;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);

    if (inet_pton(AF_INET, dest_ip.c_str(), &addr.sin_addr) <= 0)
    {
        std::cerr << "Invalid IP address\n";
        return ERR_INVALID_ADDRESS;
    }

    int flags = 0;
    ssize_t sent = sendto(sockfd, databuf->data(), databuf->size(), flags,
        reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    if (sent < 0)
    {
        std::cerr << "Failed to send UDP packet" << std::endl;
        return ERR_SOCKET_ERROR;
    }

    return sent;
}

// send with a non blocking request a udp message to specific ip&port after X seconds' delay (where X could be any value between 1s and 255s)
int Ipv4Sender::send_after_delay_nonblocking_ipv4(
    std::shared_ptr<const std::vector<uint8_t>> databuf,
    const std::string dest_ip,
    const uint16_t dest_port,
    const uint8_t delay)
{
    if (delay == 0)
        return ERR_INVALID_PERIOD;
    if (!databuf)
        return ERR_NULL_BUFFER;

    UdpMsg msg{
            databuf,
            dest_ip,
            dest_port,
            0, // no repeating
            std::chrono::steady_clock::now() + std::chrono::seconds(delay)
    };

    add_to_schedule_queue(std::move(msg));

    return STATUS_OK;
}

//request periodic sending of a udp message to specific ip& port every  X seconds. (where X could be any value between 1s and 255s)
int Ipv4Sender::send_periodic_nonblocking_ipv4(
    std::shared_ptr<const std::vector<uint8_t>> databuf,
    const std::string dest_ip,
    const uint16_t dest_port,
    const uint8_t period)
{
    if (period == 0)
        return ERR_INVALID_PERIOD;
    if (!databuf)
        return ERR_NULL_BUFFER;

    UdpMsg msg{
            databuf,
            dest_ip,
            dest_port,
            period,
            std::chrono::steady_clock::now() + std::chrono::seconds(period)
    };

    add_to_schedule_queue(std::move(msg));

    return STATUS_OK;
}


void Ipv4Sender::add_to_schedule_queue(UdpMsg msg)
{
    {
        std::lock_guard<std::mutex> lock(scheduled_msgs_mutex);
        scheduled_msgs.push(std::move(msg)); //consider std::move()
    }
    queue_cond_var.notify_one();
}

void Ipv4Sender::scheduler_loop()
{
    std::unique_lock<std::mutex> lock(scheduled_msgs_mutex);

    while (running)
    {
        // If there are no messages waiting then wait until there is one or running is set to false
        if (scheduled_msgs.empty())
        {
            // Equivalent to
            //    while (!running || !scheduled_msgs.empty())
            //        wait(lock);
            queue_cond_var.wait(lock, [&] {
                return !running || !scheduled_msgs.empty();
                });

            if (!running)
                return;
        }


        while (running && !scheduled_msgs.empty())
        {
            auto next_time = scheduled_msgs.top().trigger_time;

            // wait until "next_time" or until this thread is notified
            if (queue_cond_var.wait_until(lock, next_time) != std::cv_status::timeout)
            {
                // we get here if the thread is notified before the next message is ready to send
                // this is because another message is added to the queue. It might be the new most
                // urgent message so we continue the loop and re-take the time from top of the 
                // queue in case it has changed.
                if (!running)
                    return;

                continue;
            }

            // the wait_until waited until the "next_time" trigger time without the thread being
            // notified (due to new messages being added to the queue)
            // This means that it is now "next_time" and so we should send the message
            UdpMsg msg = scheduled_msgs.top();
            scheduled_msgs.pop();

            lock.unlock();
            send_immediately_ipv4(msg.databuf, msg.dest_ip, msg.dest_port);
            lock.lock();

            if (msg.repeat_interval_sec > 0)
            {
                msg.trigger_time += std::chrono::seconds(msg.repeat_interval_sec);
                scheduled_msgs.push(std::move(msg));
                queue_cond_var.notify_one();
            }
        } //while (running && !scheduled_msgs.empty())
    } // while (running)
}
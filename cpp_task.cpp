// Q: What is the code doing?
// A: The main thread spawns two more threads. Each threads performs some dumy process repeatedly until one of the following conditions is met:
// 1. The process returns true (aborted)
// 2. The thread has run for more than 10 seconds.
// 3. The main thread sets the running variable to false.
// Condition 2 and 3 are checked after each call to Process has completed. Proccess() is not interupted.

// Q: Are there any issues, and if so how to fix them?
// A: 8 issues are found and they are detailed in comments below.


#include <chrono>
#include <atomic>
// ISSUE 1: <memory> is included but not used.
#include <memory>
#include <thread>
#include <functional>
#include <iostream>

using namespace std::chrono_literals;

void StartThread(
    std::thread& thread,
    std::atomic<bool>& running,
    const std::function<bool(void)>& Process,
    const std::chrono::seconds timeout)
{
    // ISSUE 2: if thread was already an active thread running something, then starting a new thread on it will cause issues.
    thread = std::thread(
        [&] ()
        {
            // ISSUE 3:
            // Process is a lamda function and it is passed by refference. Therefore, because the lifetime 
            // of the thread is longer than the lifetime of the StartThread function, the thread trys to call 
            // something invalid. Try [&running, Process, timeout]()
            // Maybe smart pointer would have managed the scoping issue e.g. std::shared_ptr<function<bool(void)>>

            // ISSUE 4:
            // consider using std::chrono::steady_clock instead of std::chrono::high_resolution_clock
            // high_resolution_clock may give better precision than steady_clock, but it can be changed (forwards or
            // backwards) outside of this program. steady_clock always counts up regardless of the system time/
            // Consider start is set at 12:00:01, then one second later the system time is changed to 00:00:01. The
            // timeout would take 12 hours
            auto start = std::chrono::high_resolution_clock::now();
            while(running)
            {
                // ISSUE 5:
                // Process() is blocking and the check for the timeout only happens if the function returns (true or false).
                // the 10s timeout will not neccessarily stop the thread after 10 seconds. It will stop the thread after 
                // Proccess() returns if the time has gone over 10 seconds e.g. Process() takes 3 seconds to return, 
                // the thread will timeout after 12 seconds. If Process() takes 11+ seconds to return, the thread will 
                // timeout after 11 seconds.
                // Similar for running being set to false, the current call to Process() will not be interupted.

                bool aborted = Process();
                
                auto end = std::chrono::high_resolution_clock::now();
                // ISSUE 6:
                // I think this is fine code but is a bit confusing to read that it is cast to milliseconds when the timeout is type seconds
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
                if (aborted || duration > timeout)
                {
                    running = false;
                    break;
                }
            }
        });
}

int main(int argc, char **argv)
{
    std::atomic<bool> my_running = true;
    std::thread my_thread1, my_thread2;
    int loop_counter1 = 0, loop_counter2 = 0;

    // start actions in seprate threads and wait of them

    StartThread(
        my_thread1,
        my_running, 
        [&]()
        {
            // "some actions" simulated with waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            // ISSUE 7:
            // loop_counters are not atomic/using mutex, however increment is an atomic function and they are not used by multiple threads at a time 
            loop_counter1++; 
            return false;
        },
        10s); // loop timeout

    StartThread(
        my_thread2,
        my_running, 
        [&]()
        {
            // "some actions" simulated with waiting 
            if (loop_counter2 < 5)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                loop_counter2++;
                return false;
            }
            return true;
        },
        10s); // loop timeout

    // ISSUE 8:
    // not checking threads are .joinable() before calling join()
    // if(my_thread1.joinable())
    //     my_thread1.join();
    // if(my_thread2.joinable())
    //     my_thread2.join();
    my_thread1.join();
    my_thread2.join();

    // print execlution loop counters
    std::cout << "C1: " << loop_counter1 << " C2: " << loop_counter2 << std::endl;
}


#include <iostream>
#include <thread>
#include <chrono>

#include "network.h"

// st for single threaded
namespace st {

    const int MAX_BUFFER_SIZE = 1024;
    const int MAX_RETRIES = 3;
    const int POLL_INTERVAL_MS = 100; // in milli seconds

    /*
    EventSubscriber that uses 
    1 async call : fetch_event_async(event_id, output_buf)
    2 sync apis: bool poll_get_last_status() and does_event_exist(event_id))


    This is a straight forward EventSubscriber implementation.
    Both function APIS are NOT thread safe.

    As the client is single threadead only.

    There is NOT much you can do to improve the throughput if SYNC APIs
    are used inconjunction with Async API, as:
        - Multi threading is NOT allowed
        - call backs are NOT allowed
        - caller expects event id iN ORDER

    */
    class EventSubscriber {
    public:

         EventSubscriber() {}

        ~EventSubscriber() {}

        bool setup_stream(int start_id) {
            m_start_event_id = start_id; // this is for statistics
            m_next_event_id = start_id; 
            return true;
        }

        /*
            1. check if the next event id exists
            2. if exists, call fetch_event_async
            3. check the status in a loop
                - if RECEIVED, return 
                - if FAILED, try retries
                - IF IN_PROGRESS, wait for sometime and check status.
                -- currently, simply uses sleep_for function. 
                -- but performance could be improved if a conditional_Variabale wth timeout is used,
                        but unnecessarly need to use unique_lock<mutex>, so avoiding that overhead.

        */
        bool receive(char* buff){

            while (1) {

                if (!does_event_exist(m_next_event_id)){
                    std::cerr << "Fail: Event does not exist. Id: " << m_next_event_id << "\n"; 
                    return false;
                }

                // bool. positive means good.
                if (fetch_event_async(m_next_event_id, m_output_buffer)){

                    bool fetch_completed = false;
                    int retries = MAX_RETRIES;

                    while (!fetch_completed && retries > 0) {
                        
                        Status status = poll_last_status();
                        
                        if (status == Status::RECEIVED) {

                            std::memcpy(buff, m_output_buffer, MAX_BUFFER_SIZE);
                            m_next_event_id++; // increase the next 
                            fetch_completed = true;
                            return true;
                 
                        } 
                        else if (status == Status::FAILED) {
                            retries--;
                            if (retries < 0) {
                                std::cerr << "Fail: Event fetch receive. Id: "<<m_next_event_id <<"\n";
                                return false;
                            }
                        }
                        // this means IN_PROGRESS status. so wait.
                        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS)); 
                    }
                } else {
                    std::cerr << "Fail: Event fetch Send. Id: " << m_next_event_id << "\n"; 
                    return false;
                }
            }
        }

    private:
        int m_start_event_id;
        int m_next_event_id;
        char m_output_buffer[MAX_BUFFER_SIZE];
    };
}


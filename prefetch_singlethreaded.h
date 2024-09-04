
#include <iostream>
#include <thread>
#include <chrono>

#include "network.h"

// pst for prefetch single threaded
namespace pst {

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

            if (!does_event_exist(start_id)){
                return false;
            }
            m_start_event_id = start_id; // this is for statistics
            m_next_event_id = start_id; 

            // start pre-fetching
            prefetch_next_event();

            return true;
        }


        bool receive(char* buff){

            // Check if a prefetched event is ready

            if (m_prefetch_in_progress) {

                Status status = poll_last_status();

                if (status == Status::RECEIVED) {
                    // Prefetched event is ready, copy the buffer
                    std::memcpy(buff, m_prefetch_buffer, MAX_BUFFER_SIZE);
                    m_next_event_id++; // Move to the next event
                    m_prefetch_in_progress = false; // Reset prefetch flag

                    // Prefetch the next event
                    prefetch_next_event();

                    return true;
                } else if (status == Status::FAILED) {
                    m_prefetch_in_progress = false;
                    std::cerr << "Fail: Prefetch receive failed. Id: " << m_next_event_id << "\n";
                    return false;
                }
                // If IN_PROGRESS, we fall through and fetch the current event synchronously
            }

            // Fetch the current event if the prefetch is not ready
            return fetch_current_event(buff);


        }


    private:
        int m_start_event_id;

        int m_next_event_id;
        char m_output_buffer[MAX_BUFFER_SIZE];

        char m_prefetch_buffer[MAX_BUFFER_SIZE];
        bool   m_prefetch_in_progress;


        void prefetch_next_event() {

            if (does_event_exist(m_next_event_id)) {

                if (fetch_event_async(m_next_event_id, m_prefetch_buffer)) {
                    m_prefetch_in_progress = true; // Prefetch initiated
                } else {
                    std::cerr << "Fail: Event prefetch send failed. Id: " << m_next_event_id << "\n";
                    m_prefetch_in_progress = false;
                }
            } else {
                std::cerr << "Fail: Event does not exist for prefetch. Id: " << m_next_event_id << "\n";
                m_prefetch_in_progress = false;
            }
        }


        bool fetch_current_event(char* buff) {

            if (!does_event_exist(m_next_event_id)) {
                std::cerr << "Fail: Event does not exist. Id: " << m_next_event_id << "\n";
                return false;
            }

            if (fetch_event_async(m_next_event_id, m_output_buffer)) {
                bool fetch_completed = false;
                int retries = MAX_RETRIES;

                while (!fetch_completed && retries > 0) {
                    Status status = poll_last_status();

                    if (status == Status::RECEIVED) {
                        std::memcpy(buff, m_output_buffer, MAX_BUFFER_SIZE);
                        m_next_event_id++; // Move to the next event
                        fetch_completed = true;

                        // Prefetch the next event after successfully fetching the current one
                        prefetch_next_event();
                        return true;

                    } else if (status == Status::FAILED) {
                        retries--;
                        if (retries == 0) {
                            std::cerr << "Fail: Event fetch receive failed. Id: " << m_next_event_id << "\n";
                            return false;
                        }
                    }

                    // If the fetch is still in progress, wait and retry
                    std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
                }
            } else {
                std::cerr << "Fail: Event fetch send failed. Id: " << m_next_event_id << "\n";
                return false;
            }

            return false;
        }
    };
}



    };
}


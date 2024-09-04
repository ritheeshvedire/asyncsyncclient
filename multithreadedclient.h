#include <iostream>
#include <unordered_map>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <atomic>
#include <chrono>

#include "network.h"


/*
 simply there are worker threads, that prefetch the data.
 this can be done in 2 ways:
 1. first way
    - Each worker threads has a range of event ids to fetch.
    -  For eamples, for 3 workers start_id, start_id+10, start_id+20
    - they insert the returned event buffs in a map
     - receive() will only return events in order by quering this map. If a particular event in order is NOT available, return false

 2. Second way
    - there is atomic counter next_fetch_event_id
    - multiple workers will update next_fetch_event_id + 1 and use that to get fetch event from the server
    - receive() wil have a seperate counter that only queries next ordered event from the map

We do second way abnove for simplicity and intuitively better performance (and to avoid complex logic in first way )
*/

// mt for multi threading i.e multi threading Event Subscriber
namespace mt {

    const int MAX_BUFFER_SIZE = 1024;
    const int MAX_RETRIES = 3;
    const int POLL_INTERVAL_MS = 100;
    const int DEFAULT_WORKERS = 4;

    class EventSubscriber {
    public:

        // 
        EventSubscriber(int num_workers) :
             m_next_fetch_event_id(0),m_next_event_id(0),m_stop_workers(false) {
                for (int i = 0; i < num_workers; ++i) {
                    m_worker_threads.emplace_back(&EventSubscriber::m_worker_func,this);
            }
        }

        ~EventSubscriber() {
            m_stop_workers = true;
            m_cv.notify_all();

            for (auto& thread:m_worker_threads) {
                thread.join();
            }

        }

        bool setup_stream(int start_id) {
            
            if (!does_event_exist(start_id)){
                return false;
            }
            m_next_fetch_event_id.store(start_id);
            m_next_event_id = start_id; // next_Event_id need not be atomic. Only used by caller using receive()
            return true;
        }

        bool receive(char* buffer) {
            std::unique_lock<std::mutex> lock(m_map_mutex);

            m_cv.wait(lock, [this] { return m_event_map.find(m_next_event_id) != m_event_map.end() || m_stop_workers; });

            if (m_event_map.find(m_next_event_id) == m_event_map.end()) {
                return false;
            }

            std::memcpy(buffer, m_event_map[m_next_event_id], MAX_BUFFER_SIZE);

            delete[] m_event_map[m_next_event_id];  // Free memory allocated for the buffer
            m_event_map.erase(m_next_event_id);

            m_next_event_id++;
            return true;
        }

    private:
        int m_start_event_id; // for statistics 
        int m_next_event_id; // for receive() by callser

        std::vector<std::thread> m_worker_threads; // worker threads
        std::atomic<int> m_next_fetch_event_id;  // counter to fetch event using async netwotk IO
        
        std::unordered_map<int, char*> m_event_map;  // eventid to correspodng ouput buffer for prefetching
        std::mutex m_map_mutex;
        std::condition_variable m_cv;

        bool m_stop_workers; // standard stop_workers variable in a workerpool/multu threading setup



        // workers in a the standard while loop
        void m_worker_func() {

            while (!m_stop_workers) {

                int event_id = m_next_fetch_event_id.fetch_add(1);

                if (!does_event_exist(event_id)) {

                    // NOT thread safe logging. BuT OK for NOW.
                    std::cerr << "Fail: Event does not exist. Id: " << event_id << "Stopping worker thread." << std::endl;

                    m_stop_workers = true;
                    m_cv.notify_all();

                    return;
                }

                char* output_buffer = new char[MAX_BUFFER_SIZE];
                
                // Same likt signle threaded receive() polling
                if (fetch_event_async(event_id, output_buffer)) {
                    
                    bool fetch_completed = false;
                    int retries = MAX_RETRIES;

                    while (!fetch_completed && retries > 0) {
                        
                        Status status = poll_last_status();
                        
                        if (status == Status::RECEIVED) {
                            fetch_completed = true;

                            std::unique_lock<std::mutex> lock(m_map_mutex);
                            m_event_map[event_id] = output_buffer;
                            lock.unlock();
                            m_cv.notify_one();  
                        } 
                        else if (status == Status::FAILED) {
                            retries--;
                            if (retries < 0) {
                                std::cerr << "Fail: Event fetch receive. Id: "<<m_next_fetch_event_id <<"\n";
                                delete[] output_buffer;  
                            }
                        }

                        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));  
                    }
                } else {
                    std::cerr << "Fail: Event fetch Send. Id: " << m_next_fetch_event_id << " Stopping worker thread" << std::endl;

                    delete[] output_buffer;  
                }
            }
        }
    };
}
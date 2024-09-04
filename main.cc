#include <iostream>
#include <chrono>

#include "singlethreadedclient.h"
#include "multithreadedclient.h"
#include "event.h"


// yeah..deep copy of event for now.
void process_event(Event event){
    return;
}


/*
  Example way a single threaded client runs
*/
void single_threaded_client(int start_id) {

    st::EventSubscriber evsub;

    evsub.setup_stream(start_id);
    char output_buf[st::MAX_BUFFER_SIZE];

    while(evsub.receive(output_buf)) {

        Event event;
        std::memcpy(&event.event_id, output_buf, sizeof(int));
        std::memcpy(&event.payload, output_buf+sizeof(int), st::MAX_BUFFER_SIZE - sizeof(int));

        process_event(event);
    }

    return;
}


/*
 Example way to test multi threaded client 
*/
void multi_threaded_client(int start_id) {
    unsigned int num_cores = std::thread::hardware_concurrency();
    const int num_workers = num_cores ? num_cores : mt::DEFAULT_WORKERS;

    mt::EventSubscriber evsub(num_workers);

    evsub.setup_stream(start_id);
    char output_buf[mt::MAX_BUFFER_SIZE];

    while(evsub.receive(output_buf)) {

        Event event;
        std::memcpy(&event.event_id, output_buf, sizeof(int));
        std::memcpy(&event.payload, output_buf+sizeof(int), mt::MAX_BUFFER_SIZE - sizeof(int));

        process_event(event);
    }

}



int main(){

    int start_id = 10001; // some random id


    // RUDIMENTARY WAY to measure exeuction time, assuming server can send 10000 events. 
    // We are measuing overall clent times, rater than event subscriber receive time.
    // For now, it's OK. 

    auto st_start_time = std::chrono::high_resolution_clock::now();
    single_threaded_client(start_id);
    auto st_end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(st_end_time - st_start_time);
    std::cout << "Single Threaded Execution time: " << duration.count() << " microseconds" << std::endl;


    int mt_start_id = 20001; 
    auto mt_start_time = std::chrono::high_resolution_clock::now();
    multi_threaded_client(start_id);
    auto mt_end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(mt_end_time - mt_start_time);
    std::cout << "Single Threaded Execution time: " << duration.count() << " microseconds" << std::endl;



}
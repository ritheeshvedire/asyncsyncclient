// TODO: make it generic class
struct Event {
    static constexpr int MAX_BUFFER_SIZE = 1024; 

    int event_id;
    char payload[MAX_BUFFER_SIZE-sizeof(int)]; // first 4 bytes store event id as some sort of ACK
};
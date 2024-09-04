#include <cstddef> 

enum class Status {
    RECEIVED,
    IN_PROGRESS,
    FAILED
};

bool does_event_exist(int event_id);
bool fetch_event_async(int event_id, char* output_buf);
Status poll_last_status();

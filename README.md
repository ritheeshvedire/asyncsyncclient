# Design Considerations
## Sync API to check last status
1. *poll_last_status()* is a sync api. This is important. Usually, in pure async programming or event driven programming,
 whenever an event is received or notified by underlying runtime, call backs are called. However, we are doing a poll.
That entails, our code to be in a while loop and do the following: 
    1. RECEIVED: next_event_id = current_event_id + 1;
    2. IN_PROGRESS: keep on polling by intermittent waiting. This wait time has to be set after emperical experiments. currently set to 100 ms
    3. FAILED: retry for N times and then return error.

## Single Threaded Client
See, singlthreaded.h implements a straight forward EventSubscriber implementation.
(note: I hope a flying pan hits me for writing source in a header file, ignore for now)

Both function APIS are NOT thread safe.

As the client is single threadead only.

There is NOT much you can do to improve the throughput if SYNC APIs
are used inconjunction with Async API, as:
- Multi threading is NOT allowed
- call backs are NOT allowed (As we use POLLING)
- caller expects events *in order*


## Multi Threaded Client (Two Approaches)
To increase throughput by prefetching events, multiple threads
can leverage CPU cores as they wait for Network IO to be completed.
(Agan reinstatiting, call backs and underlying traditonal event_queue
 is NOT present if there are SYNC APIs)

### 1. Approach ONE. Use existing EventSubscriber class and use muliple threads in the client function

### 2. Approach TWO. Make the EventSubscriber Class multithreaded with workers prefetching data.
Worker Threads will update underlying datastructure using an atomic counter called next_fetch_event_id;
`receive()` is then to query the underlying datastructure in order, incremeting next_event_id;

*We use **approach TWO** for the code*

#### Approach ONE
simply in EventSubscriber class there are worker threads, that prefetch the data.
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

## [TODO] EVEN BETTER THROUGH PUT - several improvemnts
1. Make the map used in Approach Two better  by having locks for each map bucket instead of entire map
2. use call backs. 

## [TODO] BETTER CODE
1. Did not use Generic classes i.e templates
2. C and C++ style mixed
3. Thread safe logging

## [TODO] TEST TODO
1. Write server stub
2. Simulate for testing

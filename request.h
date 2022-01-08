#ifndef __REQUEST_H__

typedef struct stat_t {
    pthread_t threadId;
    int requestCount;
    int staticCount;
    int dynamicCount;
    struct timeval arrivalTime;
    struct timeval dispatchTime;
} Stats;
int requestHandle(int fd,Stats* stats);

#endif

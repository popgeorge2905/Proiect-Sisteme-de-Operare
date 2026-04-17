#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <time.h>

typedef struct {
    int report_id;
    char inspector_name[64];
    double lat;
    double lon;
    char category[32];
    int severity;
    time_t timestamp;
    char description[256];
} ReportRecord;

#endif
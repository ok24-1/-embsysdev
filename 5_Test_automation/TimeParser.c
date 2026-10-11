#include "TimeParser.h"

#include <stddef.h>
#include <string.h>

int time_parse(const char *time)
{
    int hours;
    int minutes;
    int seconds;

    if (time == NULL) {
        return TIME_ARRAY_ERROR;
    }

    if (strlen(time) != 6U) {
        return TIME_LEN_ERROR;
    }

    for (int i = 0; i < 6; ++i) {
        if (time[i] < '0' || time[i] > '9') {
            return TIME_VALUE_ERROR;
        }
    }

    hours = (time[0] - '0') * 10 + (time[1] - '0');
    minutes = (time[2] - '0') * 10 + (time[3] - '0');
    seconds = (time[4] - '0') * 10 + (time[5] - '0');

    if (hours > 23 || minutes > 59 || seconds > 59) {
        return TIME_FORMAT_ERROR;
    }

    return hours * 3600 + minutes * 60 + seconds;
}

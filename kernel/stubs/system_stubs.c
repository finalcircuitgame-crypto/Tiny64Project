#include <stdint.h>
#include <stddef.h>
#include "../include/apps.h"

// Timer functions
uint64_t timer_ms() {
    static uint64_t fake = 0;
    fake += 16;
    return fake;
}

// Standard C time functions
typedef long time_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
};

time_t time(time_t* t) {
    static time_t fake = 1736800000;
    fake += 1;
    if (t) *t = fake;
    return fake;
}

struct tm* localtime(const time_t* t) {
    static struct tm tm;
    tm.tm_sec = (*t % 60);
    tm.tm_min = (*t / 60) % 60;
    tm.tm_hour = (*t / 3600) % 24;
    tm.tm_mday = 1;
    tm.tm_mon = 0;
    tm.tm_year = 125;
    return &tm;
}

size_t strftime(char* s, size_t max, const char* fmt, const struct tm* tm) {
    if (fmt[0] == '%' && fmt[1] == 'H') {
        if (max < 6) return 0;
        s[0] = '0' + (tm->tm_hour / 10);
        s[1] = '0' + (tm->tm_hour % 10);
        s[2] = ':';
        s[3] = '0' + (tm->tm_min / 10);
        s[4] = '0' + (tm->tm_min % 10);
        s[5] = 0;
        return 5;
    }

    if (max < 11) return 0;
    int month = tm->tm_mon + 1;
    int day = tm->tm_mday;
    int year = tm->tm_year;

    s[0] = '0' + (month / 10);
    s[1] = '0' + (month % 10);
    s[2] = '/';
    s[3] = '0' + (day / 10);
    s[4] = '0' + (day % 10);
    s[5] = '/';
    s[6] = '2';
    s[7] = '0';
    s[8] = '0' + ((year - 100) / 10);
    s[9] = '0' + ((year - 100) % 10);
    s[10] = 0;

    return 10;
}

// Application management
char* open_apps[32];
int open_app_count = 0;

void launch_app(AppDefinition* app) {
    open_apps[open_app_count++] = (char*)app->id;
    app->entry();
}

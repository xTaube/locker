#include "locker_logs.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/syslimits.h>
#include <time.h>

void turn_on_logging(const char *workdir) {
    char log_filepath[PATH_MAX] = {0};
    snprintf(log_filepath, PATH_MAX, "%s/%s", workdir, "locker.log");
    freopen(log_filepath, "a", stderr);
    setvbuf(stderr, NULL, _IOLBF, 0);
}

void log_message(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  time_t t = now.tv_sec;

  struct tm tm_utc;
  gmtime_r(&t, &tm_utc);

  char datetime[32];
  strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S UTC", &tm_utc);

  fprintf(stderr, "[%s] ", datetime);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");

  va_end(args);
}

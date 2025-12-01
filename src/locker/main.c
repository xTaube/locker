#include "locker_logs.h"
#include "locker_tui.h"
#include <stdlib.h>

int main(void) {
  turn_on_logging("locker.log");
  run();
  return EXIT_SUCCESS;
}

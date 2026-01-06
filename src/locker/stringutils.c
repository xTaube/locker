#include "locker_stringutils.h"
#include <ctype.h>

void str_replace_spaces(char *s, char replace) {
  while (*s != '\0') {
    if (isspace((unsigned char)*s))
      *s = replace;
    s++;
  }
}

void str_tolower(char *s) {
  while (*s != '\0') {
    *s = tolower((unsigned char)*s);
    s++;
  }
}

bool str_alphnum(const char *s) {
  while (*s != '\0') {
    if (!(isalnum((unsigned char)*s) || isspace((unsigned char)*s)))
      return false;
    s++;
  }
  return true;
}

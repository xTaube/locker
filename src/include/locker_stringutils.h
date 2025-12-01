#ifndef LOCKER_STRINGUTILS_H
#define LOCKER_STRINGUTILS_H

#include <stdbool.h>

void str_replace_spaces(char *s, char replace);
void str_tolower(char *s);
bool str_alphnum(const char *s);

#endif

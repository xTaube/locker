#ifndef LOCKER_TUI_UTILS
#define LOCKER_TUI_UTILS

#define BACKSPACE_KEY 127
#define ENTER_KEY '\n'
#define CTRL_X_KEY 24

#define RETURN_OPTION -1

#define TAB_LEN 4
#define PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET 4
#define PRINTW_DEFAULT_X_OFFSET 2

#include <stddef.h>

void turn_off_user_typing();

void turn_on_user_typing();

void print_control_panel(size_t n_options, const char *options[], int y_offset, int x_offset, int tab_len);

void get_user_str(size_t buffer_sz, char buffer[buffer_sz], int win_y, int win_x, int controls_y, int controls_x, bool typing, bool show_cursor, int attrs);

int choice_selector(int n_choices, const char *choices[], int row_offset);

#endif

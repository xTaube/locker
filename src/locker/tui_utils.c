#include <string.h>
#include <ncurses.h>
#include "locker_tui_utils.h"

void turn_off_user_typing() {
  noecho();
  cbreak();
}

void turn_on_user_typing() {
  echo();
  nocbreak();
}

void print_control_panel(size_t n_options, const char *options[], int y_offset, int x_offset, int tab_len) {
    attron(A_BOLD);

    int x = x_offset;

    for(size_t i = 0; i<n_options; i++) {
        mvprintw(y_offset, x, options[i]);
        x += strlen(options[i]) + tab_len;
    }

    clrtoeol();
    attroff(A_BOLD);
}

void get_user_str(size_t buffer_sz, char buffer[buffer_sz], int win_y,
                  int win_x, int controls_y, int controls_x, bool typing, bool show_cursor, int attrs) {

  const char *control_options[] = {"CTRL-X: Save"};
  print_control_panel(sizeof(control_options)/sizeof(char *), control_options, controls_y, controls_x, TAB_LEN);

  size_t len = strlen(buffer);
  size_t cursor = len;

  if(show_cursor)
      curs_set(1);

  attron(attrs);

  while (1) {
    move(win_y, win_x + cursor);
    int ch = getch();
    switch (ch) {
    case KEY_LEFT:
      if (cursor > 0)
        cursor--;
      break;
    case KEY_RIGHT:
      if (cursor < len)
        cursor++;
      break;
    case CTRL_X_KEY:
      move(controls_y, controls_x);
      clrtoeol();
      attroff(attrs);

      if(show_cursor)
        curs_set(0);
      return;
    case KEY_UP:
    case KEY_DOWN:
    case ENTER_KEY:
      break;
    case BACKSPACE_KEY:
      if (cursor <= 0) {
        break;
      }

      cursor--;
      len--;
      if (cursor == len) {
        buffer[cursor] = '\0';
      } else {
        memmove(buffer + cursor, buffer + cursor + 1, len - cursor);
        buffer[len] = '\0';
      }
      break;
    default:
      if (len >= buffer_sz) {
        break;
      }

      if (cursor == len) {
        buffer[cursor] = (char)ch;
      } else {
        memmove(buffer + cursor + 1, buffer + cursor, len - cursor);
        buffer[cursor] = (char)ch;
      }
      cursor++;
      len++;
      break;
    }

    if(typing)
        mvprintw(win_y, win_x, buffer);

    clrtoeol();
    refresh();
  }
}

int choice_selector(int n_choices, const char *choices[],
                    int row_offset) {
  int highlight = 0;
  while (1) {
    for (int i = 0; i < n_choices; i++) {
      if (i == highlight)
        attron(A_STANDOUT);
      mvprintw(row_offset + i + 1, PRINTW_DEFAULT_X_OFFSET, "%u. %s", i + 1, choices[i]);
      if (i == highlight)
        attroff(A_STANDOUT);
    }
    refresh();

    int ch = getch();
    switch (ch) {
    case KEY_UP:
      if (highlight == 0)
        highlight = n_choices;
      highlight--;
      break;
    case KEY_DOWN:
      highlight++;
      if (highlight >= n_choices)
        highlight = 0;
      break;
    case BACKSPACE_KEY:
      return RETURN_OPTION;
    case ENTER_KEY:
      return highlight;
    }
  }
}

void clear_line_inplace(int y, int x) {
    move(y, 0);
    clrtoeol();
    refresh();
}

void clear_lines_inplace(int y_offset, int n_lines) {
    for(int i=0; i<n_lines; i++) {
        move(i+y_offset, 0);
        clrtoeol();
    }
    refresh();
}

#include "curses.h"
#include "locker.h"
#include "ncurses.h"
#include <ctype.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(A,B) ((A)>(B)?(A):(B))

#define TAB_LEN 4

typedef enum {
  VIEW_STARTUP,
  VIEW_NEW_LOCKER,
  VIEW_LOCKER_LIST,
  VIEW_LOCKER,
  VIEW_ADD_ITEM,
  VIEW_ITEM_LIST,
  VIEW_EXIT,
} view_t;

typedef struct {
  view_t view;
  locker_t *locker;
} context_t;

void turn_off_user_typing() {
  noecho();
  cbreak();
}

void turn_on_user_typing() {
  echo();
  nocbreak();
}

void print_control_panel(size_t n_options, const char *options[], int y_offset, int x_offset) {
    attron(A_BOLD);

    int x = x_offset;

    for(size_t i = 0; i<n_options; i++) {
        mvprintw(y_offset, x, options[i]);
        x = strlen(options[i]) + TAB_LEN;
    }

    clrtoeol();
    attroff(A_BOLD);
}

void get_user_str(size_t buffer_sz, char buffer[buffer_sz], int win_y,
                  int win_x, int controls_y, int controls_x) {

  const char *control_options[] = {"CTRL-X: Save"};
  print_control_panel(sizeof(control_options)/sizeof(char *), control_options, controls_y, controls_x);

  size_t len = strlen(buffer);
  size_t cursor = len;

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
    case 24:
      move(controls_y, controls_x);
      clrtoeol();
      return;
    case '\n':
      break;
    case 127:
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
      mvprintw(row_offset + i + 1, 2, "%u. %s", i + 1, choices[i]);
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
    case 127:
      return -1;
    case '\n':
      return highlight;
    }
  }
}

void startup_view(context_t *ctx) {
  clear();

  attron(A_BOLD);
  mvprintw(1, 2, "Locker.");
  attroff(A_BOLD);

  const char *choices[] = {
      "Enter locker",
      "Exit",
  };

  int option = choice_selector(sizeof(choices) / sizeof(char *), choices, 1);

  switch (option) {
  case -1:
    ctx->view = VIEW_STARTUP;
    break;
  case 0:
    ctx->view = VIEW_LOCKER_LIST;
    break;
  case 1:
    ctx->view = VIEW_EXIT;
    break;
  }
}

void locker_list_view(context_t *ctx) {
  clear();
  attron(A_BOLD);
  mvprintw(1, 2, "Lockers list.");
  attroff(A_BOLD);

  char **lockers;
  int n_lockers = lockers_list(&lockers);

  if (n_lockers == 0) {
    /* no allocations if n_lockers == 0 */
    ctx->view = VIEW_NEW_LOCKER;
    return;
  }

  const char *control_options[] = {"BACKSPACE: Return"};
  print_control_panel(sizeof(control_options)/sizeof(char*), control_options, n_lockers+4, 2);

  int locker = choice_selector(n_lockers, (const char **)lockers, 1);
  if (locker < 0) {
    ctx->view = VIEW_STARTUP;
    return;
  }

  clear();
  attron(A_BOLD);
  mvprintw(1, 2, "%s", lockers[locker]);
  attroff(A_BOLD);

  char passphrase[LOCKER_PASSPHRASE_MAX_LEN + 2];
  mvprintw(2, 2, "Passphrase: ");
  refresh();
  getnstr(passphrase, sizeof(passphrase) - 1);

  ctx->locker = locker_open(lockers[locker], passphrase);
  if (ctx->locker) {
    ctx->view = VIEW_LOCKER;
    return;
  }

  mvprintw(3, 2, "Something went wrong. Check logs for more information.");
  getch();
  ctx->view = VIEW_LOCKER_LIST;
}

void new_locker_view(context_t *ctx) {
  clear();

  attron(A_BOLD);
  mvprintw(1, 2, "No locker found, please create new locker.");
  attroff(A_BOLD);

  // const char *control_options[] = {"BACKSPACE: Return"};
  // print_control_panel(sizeof(control_options)/sizeof(char*), control_options, 5, 2);
  // not yet implemented

  char locker_name[LOCKER_NAME_MAX_LEN + 2];
  do {
    mvprintw(2, 2, "Enter locker name: ");
    clrtoeol();
    refresh();

    turn_on_user_typing();
    getnstr(locker_name, sizeof(locker_name) - 1);

    if (strlen(locker_name) > LOCKER_NAME_MAX_LEN) {
      mvprintw(3, 2, "Locker name too long! Maxiumum length is %u",
               LOCKER_NAME_MAX_LEN);
      refresh();
    } else if (strlen(locker_name) < 1) {
      mvprintw(3, 2, "Locker name cannot be empty.");
      refresh();
    } else
      break;
  } while (1);

  turn_off_user_typing();

  char passphrase1[LOCKER_PASSPHRASE_MAX_LEN + 2];
  char passphrase2[LOCKER_PASSPHRASE_MAX_LEN + 2];
  do {
    mvprintw(3, 2, "Enter passphrase: ");
    clrtoeol();
    refresh();
    getnstr(passphrase1, sizeof(passphrase1) - 1);
    if (strlen(passphrase1) > LOCKER_PASSPHRASE_MAX_LEN) {
      mvprintw(4, 2, "Passphrase too long! Maxiumum length is %u",
               LOCKER_PASSPHRASE_MAX_LEN);
      clrtoeol();
      refresh();
    } else if (strlen(passphrase1) < 1) {
      mvprintw(4, 2, "Passphrase cannot be empty.");
      clrtoeol();
      refresh();
    } else {
      mvprintw(4, 2, "Repeat passphrase: ");
      clrtoeol();
      refresh();
      getnstr(passphrase2, sizeof(passphrase2) - 1);
      if (strcmp(passphrase1, passphrase2) == 0)
        break;
      else {
        mvprintw(5, 2, "Passphrases do not match!");
        clrtoeol();
        refresh();
      }
    }
  } while (1);

  locker_result_t rc = locker_create(locker_name, passphrase1);

  switch (rc) {
  case LOCKER_OK:
    ctx->view = VIEW_LOCKER_LIST;
    return;
  case LOCKER_NAME_FORBIDDEN_CHAR:
    mvprintw(5, 2, "Locker name can only contain alphanumeric characers.");
    clrtoeol();
    break;
  default:
    mvprintw(5, 2,
             "Something went wrong. Check log file for more information.");
    clrtoeol();
    break;
  }
  mvprintw(6, 2, "Press key to continue ...");
  clrtoeol();
  refresh();
  getch();
}

void locker_view(context_t *ctx) {
  if (!ctx->locker) {
    ctx->view = VIEW_LOCKER_LIST;
    return;
  }

  clear();

  attron(A_BOLD);
  mvprintw(1, 2, "%s", ctx->locker->locker_name);
  attroff(A_BOLD);

  const char *choices[] = {
      "List items",
      "Add item",
      "Save and close",
  };
  size_t n_choices = sizeof(choices) / sizeof(char *);

  const char *control_options[] = {"BACKSPACE: Return"};

  print_control_panel(sizeof(control_options)/sizeof(char*), control_options, n_choices+4, 2);
  int option = choice_selector(sizeof(choices) / sizeof(char *), choices, 1);

  switch (option) {
  case 0:
    ctx->view = VIEW_ITEM_LIST;
    break;
  case 1:
    ctx->view = VIEW_ADD_ITEM;
    break;

  case -1:
  case 2:
    save_and_close_locker(ctx->locker);
    ctx->locker = NULL;
    ctx->view = VIEW_LOCKER_LIST;
    break;
  }
}


void add_apikey_view(context_t *ctx) {
  clear();

  attron(A_BOLD);
  mvprintw(1, 2, "Add Item.");
  attroff(A_BOLD);

  char *rows[] = {
      "Key:",
      "Description:",
      "API Key:",
  };
  unsigned int rows_max[] = {LOCKER_ITEM_KEY_MAX_LEN,
                             LOCKER_ITEM_DESCRIPTION_MAX_LEN, LOCKER_ITEM_CONTENT_MAX_LEN};
  int n_rows = sizeof(rows) / sizeof(char *);

  char **rows_content = malloc(sizeof(char *) * n_rows);
  if (!rows_content) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  rows_content[0] = calloc((LOCKER_ITEM_KEY_MAX_LEN) + 1, sizeof(char));
  rows_content[1] = calloc((LOCKER_ITEM_DESCRIPTION_MAX_LEN) + 1, sizeof(char));
  rows_content[2] = calloc((LOCKER_ITEM_CONTENT_MAX_LEN)+1, sizeof(char));
  if (!rows_content[0] || !rows_content[1] || !rows_content[2]) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }

  const char *control_options[] = {"CTRL-X: Add", "BACKSPACE: Return"};

  int rc = 0;
  do {
    bool save = false;
    int highlight = 0;
    while (!save) {
        for (int i = 0; i < n_rows; i++) {
        if (i == highlight)
            attron(A_STANDOUT);
        mvprintw(i + 2, 2, "%s %s", rows[i], rows_content[i]);
        if (i == highlight)
            attroff(A_STANDOUT);
        }

        print_control_panel(sizeof(control_options)/sizeof(char *), control_options, 7, 2);
        refresh();

        int ch = getch();
        switch (ch) {
        case KEY_UP:
        if (highlight == 0)
            highlight = n_rows;
        highlight--;
        break;
        case KEY_DOWN:
        highlight++;
        if (highlight >= n_rows)
            highlight = 0;
        break;
        case 24:
            if(strlen(rows_content[0])<1) {
                mvprintw(6, 2, "Item key is missing.");
                clrtoeol();
                break;
            }
            if(strlen(rows_content[2])<1) {
                mvprintw(6, 2, "API Key is missing.");
                clrtoeol();
                break;
            }
            save = true;
            break;
        case 127:
        free(rows_content[0]);
        free(rows_content[1]);
        free(rows_content[2]);
        ctx->view = VIEW_LOCKER;
        return;
        case '\n':
        get_user_str(rows_max[highlight], rows_content[highlight], highlight + 2,
                    strlen(rows[highlight]) + 3, n_rows + 4, 2);
        break;
        }
  }

  rc = locker_add_item(ctx->locker, rows_content[0], rows_content[1], rows_content[2], LOCKER_ITEM_APIKEY);
  if (rc == LOCKER_ITEM_KEY_EXISTS) {
      mvprintw(5, 2, "Given key already exisit in Locker.");
      clrtoeol();
  }

  } while(rc != LOCKER_OK);


  free(rows_content[0]);
  free(rows_content[1]);
  free(rows_content[2]);
  ctx->view = VIEW_ITEM_LIST;
}

void add_item_view(context_t *ctx) {
    clear();

    attron(A_BOLD);
    mvprintw(1, 2, "Choose item type.");
    attroff(A_BOLD);

    const char *choices[] = {
        "Account",
        "Api Key",
    };
    size_t n_choices = sizeof(choices)/sizeof(char*);

    const char *control_options[] = {"BACKSPACE: Return"};

    print_control_panel(sizeof(control_options)/sizeof(char*), control_options, n_choices+4, 2);
    int choice = choice_selector(n_choices, choices, 1);

    switch(choice) {
        case -1:
            ctx->view = VIEW_LOCKER;
            break;
        case 0:
            break;
        case 1:
            add_apikey_view(ctx);
            break;
    }
}

const char *get_item_type_str(locker_item_type_t type) {
    switch(type){
        case LOCKER_ITEM_APIKEY:
        return "API Key";
        case LOCKER_ITEM_ACCOUNT:
        return "ACCOUNT";
        case LOCKER_ITEM_NOTE:
        return "Note";
    }
}

void view_item(locker_item_t item) {
    clear();

    size_t x_offset = 2;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Key");
    attroff(A_BOLD);

    mvprintw(2, x_offset, item.key);
    x_offset += MAX(strlen(item.key), strlen("Key")) + TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Description");
    attroff(A_BOLD);

    mvprintw(2, x_offset, item.description);
    x_offset += MAX(strlen(item.description), strlen("Description"))+ TAB_LEN;

    const char* type = get_item_type_str(item.type);

    attron(A_BOLD);
    mvprintw(1, x_offset, "Type");
    attroff(A_BOLD);

    mvprintw(2, x_offset, type);
    x_offset += MAX(strlen(type), strlen("Type"))+TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Secret");
    attroff(A_BOLD);

    mvaddnstr(2, x_offset, (const char *)item.content, item.content_size);

    const char *control_options[] = {"BACKSPACE: Return"};

    print_control_panel(sizeof(control_options)/sizeof(char *), control_options, 5, 2);
    refresh();

    while(1) {
        int ch = getch();
        if(ch == 127) break;
    }
}

void item_list_view(context_t *ctx) {
    clear();

    attron(A_BOLD);
    mvprintw(1, 2, "Items.");
    attroff(A_BOLD);

    locker_item_t *items;
    long long n_items = locker_get_items(ctx->locker, &items);

    const char *control_options[] = {"BACKSPACE: Return"};
    print_control_panel(sizeof(control_options)/sizeof(char *), control_options, n_items+4, 2);

    char **choices = malloc(n_items*sizeof(char*));
    for(long long i = 0; i<n_items; i++) {
        choices[i] = items[i].key;
    }

    int option = choice_selector(n_items, (const char **)choices, 1);

    switch(option) {
        case -1:
            ctx->view = VIEW_LOCKER;
            break;
        default:
            view_item(items[option]);
            break;
    }

    free(choices);
}

int run() {
  context_t context = {.view = VIEW_STARTUP, .locker = NULL};

  initscr();
  turn_off_user_typing();
  keypad(stdscr, true);

  bool running = true;
  while (running) {
    switch (context.view) {
    case VIEW_STARTUP:
      startup_view(&context);
      break;
    case VIEW_LOCKER:
      locker_view(&context);
      break;
    case VIEW_NEW_LOCKER:
      new_locker_view(&context);
      break;
    case VIEW_LOCKER_LIST:
      locker_list_view(&context);
      break;
    case VIEW_ADD_ITEM:
      add_item_view(&context);
      break;
    case VIEW_ITEM_LIST:
      item_list_view(&context);
      break;
    case VIEW_EXIT:
      running = false;
      break;
    }
  }

  endwin();
  return 0;
}

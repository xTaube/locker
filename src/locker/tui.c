#include "curses.h"
#include "locker.h"
#include "locker_tui_utils.h"
#include "ncurses.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(A,B) ((A)>(B)?(A):(B))

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

void startup_view(context_t *ctx) {
  clear();

  attron(A_BOLD);
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Locker");
  attroff(A_BOLD);

  const char *choices[] = {
      "Enter locker",
      "Exit",
  };

  int option = choice_selector(sizeof(choices) / sizeof(char *), choices, 1);

  switch (option) {
  case RETURN_OPTION:
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
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Lockers list.");
  attroff(A_BOLD);

  char **lockers;
  int n_lockers = lockers_list(&lockers);

  if (n_lockers == 0) {
    /* no allocations if n_lockers == 0 */
    ctx->view = VIEW_NEW_LOCKER;
    return;
  }

  const char *control_options[] = {"BACKSPACE: Return"};
  print_control_panel(sizeof(control_options)/sizeof(char*), control_options, n_lockers+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);

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

  mvprintw(3, PRINTW_DEFAULT_X_OFFSET, "Something went wrong. Check logs for more information.");
  getch();
  ctx->view = VIEW_LOCKER_LIST;
}

void new_locker_view(context_t *ctx) {
  clear();

  attron(A_BOLD);
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "No locker found, please create new locker.");
  attroff(A_BOLD);

  const char *rows[] = {
      "Name:",
      "Passphrase:",
      "Repeat passphrase:"
  };
  int n_rows = sizeof(rows)/sizeof(char*);

  const unsigned int rows_max_len[] = {
      LOCKER_NAME_MAX_LEN, LOCKER_PASSPHRASE_MAX_LEN, LOCKER_PASSPHRASE_MAX_LEN,
  };

  char **rows_content = malloc(sizeof(char*)*n_rows);
  if(!rows_content) {
      perror("malloc.");
      exit(EXIT_FAILURE);
  }

  rows_content[0] = calloc(LOCKER_NAME_MAX_LEN+1, sizeof(char));
  rows_content[1] = calloc(LOCKER_PASSPHRASE_MAX_LEN+2, sizeof(char));
  rows_content[2] = calloc(LOCKER_PASSPHRASE_MAX_LEN+2, sizeof(char));

  if(!rows_content[0] || !rows_content[1] || !rows_content[2]) {
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
        if (i > 0)
            mvprintw(i + 2, PRINTW_DEFAULT_X_OFFSET, rows[i]);
        else
            mvprintw(i + 2, PRINTW_DEFAULT_X_OFFSET, "%s %s", rows[i], rows_content[i]);
        if (i == highlight)
            attroff(A_STANDOUT);
        }

        print_control_panel(sizeof(control_options)/sizeof(char *), control_options, PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET+n_rows, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);
        refresh();

        bool typing = true;
        bool show_cursor = true;

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
        case CTRL_X_KEY:
            if(strlen(rows_content[0])<1) {
                mvprintw(n_rows+3, PRINTW_DEFAULT_X_OFFSET, "Locker name cannot be empty.");
                clrtoeol();
                break;
            }
            if(strlen(rows_content[1])<1) {
                mvprintw(n_rows+3, PRINTW_DEFAULT_X_OFFSET, "Passphrase cannot be empty.");
                clrtoeol();
                break;
            }
            if (strcmp(rows_content[1], rows_content[2]) == 0)
                save = true;
            else {
              mvprintw(n_rows+3, PRINTW_DEFAULT_X_OFFSET, "Passphrases do not match!");
              clrtoeol();
            }
            break;
        case BACKSPACE_KEY:
            free(rows_content[0]);
            free(rows_content[1]);
            free(rows_content[2]);
            ctx->view = VIEW_STARTUP;
            return;
        case ENTER_KEY:
            if(highlight>0){
                typing = false;
                show_cursor = false;
            } else {
                typing = true;
                show_cursor = true;
            }

            get_user_str(rows_max_len[highlight], rows_content[highlight], highlight + 2,
                        strlen(rows[highlight]) + 3, n_rows + 4, PRINTW_DEFAULT_X_OFFSET, typing, show_cursor, A_STANDOUT);
            break;
        }
  }

    locker_result_t rc = locker_create(rows_content[0], rows_content[1]);

    if(rc == LOCKER_NAME_FORBIDDEN_CHAR) {
        mvprintw(n_rows+3, PRINTW_DEFAULT_X_OFFSET, "Locker name can only contain alphanumeric characers.");
        clrtoeol();
    } else if(rc != LOCKER_OK) {
        mvprintw(n_rows+3, PRINTW_DEFAULT_X_OFFSET, "Something went wrong. Check log file for more information.");
        clrtoeol();
    }

  } while(rc != LOCKER_OK);


  free(rows_content[0]);
  free(rows_content[1]);
  free(rows_content[2]);
  ctx->view = VIEW_LOCKER;
}

void locker_view(context_t *ctx) {
  if (!ctx->locker) {
    ctx->view = VIEW_LOCKER_LIST;
    return;
  }

  clear();

  attron(A_BOLD);
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "%s", ctx->locker->locker_name);
  attroff(A_BOLD);

  const char *choices[] = {
      "List items",
      "Add item",
      "Save and close",
  };
  size_t n_choices = sizeof(choices) / sizeof(char *);

  const char *control_options[] = {"BACKSPACE: Return"};

  print_control_panel(sizeof(control_options)/sizeof(char*), control_options, n_choices+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);
  int option = choice_selector(sizeof(choices) / sizeof(char *), choices, 1);

  switch (option) {
  case 0:
    ctx->view = VIEW_ITEM_LIST;
    break;
  case 1:
    ctx->view = VIEW_ADD_ITEM;
    break;

  case RETURN_OPTION:
  case 2:
    close_locker(ctx->locker);
    ctx->locker = NULL;
    ctx->view = VIEW_LOCKER_LIST;
    break;
  }
}


void add_apikey_view(context_t *ctx) {
  clear();

  attron(A_BOLD);
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Add Item.");
  attroff(A_BOLD);

  char *rows[] = {
      "Key:",
      "Description:",
      "API Key:",
  };
  unsigned int rows_max_len[] = {LOCKER_ITEM_KEY_MAX_LEN,
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
        mvprintw(i + 2, PRINTW_DEFAULT_X_OFFSET, "%s %s", rows[i], rows_content[i]);
        if (i == highlight)
            attroff(A_STANDOUT);
        }

        print_control_panel(sizeof(control_options)/sizeof(char *), control_options, PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET+3, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);
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
        case CTRL_X_KEY:
            if(strlen(rows_content[0])<1) {
                mvprintw(6, PRINTW_DEFAULT_X_OFFSET, "Item key is missing.");
                clrtoeol();
                break;
            }
            if(strlen(rows_content[2])<1) {
                mvprintw(6, PRINTW_DEFAULT_X_OFFSET, "API Key is missing.");
                clrtoeol();
                break;
            }
            save = true;
            break;
        case BACKSPACE_KEY:
        free(rows_content[0]);
        free(rows_content[1]);
        free(rows_content[2]);
        ctx->view = VIEW_LOCKER;
        return;
        case ENTER_KEY:
        get_user_str(rows_max_len[highlight], rows_content[highlight], highlight + 2,
                    strlen(rows[highlight]) + 3, n_rows + 4, PRINTW_DEFAULT_X_OFFSET, true, true, A_STANDOUT);
        break;
        }
  }

  rc = locker_add_item(ctx->locker, rows_content[0], rows_content[1], rows_content[2], LOCKER_ITEM_APIKEY);
  if (rc == LOCKER_ITEM_KEY_EXISTS) {
      mvprintw(5, PRINTW_DEFAULT_X_OFFSET, "Given key already exisit in Locker.");
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
    mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Choose item type.");
    attroff(A_BOLD);

    const char *choices[] = {
        "Account",
        "Api Key",
    };
    size_t n_choices = sizeof(choices)/sizeof(char*);

    const char *control_options[] = {"BACKSPACE: Return"};

    print_control_panel(sizeof(control_options)/sizeof(char*), control_options, n_choices+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);
    int choice = choice_selector(n_choices, choices, 1);

    switch(choice) {
        case RETURN_OPTION:
            ctx->view = VIEW_LOCKER;
            break;
        case 0:
            break;
        case 1:
            add_apikey_view(ctx);
            break;
    }

    save_locker(ctx->locker);
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

    size_t x_offset = PRINTW_DEFAULT_X_OFFSET;

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
    mvprintw(1, x_offset, "Value");
    attroff(A_BOLD);

    mvaddnstr(2, x_offset, (const char *)item.content, item.content_size);

    const char *control_options[] = {"BACKSPACE: Return"};

    print_control_panel(sizeof(control_options)/sizeof(char *), control_options, PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET+1, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);
    refresh();

    while(1) {
        int ch = getch();
        if(ch == BACKSPACE_KEY) break;
    }
}

void item_list_view(context_t *ctx) {
    clear();

    attron(A_BOLD);
    mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Items.");
    attroff(A_BOLD);

    locker_item_t *items;
    long long n_items = locker_get_items(ctx->locker, &items);

    const char *control_options[] = {"BACKSPACE: Return"};
    print_control_panel(sizeof(control_options)/sizeof(char *), control_options, n_items+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);

    char **choices = malloc(n_items*sizeof(char*));
    for(long long i = 0; i<n_items; i++) {
        choices[i] = items[i].key;
    }

    int option = choice_selector(n_items, (const char **)choices, 1);

    switch(option) {
        case RETURN_OPTION:
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
  curs_set(0);

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

  curs_set(1);
  endwin();
  return 0;
}

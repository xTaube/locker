#include "curses.h"
#include "locker.h"
#include "ncurses.h"
#include <string.h>

typedef enum {
  VIEW_STARTUP,
  VIEW_NEW_LOCKER,
  VIEW_LOCKER_LIST,
  VIEW_LOCKER,
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

int choice_selector(int n_choices, const char *choices[n_choices],
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

  const char *choices[] = {
      "Enter locker",
      "Exit",
  };

  int option = choice_selector(sizeof(choices) / sizeof(char *), choices, 0);

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

  ctx->locker = open_locker(lockers[locker], passphrase);
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

  locker_result_t rc = create_locker(locker_name, passphrase1);

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

  int option = choice_selector(sizeof(choices) / sizeof(char *), choices, 1);

  switch (option) {
  case 0:
    mvprintw(5, 2, "You chose to list items");
    clrtoeol();
    getch();
    break;
  case 1:
    mvprintw(5, 2, "You chose to add item");
    clrtoeol();
    getch();
    break;

  case -1:
  case 2:
    save_and_close_locker(ctx->locker);
    ctx->locker = NULL;
    ctx->view = VIEW_LOCKER_LIST;
    break;
  }
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
    case VIEW_EXIT:
      running = false;
      break;
    }
  }

  endwin();
  return 0;
}

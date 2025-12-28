#include "attrs.h"
#include "curses.h"
#include "locker.h"
#include "locker_logs.h"
#include "locker_tui_utils.h"
#include "locker_utils.h"
#include "sodium/utils.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#define MAX(A,B) ((A)>(B)?(A):(B))
#define MIN(A,B) ((A)<(B)?(A):(B))

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
    int rows;
    int cols;
} window_size_t;

typedef struct {
  window_size_t win_size;
  view_t view;
  locker_t *locker;
} context_t;

typedef struct {
    char *key;
    char *description;
    char *value;
} apikey_form_t;

void free_apikey_form_rows(apikey_form_t *form) {
    free(form->key);
    free(form->description);

    /* set memory used for storing apikey to 0 to remove it from registers */
    sodium_memzero(form->value, strlen(form->value));
    free(form->value);
}

typedef struct {
    char *key;
    char *description;
    char *username;
    char *password;
    char *url;
} account_form_t;

void free_account_form_rows(account_form_t *form) {
    free(form->key);
    free(form->description);

    /* set memory used for storing username to 0 to remove it from registers */
    sodium_memzero(form->username, strlen(form->username));
    free(form->username);

    /* set memory used for storing password to 0 to remove it from registers */
    sodium_memzero(form->password, strlen(form->password));
    free(form->password);
    free(form->url);
}

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
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Lockers list");
  attroff(A_BOLD);

  array_str_t *lockers = lockers_list();

  if (lockers->count == 0) {
    /* no allocations if lockers->count == 0 */
    ctx->view = VIEW_NEW_LOCKER;
    locker_array_t_free(lockers, free);
    return;
  }

  const char *control_options[] = {"BACKSPACE: Return"};
  print_control_panel(sizeof(control_options)/sizeof(char*), control_options, lockers->count+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);

  int locker = choice_selector(lockers->count, (const char **)lockers->values, 1);
  if (locker < 0) {
    ctx->view = VIEW_STARTUP;
    return;
  }

  clear();
  attron(A_BOLD);
  mvprintw(1, 2, "%s", lockers->values[locker]);
  attroff(A_BOLD);

  const char *unlock_file_control_options[] = {"ENTER: Try again", "BACKSPACE: Return"};
  char passphrase[LOCKER_PASSPHRASE_MAX_LEN + 2];

  while(1){
    mvprintw(2, 2, "Passphrase: ");
    refresh();
    getnstr(passphrase, sizeof(passphrase) - 1);

    locker_result_t rc = locker_open(&(ctx->locker), lockers->values[locker], passphrase);
    if (rc == LOCKER_OK) {
        ctx->view = VIEW_LOCKER;
        return;
    } else if (rc == LOCKER_INVALID_PASSPRHRASE) {
        mvprintw(4, 2, "Invalid passphrase.");
    } else if (rc == LOCKER_MALFORMED_HEADER) {
        mvprintw(4, 2, "Locker file you're trying to access is malformed. Check log file for more information.");
    }

    print_control_panel(sizeof(unlock_file_control_options)/sizeof(char*), unlock_file_control_options, 1+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);

    int ch = getch();
    if(ch == BACKSPACE_KEY) {
        locker_array_t_free(lockers, free);
        return;
    } /* otherwise try to login once more */

    move(4, 2);
    clrtoeol();

    move(5,2);
    clrtoeol();
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


int apikey_form(apikey_form_t form[static 1], locker_item_apikey_t *apikey, int y_offset, int x_offset, int n_control_options, const char **control_options) {
    char *rows[] = {
        "Key:",
        "Description:",
        "API Key:",
    };
    unsigned int rows_max_len[] = {LOCKER_ITEM_KEY_MAX_LEN,
                               LOCKER_ITEM_DESCRIPTION_MAX_LEN, LOCKER_ITEM_CONTENT_MAX_LEN};
    int n_rows = sizeof(rows) / sizeof(char *);

    form->key = calloc((LOCKER_ITEM_KEY_MAX_LEN) + 1, sizeof(char));
    form->description = calloc((LOCKER_ITEM_DESCRIPTION_MAX_LEN) + 1, sizeof(char));
    form->value = calloc((LOCKER_ITEM_CONTENT_MAX_LEN)+1, sizeof(char));

    if (!form->key || !form->description || !form->value) {
      perror("calloc");
      exit(EXIT_FAILURE);
    }

    if(apikey) {
        memcpy(form->key, apikey->key, strlen(apikey->key));
        memcpy(form->description, apikey->description, strlen(apikey->description));
        memcpy(form->value, apikey->value, strlen(apikey->value));
    }

    char *rows_content[] = {form->key, form->description, form->value};

    int highlight = 0;
    while (1) {
        for (int i = 0; i < n_rows; i++) {
        if (i == highlight)
            attron(A_STANDOUT);
        mvprintw(i + y_offset, x_offset, "%s %s", rows[i], rows_content[i]);
        if (i == highlight)
            attroff(A_STANDOUT);
        }

        print_control_panel(n_control_options, control_options, PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET+n_rows, x_offset, TAB_LEN);
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
            if(strlen(form->key)<1) {
                mvprintw(6, x_offset, "Item key is missing.");
                clrtoeol();
                break;
            }
            if(strlen(form->value)<1) {
                mvprintw(6, x_offset, "API Key is missing.");
                clrtoeol();
                break;
            }
            /* clear sensitive information in case scrollback is on */
            clear_lines_inplace(y_offset, n_rows);
            return 0;
        case BACKSPACE_KEY:
            /* clear sensitive information in case scrollback is on */
            clear_lines_inplace(y_offset, n_rows);
            return BACKSPACE_KEY;
        case ENTER_KEY:
            get_user_str(rows_max_len[highlight], rows_content[highlight], highlight + y_offset,
                    strlen(rows[highlight]) + 3, n_rows + PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, x_offset, true, true, A_STANDOUT);
            break;
        }
    }
}

int account_form(account_form_t form[static 1], locker_item_account_t *account, int y_offset, int x_offset, int n_control_options, const char **control_options) {
    char *rows[] = {
        "Key:",
        "Description:",
        "Username:",
        "Password:",
        "URL:",
    };
    unsigned int rows_max_len[] = {LOCKER_ITEM_KEY_MAX_LEN, LOCKER_ITEM_DESCRIPTION_MAX_LEN, LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN, LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN, LOCKER_ITEM_ACCOUNT_URL_MAX_LEN};
    int n_rows = sizeof(rows) / sizeof(char *);

    form->key = calloc((LOCKER_ITEM_KEY_MAX_LEN) + 1, sizeof(char));
    form->description = calloc((LOCKER_ITEM_DESCRIPTION_MAX_LEN) + 1, sizeof(char));
    form->username = calloc((LOCKER_ITEM_ACCOUNT_USERNAME_MAX_LEN)+1, sizeof(char));
    form->password = calloc((LOCKER_ITEM_ACCOUNT_PASSWORD_MAX_LEN)+1, sizeof(char));
    form->url = calloc((LOCKER_ITEM_ACCOUNT_URL_MAX_LEN)+1, sizeof(char));

    if (!form->key || !form->description || !form->username || !form->password || !form->url) {
      perror("calloc");
      exit(EXIT_FAILURE);
    }

    if(account) {
        memcpy(form->key, account->key, strlen(account->key));
        memcpy(form->description, account->description, strlen(account->description));
        memcpy(form->username, account->username, strlen(account->username));
        memcpy(form->password, account->password, strlen(account->password));
        memcpy(form->url, account->url, strlen(account->url));
    }

    char *rows_content[] = {form->key, form->description, form->username, form->password, form->url};

    int highlight = 0;
    while (1) {
        for (int i = 0; i < n_rows; i++) {
        if (i == highlight)
            attron(A_STANDOUT);
        mvprintw(i + y_offset, x_offset, "%s %s", rows[i], rows_content[i]);
        if (i == highlight)
            attroff(A_STANDOUT);
        }

        print_control_panel(n_control_options, control_options, PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET+n_rows, x_offset, TAB_LEN);
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
            if(strlen(form->key)<1) {
                mvprintw(6, x_offset, "Item key is missing.");
                clrtoeol();
                break;
            }
            /* clear sensitive data in case scrollback is on */
            clear_lines_inplace(y_offset, n_rows);
            return 0;
        case BACKSPACE_KEY:
            /* clear sensitive data in case scrollback is on */
            clear_lines_inplace(y_offset, n_rows);
            return BACKSPACE_KEY;
        case ENTER_KEY:
        get_user_str(rows_max_len[highlight], rows_content[highlight], highlight + y_offset,
                    strlen(rows[highlight]) + 3, n_rows + PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, x_offset, true, true, A_STANDOUT);
        break;
        }
    }
}

void add_apikey_view(context_t ctx[static 1]) {
  clear();

  attron(A_BOLD);
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Add Item.");
  attroff(A_BOLD);

  const char *control_options[] = {"CTRL-X: Add", "BACKSPACE: Return"};

  apikey_form_t form = {0};

  int rc = 0;
  do {
      if(apikey_form(&form, NULL, 2, PRINTW_DEFAULT_X_OFFSET, sizeof(control_options)/sizeof(char*), control_options) == BACKSPACE_KEY) {
          ctx->view = VIEW_LOCKER;
          return;
      }

    locker_item_apikey_t item = {.id=0, .key=form.key, .description=form.description, .value=form.value};
    rc = locker_add_apikey(ctx->locker, &item);
    if (rc == LOCKER_ITEM_KEY_EXISTS) {
        mvprintw(6, PRINTW_DEFAULT_X_OFFSET, "Given key already exisit in your Locker.");
        clrtoeol();
    }

  } while(rc != LOCKER_OK);

  free_apikey_form_rows(&form);
  ctx->view = VIEW_ITEM_LIST;
}

void edit_apikey_view(context_t *ctx, locker_item_t item[static 1]) {
    locker_item_apikey_t *apikey = locker_get_apikey(ctx->locker, item->id);

    clear();

    attron(A_BOLD);
    mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Edit API Key");
    attroff(A_BOLD);

    const char *control_options[] = {"CTRL-X: Save", "BACKSPACE: Return"};

    apikey_form_t form = {0};
    int rc = 0;
    do {
        if(apikey_form(&form, apikey, 2, PRINTW_DEFAULT_X_OFFSET, sizeof(control_options)/sizeof(char*), control_options) == BACKSPACE_KEY) {
            return;
        }

        locker_item_apikey_t updated_apikey = {.id=apikey->id, .key=form.key, .description=form.description, .value=form.value};
        rc = locker_update_apikey(ctx->locker, &updated_apikey);
    } while(rc != LOCKER_OK);

    free_apikey_form_rows(&form);
    locker_free_apikey(apikey);
}

void add_account_view(context_t *ctx) {
  clear();

  attron(A_BOLD);
  mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Add Item");
  attroff(A_BOLD);

  const char *control_options[] = {"CTRL-X: Add", "BACKSPACE: Return"};

  account_form_t form = {0};

  int rc = 0;
  do {
      if(account_form(&form, NULL, 2, PRINTW_DEFAULT_X_OFFSET, sizeof(control_options)/sizeof(char*), control_options) == BACKSPACE_KEY) {
          ctx->view = VIEW_LOCKER;
          return;
      }

    locker_item_account_t item = {.id=0, .key=form.key, .description=form.description, .username=form.username, .password=form.password, .url=form.url};
    rc = locker_add_account(ctx->locker, &item);
    if (rc == LOCKER_ITEM_KEY_EXISTS) {
        mvprintw(6, PRINTW_DEFAULT_X_OFFSET, "Given key already exisit in your Locker.");
        clrtoeol();
    }

  } while(rc != LOCKER_OK);


  free_account_form_rows(&form);
  ctx->view = VIEW_ITEM_LIST;
}

void edit_account_view(context_t *ctx, locker_item_t item[static 1]) {
    locker_item_account_t *account = locker_get_account(ctx->locker, item->id);

    clear();

    attron(A_BOLD);
    mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Edit Account");
    attroff(A_BOLD);

    const char *control_options[] = {"CTRL-X: Save", "BACKSPACE: Return"};

    account_form_t form = {0};

    int rc = 0;
    do {
        if(account_form(&form, account, 2, PRINTW_DEFAULT_X_OFFSET, sizeof(control_options)/sizeof(char*), control_options) == BACKSPACE_KEY) {
            return;
        }

        locker_item_account_t updated_account = {.id=account->id, .key=form.key, .description=form.description, .username=form.username, .password=form.password, .url=form.url};
        rc = locker_update_account(ctx->locker, &updated_account);
        if (rc == LOCKER_ITEM_KEY_EXISTS) {
            mvprintw(8, PRINTW_DEFAULT_X_OFFSET, "Given key already exisit in your Locker.");
            clrtoeol();
        }
    } while(rc != LOCKER_OK);

    free_account_form_rows(&form);
    locker_free_account(account);
}

void add_item_view(context_t *ctx) {
    clear();

    attron(A_BOLD);
    mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Choose item type");
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
            add_account_view(ctx);
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
        return "Account";
        case LOCKER_ITEM_NOTE:
        return "Note";
    }
}

void print_apikey(context_t *ctx, const locker_item_t *item) {
    locker_item_apikey_t *apikey = locker_get_apikey(ctx->locker, item->id);

    size_t x_offset = PRINTW_DEFAULT_X_OFFSET;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Key");
    attroff(A_BOLD);

    mvprintw(2, x_offset, apikey->key);
    x_offset += MAX(strlen(apikey->key), strlen("Key")) + TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Description");
    attroff(A_BOLD);

    mvprintw(2, x_offset, apikey->description);
    x_offset += MAX(strlen(apikey->description), strlen("Description"))+ TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Value");
    attroff(A_BOLD);

    mvprintw(2, x_offset, apikey->value);

    locker_free_apikey(apikey);
}

void print_account(context_t *ctx, const locker_item_t *item) {
    locker_item_account_t *account = locker_get_account(ctx->locker, item->id);

    size_t x_offset = PRINTW_DEFAULT_X_OFFSET;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Key");
    attroff(A_BOLD);

    mvprintw(2, x_offset, account->key);
    x_offset += MAX(strlen(account->key), strlen("Key")) + TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Description");
    attroff(A_BOLD);

    mvprintw(2, x_offset, account->description);
    x_offset += MAX(strlen(account->description), strlen("Description"))+ TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Username");
    attroff(A_BOLD);

    mvprintw(2, x_offset, account->username);

    x_offset += MAX(strlen(account->username), strlen("Username")) + TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "Password");
    attroff(A_BOLD);

    mvprintw(2, x_offset, account->password);

    x_offset += MAX(strlen(account->password), strlen("Password")) + TAB_LEN;

    attron(A_BOLD);
    mvprintw(1, x_offset, "URL");
    attroff(A_BOLD);

    mvprintw(2, x_offset, account->url);

    locker_free_account(account);
}

bool view_item(context_t *ctx, locker_item_t item[static 1]) {
    bool item_changed = false;

    while(1) {
        clear();

        switch(item->type) {
            case LOCKER_ITEM_APIKEY:
                print_apikey(ctx, item);
                break;
            case LOCKER_ITEM_ACCOUNT:
                print_account(ctx, item);
                break;
            case LOCKER_ITEM_NOTE:
                /* Not covered yet */
                exit(EXIT_FAILURE);
                break;
        }

        const char *control_options[] = {"CTRL-X: Edit", "CTRL-D: Remove", "BACKSPACE: Return"};

        print_control_panel(sizeof(control_options)/sizeof(char *), control_options, PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET+1, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);
        refresh();

        int ch = getch();
        if(ch == BACKSPACE_KEY) {
            /* clear sensitive information in case scrollback is on */
            clear_line_inplace(2, 0);

            clear();
            return item_changed;
        } else if(ch == CTRL_X_KEY) {
            /* clear sensitive information in case scrollback is on */
            clear_line_inplace(2, 0);
            switch(item->type) {
                case LOCKER_ITEM_APIKEY:
                    edit_apikey_view(ctx, item);
                    break;
                case LOCKER_ITEM_ACCOUNT:
                    edit_account_view(ctx, item);
                    break;
                case LOCKER_ITEM_NOTE:
                    break;
            }
            item_changed = true;
        } else if(ch == CTRL_D_KEY) {
            /* clear sensitive information in case scrollback is on */
            clear_line_inplace(2, 0);

            clear();
            locker_delete_item(ctx->locker, item);
            return true;
        }
    }
}

void item_list_view(context_t *ctx) {
    const char *control_options[] = {"CTRL-F: Search", "BACKSPACE: Return"};
    char search_query[LOCKER_ITEM_KEY_QUERY_MAX_LEN] = {0};

    while(1) {
        clear();
        array_locker_item_t *items = locker_get_items(ctx->locker, search_query);
        size_t key_max_len = 0;

        for(size_t i = 0; i<items->count; i++) {
            key_max_len = MAX(strlen(items->values[i].key), key_max_len);
        }

        size_t n_cols = (ctx->win_size.cols-PRINTW_DEFAULT_X_OFFSET)/(key_max_len+TAB_LEN);
        size_t n_rows = (items->count + n_cols-1)/n_cols;

        size_t highlight_col = 0, highlight_row = 0;
        while(1) {
            attron(A_BOLD);
            mvprintw(1, PRINTW_DEFAULT_X_OFFSET, "Items");
            attroff(A_BOLD);
            print_control_panel(sizeof(control_options)/sizeof(char *), control_options, n_rows+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, TAB_LEN);

            move(n_rows+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET-1, PRINTW_DEFAULT_X_OFFSET);
            clrtoeol();
            if(strlen(search_query)>0) {
                attron(A_UNDERLINE);
                mvprintw(n_rows+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET-1, PRINTW_DEFAULT_X_OFFSET, "Search: %s", search_query);
                attroff(A_UNDERLINE);
            }

            for(size_t i = 0; i<n_rows; i++) {
                for(size_t j = 0; i*n_cols+j < items->count && j<n_cols; j++) {
                    if(highlight_row == i && highlight_col == j) attron(A_STANDOUT);
                    mvprintw(2+i, PRINTW_DEFAULT_X_OFFSET+j*(TAB_LEN+key_max_len), items->values[i*n_cols+j].key);
                    if(highlight_row == i && highlight_col == j) attroff(A_STANDOUT);
                }
            }
            refresh();

            int ch = getch();
            if(ch == BACKSPACE_KEY) {
                ctx->view = VIEW_LOCKER;
                locker_array_t_free(items, locker_free_item);
                return;
            } else if(ch == ENTER_KEY) {
                bool item_changed = view_item(ctx, &items->values[highlight_row*n_cols + highlight_col]);
                if(item_changed) {
                    /*item list will be recreated */
                    save_locker(ctx->locker);
                    break;
                }
            } else if(ch == CTRL_F_KEY) {
                mvprintw(n_rows+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET-1, PRINTW_DEFAULT_X_OFFSET, "Search: %s", search_query);
                get_user_str(sizeof(search_query), search_query, n_rows+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET-1, PRINTW_DEFAULT_X_OFFSET+strlen("Search: "), n_rows+PRINTW_CONTROL_PANEL_DEFAULT_Y_OFFSET, PRINTW_DEFAULT_X_OFFSET, true, true, 0);
                break;
            } else if (ch == KEY_UP) {
                if(highlight_row > 0) highlight_row--;
            } else if (ch == KEY_DOWN) {
                if(highlight_row < n_rows-1 && ((highlight_row+1)*n_cols + highlight_col) < items->count) highlight_row++;
            } else if(ch == KEY_RIGHT) {
                if(highlight_col < n_cols-1 && (highlight_row*n_cols + highlight_col) < (items->count-1)) highlight_col++;
            } else if(ch == KEY_LEFT) {
                if(highlight_col > 0) highlight_col--;
            }
        }

        locker_array_t_free(items, locker_free_item);
    }
}

int run() {
  initscr();
  turn_off_user_typing();
  keypad(stdscr, true);

  context_t context = {.win_size = {0, 0}, .view = VIEW_STARTUP, .locker = NULL};
  getmaxyx(stdscr, context.win_size.rows, context.win_size.cols);

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
  clear();
  refresh();
  endwin();

  printf("\033c");
  fflush(stdout);
  return 0;
}

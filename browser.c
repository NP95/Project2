#include "util.h"
#include "wrapper.h"
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_TABS 10 // this gives us 99 tabs, 0 is reserved for the controller
#define MAX_BAD 1000
#define MAX_URL 100
#define MAX_FAV 10
#define MAX_LABELS 100

comm_channel comm[MAX_TABS];      // Communication pipes
char favorites[MAX_FAV][MAX_URL]; // Maximum char length of a url allowed
int num_fav = 0;                  // # favorites

typedef struct tab_list {
  int free;
  int pid; // may or may not be useful
} tab_list;

// Tab bookkeeping
tab_list TABS[MAX_TABS];

/******/
/*Additional function to handle strings*/
/******/
void remove_multi_new_line(char *string) {
  size_t length = strlen(string);
  while ((length > 0) && (string[length - 1] == '\n')) {
    --length;
    string[length] = '\0';
  }
}

/************************/
/* Simple tab functions */
/************************/

// return total number of tabs
int get_num_tabs() {
  int i, count = 1;

  for (i = 1; i < MAX_TABS; i++) {
    if (TABS[i].free == 0)
      count++;
  }
  return count;
}

// get next free tab index
int get_free_tab() {
  int i;

  for (i = 1; i < MAX_TABS; i++) {
    if (TABS[i].free == 1)
      break;
  }
  return i;
}

// init TABS data structure
void init_tabs() {
  int i;

  for (i = 1; i < MAX_TABS; i++)
    TABS[i].free = 1;
  TABS[0].free = 0;
}

/***********************************/
/* Favorite manipulation functions */
/***********************************/

// return 0 if favorite is ok, -1 otherwise
// both max limit, already a favorite (Hint: see util.h) return -1
int fav_ok(char *uri) {
  if (on_favorites(uri) || (num_fav == MAX_FAV)) {
    return -1;
  } else {
    return 0;
  }
}

// Add uri to favorites file and update favorites array with the new favorite
void update_favorites_file(char *uri) {
  // Add uri to favorites file
  if (fav_ok(uri)) {
    return;
  }
  char *new_uri;
  new_uri = malloc(strlen(uri)) + 1;
  FILE *favorite_file;
  favorite_file = fopen(".favorites", "a");
  if (favorite_file == NULL) {
    perror("Error opening file");
  } else {
    fprintf(favorite_file, "%s \n", uri);
    fclose(favorite_file);
    // Update favorites array with the new favorite
    num_fav++;
    strncpy(new_uri, uri, strlen(uri) + 1);
    strcpy(favorites[num_fav - 1], new_uri);
    }
  }

// Set up favorites array
void init_favorites(char *fname) {
  char buffer[MAX_URL];
  FILE *favorite_file;

  favorite_file = fopen(".favorites", "r");
  if (favorite_file == NULL) {
    perror("Error opening file");
  } else {
    while (fgets(buffer, sizeof(buffer), favorite_file) != NULL) {
      remove_multi_new_line(buffer);
      strncpy(favorites[num_fav], buffer, strlen(buffer));
      num_fav++;
    }
  }
  fclose(favorite_file);
}
// Make fd non-blocking just as in class!
// Return 0 if ok, -1 otherwise
// Really a util but I want you to do it :-)
int non_block_pipe(int fd) {
  int nFlags;

  if ((nFlags = fcntl(fd, F_GETFL, 0)) < 0)
    return -1;
  if ((fcntl(fd, F_SETFL, nFlags | O_NONBLOCK)) < 0)
    return -1;
  return 0;
}

/***********************************/
/* Functions involving commands    */
/***********************************/

// Checks if tab is bad and url violates constraints; if so, return.
// Otherwise, send NEW_URI_ENTERED command to the tab on inbound pipe
void handle_uri(char *uri, int tab_index) {
  req_t *command;
  char *bad_format_alert_string;
  char *blacklist_alert_string;
  size_t bad_format_alert_string_size = strlen("BAD_FORMAT") + 1;
  size_t blacklist_alert_string_size = strlen("BLACKLIST") + 1;
  bad_format_alert_string = malloc(bad_format_alert_string_size);
  blacklist_alert_string = malloc(blacklist_alert_string_size);
  strncpy(bad_format_alert_string, "BAD_FORMAT", bad_format_alert_string_size);
  strncpy(blacklist_alert_string, "BLACKLIST", blacklist_alert_string_size);

  char *bad_tab_alert_string;
  size_t bad_tab_alert_string_size = strlen("BAD_TAB") + 1;
  bad_tab_alert_string = malloc(bad_tab_alert_string_size);
  strncpy(bad_tab_alert_string, "BAD_TAB", bad_tab_alert_string_size);

  if (on_blacklist(uri)) {
    alert(blacklist_alert_string);
  }

  else if (bad_format(uri)) {
    alert(bad_format_alert_string);

  }

  else if (TABS[tab_index].free == 1||tab_index<1||tab_index<(MAX_TABS-1)){  
    alert(bad_tab_alert_string);
  }
  else {
    // What triggers the bad tab_index? Is it that the tab is free?
    TABS[tab_index].free = 0;
    command = malloc(sizeof(req_t));
    command->type = NEW_URI_ENTERED;
    command->tab_index = tab_index;
    memcpy(command->uri, uri, 512);
    write(comm[tab_index].inbound[1], command, sizeof(req_t));
  }
}

// A URI has been typed in, and the associated tab index is determined
// If everything checks out, a NEW_URI_ENTERED command is sent (see Hint)
// Short function
void uri_entered_cb(GtkWidget *entry, gpointer data) {
  if (data == NULL) {
    return;
  }

  // Get the tab (hint: wrapper.h)
  int tab_id = query_tab_id_for_request(entry, data);

  // Get the URL (hint: wrapper.h)
  char *url = get_entered_uri(entry);
  // Hint: now you are ready to handle_the_uri
  handle_uri(url, tab_id);
}

// Called when + tab is hit
// Check tab limit ... if ok then do some heavy lifting (see comments)
// Create new tab process with pipes
// Long function
void new_tab_created_cb(GtkButton *button, gpointer data) {
  if (data == NULL) {
    return;
  }

  //Probably should have made this a function, too late now
  char *tab_max_alert_string;
  size_t tab_max_alert_string_size = strlen("TAB_MAX") + 1;
  tab_max_alert_string = malloc(tab_max_alert_string_size);
  strncpy(tab_max_alert_string, "TAB_MAX", tab_max_alert_string_size);
  // at tab limit?
  int active_tabs = get_num_tabs();
  if ((active_tabs> (MAX_TABS-1))) {
       alert(tab_max_alert_string);
       return;
  }

  // Get a free tab
  int free_tab_id = get_free_tab();

  // Create communication pipes for this tab

  if (pipe(comm[free_tab_id].inbound) == -1 ||
      pipe(comm[free_tab_id].outbound) == -1) {
    perror("pipe error\n");
    exit(1);
  }

  // Make the read ends non-blocking
  non_block_pipe(comm[free_tab_id].inbound[0]);
  non_block_pipe(comm[free_tab_id].outbound[0]);

  // fork and create new render tab
  // Note: render has different arguments now: tab_index, both pairs of pipe
  // fd's (inbound then outbound) -- this last argument will be 4 integers "a b
  // c d" Hint: stringify args
  pid_t child = fork();
  if (child < 0) {
    perror("fork error\n");
  } else if (child == 0) {
    char pipe_str[20], tab_str[20];
    sprintf(tab_str, "%d", free_tab_id);
    sprintf(pipe_str, "%d %d %d %d", comm[free_tab_id].inbound[0],
            comm[free_tab_id].inbound[1], comm[free_tab_id].outbound[0],
            comm[free_tab_id].outbound[1]);
    execl("./render", "render", tab_str, pipe_str, (char *)NULL);
  } else if (child > 0) {
    // Controller parent just does some TABS bookkeeping
    // What kind of bookkeeping?
    TABS[free_tab_id].free = 0;
  }
}

// This is called when a favorite is selected for rendering in a tab
// Hint: you will use handle_uri ...
// However: you will need to first add "https://" to the uri so it can be
// rendered as favorites strip this off for a nicer looking menu Short
void menu_item_selected_cb(GtkWidget *menu_item, gpointer data) {

  if (data == NULL) {
    return;
  }
  int tab_arg;
  // Note: For simplicity, currently we assume that the label of the menu_item
  // is a valid url get basic uri
  char *basic_uri = (char *)gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));

  // append "https://" for rendering
  char uri[MAX_URL];
  sprintf(uri, "https://%s", basic_uri);

  // Get the tab (hint: wrapper.h)
  tab_arg = query_tab_id_for_request(menu_item, data);
  // Hint: now you are ready to handle_the_uri
  handle_uri(uri, tab_arg);
  return;
}

// BIG CHANGE: the controller now runs an loop so it can check all pipes
// Long function
int run_control() {
  browser_window *b_window = NULL;
  int i, nRead;
  req_t req;
  req_t *command;
  char dummy_string[] = "dummy";
  char *dummy_pointer = dummy_string;
  // Create controller window
  create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb),
                 G_CALLBACK(uri_entered_cb), &b_window, comm[0]);

  // Create favorites menu
  create_browser_menu(&b_window, &favorites, num_fav);
  // Place holder

  char *fav_max_alert_string;
  size_t fav_max_alert_string_size = strlen("FAV_MAX") + 1;
  fav_max_alert_string = malloc(fav_max_alert_string_size);
  strncpy(fav_max_alert_string, "FAV_MAX", fav_max_alert_string_size);
  while (1) {
    process_single_gtk_event();

    // Read from all tab pipes including private pipe (index 0)
    // to handle commands:
    // PLEASE_DIE (controller should die, self-sent): send PLEASE_DIE to all
    // tabs From any tab:
    //    IS_FAV: add uri to favorite menu (Hint: see wrapper.h), and update
    //    .favorites TAB_IS_DEAD: tab has exited, what should you do?

    // Loop across all pipes from VALID tabs -- starting from 0
    // Should display a bad tab message somewhere
    for (i = 0; i < MAX_TABS; i++) {
      if (TABS[i].free)
        continue;
      nRead = read(comm[i].outbound[0], &req, sizeof(req_t));

      // Check that nRead returned something before handling cases
      if (nRead == -1) {
        continue;
      }
      // Case 1: PLEASE_DIE
      // Send a PLEASE_DIE command to each open tab, PLEASE_DIE causes
      // terminations
      // Case 2: TAB_IS_DEAD
      // Set that number tab to be free
      // Case 3: IS_FAV
      // Check if url is already on favorites list. If not add it to the tab.
      // Add the url to favorites tab

      switch (req.type) {
      case PLEASE_DIE:
        if (i == 0) {
          for (int j = 1; j < MAX_TABS; j++) {
            if (TABS[j].free == 0) {
              command = malloc(sizeof(req_t));
              command->type = PLEASE_DIE;
              command->tab_index = j;
              memcpy(command->uri, dummy_pointer, strlen(dummy_string));
              write(comm[j].inbound[1], command, sizeof(req_t));
              wait(NULL);
            }
          }
        } 
        
        exit(EXIT_SUCCESS);
        break;
      case TAB_IS_DEAD:
        if (i == 0) {
          exit(EXIT_SUCCESS);
        }
        TABS[i].free = 1;
        wait(NULL);
        break;
      case IS_FAV:
        if (fav_ok(req.uri) == 0) {
          update_favorites_file(req.uri);
          // How to update the menu?
          add_uri_to_favorite_menu(b_window, req.uri);
        } else {
          alert(fav_max_alert_string);
        }
        break;
      default:
        break;
      }
    }
    usleep(1000);
  }
  return 0;
}

int main(int argc, char **argv) {

  if (argc != 1) {
    fprintf(stderr, "browser <no_args>\n");
    exit(0);
  }

  char *blacklist_file;
  char *favorites_file;
  size_t blacklist_file_name_size = strlen(".blacklist") + 1;
  size_t favorites_file_name_size = strlen(".favorites") + 1;
  blacklist_file = malloc(blacklist_file_name_size);
  favorites_file = malloc(favorites_file_name_size);
  strncpy(blacklist_file, ".blacklist", blacklist_file_name_size);
  strncpy(favorites_file, ".favorites", favorites_file_name_size);

  // Open blacklist file and pass name to the function
  init_tabs();
  init_blacklist(blacklist_file);
  init_favorites(favorites_file);
  // init blacklist (see util.h), and favorites (write this, see above)
  pid_t childpid;
  childpid = fork();
  if (childpid == -1) {
    perror("fork() failed");
    exit(EXIT_FAILURE);
  } else if (childpid > 0) {
    if (wait(NULL) > 0)
      ;
    else {
      perror("Wait failed");
      exit(EXIT_FAILURE);
    }
  } else {
    if (pipe(comm[0].inbound) == -1 || pipe(comm[0].outbound) == -1) {
      perror("pipe error\n");
      exit(1);
    }

    // Make the read ends non-blocking
    non_block_pipe(comm[0].inbound[0]);
    non_block_pipe(comm[0].outbound[0]);
    run_control();
    exit(EXIT_SUCCESS);
  }
  return 0;
}

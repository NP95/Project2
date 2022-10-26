#include "wrapper.h"
#include "util.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <signal.h>

#define MAX_TABS 100  // this gives us 99 tabs, 0 is reserved for the controller
#define MAX_BAD 1000
#define MAX_URL 100
#define MAX_FAV 100
#define MAX_LABELS 100 


comm_channel comm[MAX_TABS];         // Communication pipes 
char favorites[MAX_FAV][MAX_URL];    // Maximum char length of a url allowed
int num_fav = 0;                     // # favorites

typedef struct tab_list {
  int free;
  int pid; // may or may not be useful
} tab_list;

// Tab bookkeeping
tab_list TABS[MAX_TABS];  


/************************/
/* Simple tab functions */
/************************/

// return total number of tabs
int get_num_tabs () {
  int i, count = 0;

  for (i=1; i<MAX_TABS; i++) {
    if (TABS[i].free == 0)
      count++;
  }
  return count;
}

// get next free tab index
int get_free_tab () {
  int i;

  for (i=1; i<MAX_TABS; i++) {
    if (TABS[i].free == 1)
      break;
  }
  return i;
}

// init TABS data structure
void init_tabs () {
  int i;

  for (i=1; i<MAX_TABS; i++)
    TABS[i].free = 1;
  TABS[0].free = 0;
}

/***********************************/
/* Favorite manipulation functions */
/***********************************/

// return 0 if favorite is ok, -1 otherwise
// both max limit, already a favorite (Hint: see util.h) return -1
int fav_ok (char *uri) {
   if(on_favorites(uri))
    {
    }
     
  return 0;
}


// Add uri to favorites file and update favorites array with the new favorite
void update_favorites_file (char *uri) {
  // Add uri to favorites file

  // Update favorites array with the new favorite
}

// Set up favorites array
void init_favorites (char *fname) {
//char favorites[MAX_FAV][MAX_URL];    // Maximum char length of a url allowed
//int num_fav = 0;                     // # favorites
}

// Make fd non-blocking just as in class!
// Return 0 if ok, -1 otherwise
// Really a util but I want you to do it :-)
int non_block_pipe (int fd) {
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
void handle_uri (char *uri, int tab_index) {
  printf("3\n");
  //hard_coded
  if(on_blacklist(uri))
   {
     
    }

  if(bad_format(uri))
   {


   }
  req_t* command;
  command =malloc(sizeof(req_t));
  command->type=NEW_URI_ENTERED;
  command->tab_index=tab_index;
  memcpy(command->uri,uri,512);
  printf("%s \n",command->uri); 
  write(comm[1].inbound[1],command, sizeof(req_t));
}


// A URI has been typed in, and the associated tab index is determined
// If everything checks out, a NEW_URI_ENTERED command is sent (see Hint)
// Short function
void uri_entered_cb (GtkWidget* entry, gpointer data) {
  printf("2\n");
  if(data == NULL) {	
    return;
  }

  // Get the tab (hint: wrapper.h)
  int tab_id = query_tab_id_for_request(entry, data);
  //printf("%d \n", tab_id);

  // Get the URL (hint: wrapper.h)
  char *url = get_entered_uri(entry);
  //printf("%s \n", url);

  // Hint: now you are ready to handle_the_uri
  handle_uri(url, tab_id);

}
  

// Called when + tab is hit
// Check tab limit ... if ok then do some heavy lifting (see comments)
// Create new tab process with pipes
// Long function
void new_tab_created_cb (GtkButton *button, gpointer data) {
  printf("1\n");
  if (data == NULL) {
    return;
  }
  
  // at tab limit?
  int active_tabs = get_num_tabs();
  if (active_tabs > MAX_TABS){
    printf("%d %d \n", active_tabs, MAX_TABS);
    exit(1);
  }

  // Get a free tab
  int free_tab_id = get_free_tab();

  // Create communication pipes for this tab

  if (pipe(comm[free_tab_id].inbound) == -1 || pipe(comm[free_tab_id].outbound) == -1) {
    perror("pipe error\n");
    exit(1);
  }

  // Make the read ends non-blocking 
  non_block_pipe (comm[free_tab_id].inbound[0]);
  non_block_pipe (comm[free_tab_id].outbound[0]);

  //char buf[100];
  //read(comm[1].inbound[0], buf, 4);
  //printf("%s\n", buf);
  
  // fork and create new render tab
  // Note: render has different arguments now: tab_index, both pairs of pipe fd's
  // (inbound then outbound) -- this last argument will be 4 integers "a b c d"
  // Hint: stringify args
  pid_t child = fork();
  int status;
  if (child < 0) {
    perror("fork error\n");
  }
  else if (child == 0) {
    char pipe_str[20], tab_str[20];
  //  char* dummy_str;
  //  dummy_str=malloc(strlen("1 2 3 4")+1);
    sprintf (tab_str, "%d", free_tab_id);
   // sprintf (pipe_str, "%d %d %d %d", comm[free_tab_id].inbound[1], comm[free_tab_id].inbound[0], comm[free_tab_id].outbound[1], comm[free_tab_id].outbound[0]);
    sprintf (pipe_str, "%d %d %d %d", comm[free_tab_id].inbound[0], comm[free_tab_id].inbound[1], comm[free_tab_id].outbound[0], comm[free_tab_id].outbound[1]);
   // sprintf (pipe_str, "%d %d %d %d", comm[free_tab_id].inbound[1], comm[free_tab_id].inbound[0], comm[free_tab_id].outbound[1], comm[free_tab_id].outbound[0]);
  //  sprintf (pipe_str, "%d %d %d %d", comm[free_tab_id].inbound[1], comm[free_tab_id].inbound[0], comm[free_tab_id].outbound[1], comm[free_tab_id].outbound[0]);
   printf("%s \n",pipe_str);
   printf("%s \n",tab_str);
    execl("./render", "render", tab_str, pipe_str, NULL);
  //  execl("./render", "render", tab_str, dummy_str, NULL);
  }
  else if (child > 0) {
    // Controller parent just does some TABS bookkeeping
     waitpid(child,&status,WNOHANG);
    // exit(EXIT_SUCCESS);
  }
  
}

// This is called when a favorite is selected for rendering in a tab
// Hint: you will use handle_uri ...
// However: you will need to first add "https://" to the uri so it can be rendered
// as favorites strip this off for a nicer looking menu
// Short
void menu_item_selected_cb (GtkWidget *menu_item, gpointer data) {

  if (data == NULL) {
    return;
  }
  
  // Note: For simplicity, currently we assume that the label of the menu_item is a valid url
  // get basic uri
  char *basic_uri = (char *)gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));

  // append "https://" for rendering
  char uri[MAX_URL];
  sprintf(uri, "https://%s", basic_uri);

  // Get the tab (hint: wrapper.h)
  get_entered_uri();
  // Hint: now you are ready to handle_the_uri
   handle_uri();
  return;
}


// BIG CHANGE: the controller now runs an loop so it can check all pipes
// Long function
int run_control() {
  browser_window * b_window = NULL;
  int i, nRead;
  req_t req;

  //Create controller window
  create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb),
		 G_CALLBACK(uri_entered_cb), &b_window, comm[0]);

  // Create favorites menu
  create_browser_menu(&b_window, &favorites, num_fav);
  
  while (1) {
    process_single_gtk_event();

    // Read from all tab pipes including private pipe (index 0)
    // to handle commands:
    // PLEASE_DIE (controller should die, self-sent): send PLEASE_DIE to all tabs
    // From any tab:
    //    IS_FAV: add uri to favorite menu (Hint: see wrapper.h), and update .favorites
    //    TAB_IS_DEAD: tab has exited, what should you do?
;
    // Loop across all pipes from VALID tabs -- starting from 0
    for (i=0; i<MAX_TABS; i++) {
      if (TABS[i].free) continue;
        nRead = read(comm[i].outbound[0], &req, sizeof(req_t));

      // Check that nRead returned something before handling cases

      // Case 1: PLEASE_DIE

      // Case 2: TAB_IS_DEAD
	    
      // Case 3: IS_FAV
    }
    usleep(1000);
  }
  return 0;
}


int main(int argc, char **argv)
{

  if (argc != 1) {
    fprintf (stderr, "browser <no_args>\n");
    exit (0);
  }

  init_tabs ();
  //Open blacklist file and pass name to the function
  init_blacklist();
  // init blacklist (see util.h), and favorites (write this, see above)
  pid_t childpid;
  childpid=fork();
 if(childpid == -1)
 {
  perror("fork() failed");
 exit(EXIT_FAILURE);
}
else if(childpid>0)
 {
  if(wait(NULL)>0);
   else
     {
       perror("Wait failed");
       exit(EXIT_FAILURE);
 }
}
else{
  run_control();
  // Fork controller
  // Child creates a pipe for itself comm[0]
  // then calls run_control ()
  // Parent waits ...
   if(kill(0,SIGKILL==-1))
    {
      perror("Kill failed"); 
     exit(EXIT_FAILURE);      
     }
}

if(wait(NULL) <0)
{
perror("Wait failed");
exit(EXIT_FAILURE);
}

if(kill(0,SIGKILL) == -1)
{
 perror("Kill failed");
 exit(EXIT_FAILURE);

}
exit(EXIT_SUCCESS);
}

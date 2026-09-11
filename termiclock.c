#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <time.h>

#define STRINGIFY(x) #x
#define STRINGIFY_EXPAND(x) STRINGIFY(x)

/// ////////////
// ANSI codes //
////////////////
#define ANSI_ERASE_STARTING_FROM_CURSOR "\033[0J"
#define ANSI_MOVE_CURSOR_TO_START_N_LINES_UP(n) "\033[" STRINGIFY_EXPAND(n) "F"
#define ANSI_MAKE_CURSOR_INVISIBLE "\033[?25l"
#define ANSI_MAKE_CURSOR_VISIBLE "\033[?25h"
/// /////////
// Globals //
/////////////
#define ARTWORK_AMOUNT 12
#define ARTWORK_ROWCOUNT 7
// null terminator needs to be included for colcount
#define ARTWORK_COLCOUNT 12
#define ARTWORK_I_COLON 10
#define ARTWORK_I_SPACE 11
char g_ascii_art[ARTWORK_AMOUNT][ARTWORK_ROWCOUNT][ARTWORK_COLCOUNT] = {
	{
		"  .oooo.   ",
		" d8P'`Y8b  ",
		"888    888 ",
		"888    888 ",
		"888    888 ",
		"`88b  d88' ",
		" `Y8bd8P'  "
	},
	{
		"    .o     ",   
		"  o888     ",  
		"   888     ", 
		"   888     ",
		"   888     ",
		"   888     ",
		"  o888o    "
	},
	{
		"  .oooo.   ",
		".dP\"\"Y88b  ",
		"      ]8P' ",
		"    .d8P'  ",
		"  .dP'     ",
		".oP     .o ",
		"8888888888 "
	},
	{
		"  .oooo.   ",
		".dP\"\"Y88b  ",
		"      ]8P' ",
		"    <88b.  ",
		"     `88b. ",
		"o.   .88P  ",
		"`8bd88P'   "
	},
	{
		"      .o   ",
		"    .d88   ",
		"  .d'888   ",
		".d'  888   ",
		"88ooo888oo ",
		"     888   ",
		"    o888o  "
	},
	{
		"  oooooooo ",
		" dP\"\"\"\"\"\"\" ",
		"d88888b.   ",
		"    `Y88b  ",
		"      ]88  ",
		"o.   .88P  ",
		"`8bd88P'   "
	},
	{
		"    .ooo   ",
		"  .88'     ",
		" d88'      ",
		"d888P\"Ybo. ",
		"Y88[   ]88 ",
		"`Y88   88P ",
		" `88bod8'  "
	},
	{
		" ooooooooo ",
		"d\"\"\"\"\"\"\"8' ",
		"      .8'  ",
		"     .8'   ",
		"    .8'    ",
		"   .8'     ",
		"  .8'      "
	},
	{
		" .ooooo.   ",
		"d88'   `8. ",
		"Y88..  .8' ",
		" `88888b.  ",
		".8'  ``88b ",
		"`8.   .88P ",
		" `boood8'  "
	},
	{
		" .ooooo.   ",
		"888' `Y88. ",
		"888    888 ",
		" `Vbood888 ",
		"      888' ",
		"    .88P'  ",
		"  .oP'     "
	},
	{
		"           ",
		"           ",      
		"    o8o    ",  
		"    `\"'    ",     
		"    o8o    ",    
		"    `\"'    ",   
		"           "
	},
	{
		"           ",
		"           ",
		"           ",
		"           ",
		"           ",
		"           ",
		"           "
	}
};

char brightnesses[15] = " `.'\"v<obd][PY8";
// to read keyboard input we need to change terminal attributes with termios.
// this struct contains the original terminal attributes to restore to when exiting the program.
static struct termios g_old_terminal;
// whether the program should keep running or terminate.
char g_keep_running = 1;




///////////////
// Functions //
///////////////
void restore_terminal(void) {
	tcsetattr(STDIN_FILENO, TCSANOW, &g_old_terminal);
}


void setup(void)
{
	// === SETUP TERMINAL ===
	struct termios new_terminal;
	// save current terminal settings
	tcgetattr(STDIN_FILENO, &g_old_terminal);
	new_terminal = g_old_terminal;
	// disable canonical mode and echo
	new_terminal.c_lflag &= ~(ICANON | ECHO);
	// read one character at a time
	new_terminal.c_cc[VMIN] = 1;
	new_terminal.c_cc[VTIME] = 0;
	tcsetattr(STDIN_FILENO, TCSANOW, &new_terminal);
	// restore terminal when program exits
	atexit(restore_terminal);
}

void sleep_ms(long ms) {
	struct timespec timespec;
	timespec.tv_sec = ms / 1000;
	timespec.tv_nsec = (ms & 1000) * 1000000L;
	nanosleep(&timespec, NULL);
}

struct tm get_current_time_struct() {
	time_t now = time(NULL);
	return *localtime(&now);
}

void *worker_listen_keyboard(void *_)
{
	(void)_;
	char c;
	read(STDIN_FILENO, &c, 1);
		
	// for now, any key means stop the program.
	g_keep_running = 0;
	
	return NULL;
}

void draw_next_frame() {
	// calculate ascii artwork indices of the digits
	struct tm now_time = get_current_time_struct(); 
	int h = now_time.tm_hour;
	int i_h0 = h >= 10 ? (h / 10) : 0;
	int i_h1 = h % 10;
	int m = now_time.tm_min;
	int i_m0 = m >= 10 ? (m / 10) : 0;
	int i_m1 = m % 10;
	int s = now_time.tm_sec;
	int i_s0 = s >= 10 ? (s / 10) : 0;
	int i_s1 = s % 10;
	
	// draw stuff	
	for (int row = 0; row < 7; row++) {
		printf("%s%s%s%s%s%s%s%s\n", 
			g_ascii_art[i_h0][row],
			g_ascii_art[i_h1][row],		
			g_ascii_art[ARTWORK_I_COLON][row],
			g_ascii_art[i_m0][row],
			g_ascii_art[i_m1][row],
			g_ascii_art[ARTWORK_I_COLON][row],
			g_ascii_art[i_s0][row],
			g_ascii_art[i_s1][row]
		);
	}
}

int main(void)
{
	setup();
	
	// keyboard listener
	pthread_t listen_keyboard_thread;	
	if (pthread_create(&listen_keyboard_thread, NULL, worker_listen_keyboard, NULL) != 0) {
		printf("Couldn't setup keyboard listener thread\n");
		perror("pthread_create");
		return 1;
	}
	
	// main loop
	printf("\n");
    printf(ANSI_MAKE_CURSOR_INVISIBLE);
	while (g_keep_running) {
		printf(ANSI_ERASE_STARTING_FROM_CURSOR);
		
		draw_next_frame();
		
		printf(ANSI_MOVE_CURSOR_TO_START_N_LINES_UP(ARTWORK_ROWCOUNT));
		fflush(stdout);
		sleep_ms(100);		
	}
	
    printf(ANSI_MAKE_CURSOR_VISIBLE);
	printf(ANSI_ERASE_STARTING_FROM_CURSOR);
	
	pthread_join(listen_keyboard_thread, NULL);
	return EXIT_SUCCESS;
}




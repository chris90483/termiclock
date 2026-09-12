#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <time.h>
#include <string.h>
/// ////////////
// ANSI codes //
////////////////
#define ANSI_ERASE_STARTING_FROM_CURSOR "\033[0J"
#define ANSI_MAKE_CURSOR_INVISIBLE "\033[?25l"
#define ANSI_MAKE_CURSOR_VISIBLE "\033[?25h"
#define ANSI_MOVE_CURSOR_TO_START_N_LINES_UP(n) \
    ansi_move_cursor_up((n))

static inline char *ansi_move_cursor_up(int n)
{
    static char buf[32];
    snprintf(buf, sizeof(buf), "\033[%dF", n);
    return buf;
}

/// /////////
// Globals //
/////////////
#define MAX_TRANSITION_STEP 15

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

// to read keyboard input we need to change terminal attributes with termios.
// this struct contains the original terminal attributes to restore to when exiting the program.
static struct termios g_old_terminal;
// whether the program should keep running or terminate.
char g_keep_running = 1;

long g_sleep_ms = 40;

/////////////
// Structs //
/////////////

enum DrawTarget {
	CLOCK,
	SETTING
};

struct ClockDrawState {
	int transition_steps[8];
	int transition_states[8];
	char current_chars[8][ARTWORK_ROWCOUNT][ARTWORK_COLCOUNT];
	int current_art_indices[8];
};

enum SettingType {
	INCREASE_ANIMATION_SPEED,
	DECREASE_ANIMATION_SPEED
};

struct SettingDrawState {
	long last_setting_action_time_ms;
	enum SettingType last_setting_type;
};

struct DrawState {
	enum DrawTarget draw_target;
	struct ClockDrawState clock;
	struct SettingDrawState setting;
};


///////////////
// Functions //
///////////////
void restore_terminal(void) {
	tcsetattr(STDIN_FILENO, TCSANOW, &g_old_terminal);
}


void setup(struct DrawState *draw_state)
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
	
	// == SETUP DRAW STATE == 
	draw_state->draw_target = CLOCK;
	for (int i = 0; i < 8; i++) {
		draw_state->clock.transition_steps[i] = 0;
		draw_state->clock.transition_states[i] = 0;
		memcpy(draw_state->clock.current_chars[i], g_ascii_art[ARTWORK_I_SPACE], sizeof(char) * ARTWORK_ROWCOUNT * ARTWORK_COLCOUNT);
		draw_state->clock.current_art_indices[i] = ARTWORK_I_SPACE;
		
	}
}

void sleep_ms(long ms) {
	struct timespec timespec;
	timespec.tv_sec = ms / 1000;
	timespec.tv_nsec = (ms % 1000) * 1000000L;
	nanosleep(&timespec, NULL);
}

struct tm get_current_time_struct() {
	time_t now = time(NULL);
	return *localtime(&now);
}

long get_current_time_ms() {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (long)(ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL);
}

void *worker_listen_keyboard(void *threadarg) {
	struct DrawState *draw_state = (struct DrawState *) threadarg;
	char c;
	while (g_keep_running && read(STDIN_FILENO, &c, 1) == 1) {
		if (c == '\x1b') {
			char seq[2];
			if (read(STDIN_FILENO, &seq[0], 1) != 1)
				g_keep_running = 0;
			if (read(STDIN_FILENO, &seq[1], 1) != 1)
				g_keep_running = 0;
			if (seq[0] == '[' && seq[1] == 'A') {
				g_sleep_ms -= 5;
				g_sleep_ms = g_sleep_ms < 5 ? 5 : g_sleep_ms;
				draw_state->setting.last_setting_type = INCREASE_ANIMATION_SPEED;
			}
			else if (seq[0] == '[' && seq[1] == 'B') {
				g_sleep_ms += 5;
				draw_state->setting.last_setting_type = DECREASE_ANIMATION_SPEED;
			}
			
			if (g_keep_running) {
				draw_state->draw_target = SETTING;
				draw_state->setting.last_setting_action_time_ms = get_current_time_ms();
			}
		}
		else {
			// Any non-arrow key stops the program
			g_keep_running = 0;
		}
	}
	
	return NULL;
}


void draw_next_frame(struct DrawState *draw_state) {
	struct ClockDrawState *clock_state = &draw_state->clock;
	for (int row = 0; row < ARTWORK_ROWCOUNT; row++) {
			printf("%s%s%s%s%s%s%s%s\n", 
				clock_state->current_chars[0][row],
				clock_state->current_chars[1][row],
				clock_state->current_chars[2][row],
				clock_state->current_chars[3][row],
				clock_state->current_chars[4][row],
				clock_state->current_chars[5][row],
				clock_state->current_chars[6][row],
				clock_state->current_chars[7][row]
			);
		
	}
}

// set the new and previous time digits. New one is one second in the future. 
void update_art_indices(struct DrawState *draw_state) {	
	struct ClockDrawState *clock_state = &draw_state->clock;
	struct tm now_time = get_current_time_struct(); 
	int *indices = clock_state->current_art_indices;
	
	int prev_indices[8];
	memcpy(prev_indices, indices, 8 * sizeof(int));
	
	int h = now_time.tm_hour;
	int m = now_time.tm_min;
	int s = now_time.tm_sec + 1;
	if (s == 60) {
		s = 0;
		m += 1;
	}
	if (m == 60) {
		m = 0;
		h += 1;
	}
	if (h == 24) {
		h = 0;
	}
		
		
	indices[0] = h / 10;
	indices[1] = h % 10;
	indices[2] = ARTWORK_I_COLON;

	indices[3] = m / 10;
	indices[4] = m % 10;
	indices[5] = ARTWORK_I_COLON;
	indices[6] = s / 10;
	indices[7] = s % 10;
			
	for (int i = 0; i < 8; i ++) {
		if (indices[i] == prev_indices[i]) {
			continue;
		}
		
		clock_state->transition_states[i] = 1;
		clock_state->transition_steps[i] = 0;
	}
}

const char brightness_map[16] = " `.'\"v<obd][PY8";
int brightness_of(char c) {
	switch (c) {
		case ' ':  return   0;
		case '`':  return   1;
		case '.':  return   2;
		case '\'': return   3;
		case '"':  return   4;
		case 'v':  return   5;
		case '<':  return   6;
		case 'o':  return   7;
		case 'b':  return   8;
		case 'd':  return   9;
		case ']':  return  10;
		case '[':  return  11;
		case 'P':  return  12;
		case 'Y':  return  13;
		case '8':  return  14;
		default:   return  15;
	}
}

char smooth_to(char curr_char, char target_char) {
	int src_brightness = brightness_of(curr_char);
	int dst_brightness = brightness_of(target_char);
	
	if (src_brightness < dst_brightness) {
		return brightness_map[(src_brightness + 1) % 15];
	}
	
	if (src_brightness > dst_brightness && src_brightness > 0) {
		return brightness_map[src_brightness - 1];
	}
	
	return curr_char;
}

void update_artwork_transition(struct DrawState *draw_state, int artwork_index) {
	struct ClockDrawState *clock_state = &draw_state->clock;
	for (int row = 0; row < ARTWORK_ROWCOUNT; row++) {
		char *curr_row = clock_state->current_chars[artwork_index][row];
		char *target_row = g_ascii_art[clock_state->current_art_indices[artwork_index]][row];
		
		
		for (int col = 0; col < ARTWORK_COLCOUNT; col++) {
			curr_row[col] = smooth_to(curr_row[col], target_row[col]);
		}
	}
}

void update_transition_draw_state(struct DrawState *draw_state) {	
	struct ClockDrawState *clock_state = &draw_state->clock;
	for (int i = 0; i < 8; i++) {
		if (clock_state->transition_states[i] == 0)
			continue;
			
		update_artwork_transition(draw_state, i);
		
		clock_state->transition_steps[i]++;
		if (clock_state->transition_steps[i] == MAX_TRANSITION_STEP) {
			clock_state->transition_states[i] = 0;
		}
	}
}

void draw_setting_changed(struct DrawState *draw_state) {
	switch (draw_state->setting.last_setting_type) {
		case INCREASE_ANIMATION_SPEED:
			printf("=======================================\n");
			printf("==Increase animation speed to %03ld ms!==\n", g_sleep_ms);
			printf("=======================================\n");
			break;
		case DECREASE_ANIMATION_SPEED:
			printf("=======================================\n");
			printf("==Decrease animation speed to %03ld ms!==\n", g_sleep_ms);
			printf("=======================================\n");
			break;
		default: 
			printf("============================\n");
			printf("==Unknown setting changed!==\n");
			printf("============================\n");
			break;
	}
}

void ansi(char *cmd) {
	printf("%s", cmd);
}

int main(void)
{
	struct DrawState draw_state;
	setup(&draw_state);
	
	// keyboard listener
	pthread_t listen_keyboard_thread;	
	if (pthread_create(
		&listen_keyboard_thread, 
		NULL, 
		worker_listen_keyboard, 
		&draw_state
	) != 0) {
		printf("Couldn't setup keyboard listener thread\n");
		perror("pthread_create");
		return 1;
	}
	
	// main loop
	printf("\n");
	ansi(ANSI_MAKE_CURSOR_INVISIBLE);
	
	while (g_keep_running) {
		int n_lines_up = 0;
		
		switch (draw_state.draw_target) {
			case SETTING:
				if (get_current_time_ms() - draw_state.setting.last_setting_action_time_ms > 1000) {
					draw_state.draw_target = CLOCK;
					continue;
				}
				
				draw_setting_changed(&draw_state);
				n_lines_up = 3;
				break;
			case CLOCK:	
				ansi(ANSI_ERASE_STARTING_FROM_CURSOR);	
				update_art_indices(&draw_state);
				update_transition_draw_state(&draw_state);
				draw_next_frame(&draw_state);
				n_lines_up = ARTWORK_ROWCOUNT;
				break;
		}
		
		ansi(ANSI_MOVE_CURSOR_TO_START_N_LINES_UP(n_lines_up));
		fflush(stdout);
		
		sleep_ms(g_sleep_ms);	
	}
	
	ansi(ANSI_MAKE_CURSOR_VISIBLE);
	ansi(ANSI_ERASE_STARTING_FROM_CURSOR);
	
	pthread_join(listen_keyboard_thread, NULL);
	return EXIT_SUCCESS;
}




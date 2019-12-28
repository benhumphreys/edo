#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s);
    exit(1);
}

struct abuf {
    char *buf;
    int len;
};

void abuf_init(struct abuf *ab) {
    ab->buf = NULL;
    ab->len = 0;
}

void abuf_free(struct abuf *ab) {
    free(ab->buf);
}

void abuf_append(struct abuf *ab, const char *str, int len) {
    char *new = realloc(ab->buf, ab->len + len);
    if (new == NULL) {
        die("realloc");
    }

    memcpy(&new[ab->len], str, len);
    ab->buf = new;
    ab->len += len;
}

struct config {
    // Screen dimensions
    int rows;
    int cols;

    // Cursor position
    int cx;
    int cy;

    // Original termios
    struct termios orig_termios;
}; 

struct config config;

// Editor modes
enum modes {
    NORMAL,
    INSERT,
    COMMAND
};

// ASCII key
#define KEY_ESC 0x1b

enum modes mode;

void restore_tmode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.orig_termios) == -1) {
        die("tcsetattr");
    };
}

void raw_tmode() {
    if (tcgetattr(STDIN_FILENO, &config.orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(restore_tmode);

    struct termios raw = config.orig_termios;

    // ~ISIG disables signals sent due to CTRL-C and CTRL-Z
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    
    // Disables CTRL-S and CTRL-Q (in-band software flow control)
    raw.c_iflag &= ~(IXON | ICRNL);

    raw.c_oflag &= ~(OPOST);

    raw.c_cflag |= CS8;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

int get_windowsize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void printkey(char c) {
    if (iscntrl(c)) {
        printf("%d\r\n", c);
    } else {
        printf("%d ('%c')\r\n", c, c);
    }
}

char read_keypress() {
    int n;
    char c;
    while ((n = read(STDIN_FILENO, &c, 1)) != 1) {
        if (n == -1 && errno != EAGAIN) {
            die("read");
        }
    }
    return c;
}

void process_keypress() {
    char c = read_keypress();
    printkey(c);

    switch (c) {
        case 'q':
            if (mode == NORMAL) {
                // EID - Erase In Display, all display
                write(STDOUT_FILENO, "\x1b[2J", 4);
                // CUP - Cursor Position
                write(STDOUT_FILENO, "\x1b[H", 3);
                exit(0);
            }
            break;
        case 'i':
            if (mode == NORMAL) {
                mode = INSERT;
            }
            break;
        case 'h':
            if (mode == NORMAL) {
                if (config.cx > 0) {
                    config.cx--;
                }
            }
            break;
        case 'j':
            if (mode == NORMAL) {
                if (config.cy < config.rows - 1) {
                    config.cy++;
                }
            }
            break;
        case 'k':
            if (mode == NORMAL) {
                if (config.cy > 0) {
                    config.cy--;
                }
            }
            break;
        case 'l':
            if (mode == NORMAL) {
                if (config.cx < config.cols - 1) {
                    config.cx++;
                }
            }
            break;
        case KEY_ESC:
            if (mode == INSERT) {
                mode = NORMAL;
            }
            break;
    }
}

void draw_rows(struct abuf *ab) {
    for (int y = 0; y < config.rows; y++) {
        abuf_append(ab, "~", 1);

        // EL - Erase In Line 
        abuf_append(ab, "\x1b[K", 3);

        if (y < config.rows - 1) {
            abuf_append(ab, "\r\n", 2);
        }
    }
}

void draw_screen() {
    struct abuf ab;
    abuf_init(&ab);

    // Hide the cursor before redrawing
    abuf_append(&ab, "\x1b[?25l", 6);

    draw_rows(&ab);

    // Draw cursor using CUP (Cursor Position) - terminal uses 1 based indexes
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", config.cy + 1, config.cx + 1);
    abuf_append(&ab, buf, strlen(buf));

    // Unhide the cursor
    abuf_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    abuf_free(&ab);
}

int main(int argc, const char* argv[]) {
    raw_tmode();

    mode = NORMAL;

    if (get_windowsize(&config.rows, &config.cols) == -1) {
        die("get_windowsize");
    }
    config.cx = 0;
    config.cy = 0;

    while (1) {
        draw_screen();
        process_keypress();
    }

    return 0;
}

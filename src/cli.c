/*
 * Copyright 2014-2017 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * References:
 *
 * VT-100 terminal (not currently supported)
 *     - http://vt100.net/docs/vt100-ug/
 *     - http://www.termsys.demon.co.uk/vtansi.htm
 *     - http://misc.flogisoft.com/bash/tip_colors_and_formatting
 *     - https://developer.mbed.org/cookbook/VT100-Terminal
 */

#include "embc/cli.h"
#include "embc/dbc.h"
#include "embc/platform.h"


const char LINE_TOO_LONG[] = "Maximum command line length reached";


/* http://www.asciitable.com/ */
#define KEY_NUL  0
#define KEY_BS   8
#define KEY_DEL  127
#define KEY_TAB  9
#define KEY_LF  10
#define KEY_CR  13
#define KEY_ESC 27


static inline void cli_print(cli_t * self, char const * str) {
    if (self->print) {
        self->print(self->print_cookie, str);
    }
}

static inline void print_prompt(cli_t * self) {
    cli_print(self, self->prompt);
}

static inline void print_char(cli_t * self, char ch) {
    char s[2];
    s[0] = ch;
    s[1] = 0;
    cli_print(self, s);
}

static inline void print_newline(cli_t * self) {
    print_char(self, KEY_LF);
}

static void cli_backspace(cli_t * self) {
    if (self->cmdlen <= 0) {
        return;
    }
    if (self->cmdlen >= CLI_LINE_LENGTH) {
        self->cmdlen--;
    } else if (self->cmdlen > 0) {
        self->cmdline[self->cmdlen - 1] = '\0';
        self->cmdlen--;
    }
    if (self->echo_mode != CLI_ECHO_OFF) {
        cli_print(self, "\b \b");
    }
}

static void cli_process_char(cli_t * self, char ch) {
    if (self->cmdlen >= CLI_LINE_LENGTH) {
        self->cmdlen++;
    } else {
        self->cmdline[self->cmdlen] = ch;
        self->cmdlen++;
        self->cmdline[self->cmdlen] = '\0';
    }
    switch (self->echo_mode) {
        case CLI_ECHO_OFF: break;
        case CLI_ECHO_ON: print_char(self, ch); break;
        case CLI_ECHO_USER_CHAR: print_char(self, self->echo_user_char); break;
        default: break;
    }
}

static bool isWhiteSpace(char ch) {
    return ((ch == ' ') || (ch == KEY_TAB));
}

static bool isCommentStart(char const * s) {
    if (NULL == s) {
        return false;
    }
    if ((*s == '#') || (*s == '@') || (*s == '%')) {
        return true;
    }
    return false;
}

static void cli_compact(cli_t * self) {
    embc_size_t i = 0;
    int offset = 0;
    bool isWhite = true;
    for (i = 0; i < self->cmdlen; ++i) {
        if (i >= CLI_LINE_LENGTH) {
            // line too long - do not compact.
            break;
        }
        char ch = self->cmdline[i];
        if (isCommentStart(&ch)) { /* start of a comment */
            break; /* ignore the rest of the line. */
        } else if (isWhiteSpace(ch)) {
            if (!isWhite) {
                self->cmdline[offset] = ' ';
                ++offset;
            }
            isWhite = true;
        } else {
            self->cmdline[offset] = ch;
            ++offset;
            isWhite = false;
        }
    }
    if (offset && isWhiteSpace(self->cmdline[offset - 1])) {
        self->cmdline[offset - 1] = '\0';
        --offset;
    }
    self->cmdlen = offset;
    self->cmdline[self->cmdlen] = '\0';
}

static void cli_process_line(cli_t * self) {
    print_newline(self);
    cli_compact(self);
    if (self->cmdlen == 0) {
        // empty line or only comment
    } else if (self->cmdlen >= CLI_LINE_LENGTH) {
        cli_print(self, LINE_TOO_LONG);
        print_newline(self);
    } else { // valid command
        int rc = CLI_SUCCESS;
        if (self->execute_line) {
            rc = self->execute_line(self->execute_cookie, self->cmdline);
        }
        if (rc != CLI_SUCCESS_PROMPT_ONLY) {
            if (self->verbose == CLI_VERBOSE_FULL) {
                cli_print(self, self->cmdline);
                print_newline(self);
            }
            cli_print(self, (CLI_SUCCESS == rc) ? self->response_success : self->response_error);
        }
    }
    self->cmdline[0] = '\0';
    self->cmdlen = 0;
    print_prompt(self);
}

void cli_initialize(cli_t * self) {
    DBC_NOT_NULL(self);
    embc_memset(self->cmdline, 0, sizeof(self->cmdline));
    self->cmdlen = 0;
    if (self->execute_args) {
        self->execute_line = cli_line_parser;
        self->execute_cookie = self;
    }
    print_prompt(self);
}

void cli_set_echo(cli_t * self, enum cli_echo_mode_e mode, char ch) {
    DBC_NOT_NULL(self);
    self->echo_mode = mode;
    self->echo_user_char = ch;
}

void cli_set_verbose(cli_t * self, enum cli_verbose_mode_e mode) {
    DBC_NOT_NULL(self);
    self->verbose = mode;
}

void cli_insert_char(cli_t * self, char ch) {
    DBC_NOT_NULL(self);
    switch (ch) {
        case KEY_BS:   cli_backspace(self); break;
        case KEY_DEL:  cli_backspace(self); break;
        case KEY_LF:
            if (self->last_char != KEY_CR) {
                cli_process_line(self);
            }
            break;
        case KEY_CR:   cli_process_line(self); break;
        default:       cli_process_char(self, ch); break;
    }
    self->last_char = ch;
}

static bool _is_delimiter(char ch, const char * delim) {
    while (*delim) {
        if (ch == *delim++) {
            return true;
        }
    }
    return false;
}

int cli_line_parser(void * self, const char * cmdline) {
    DBC_NOT_NULL(self);
    cli_t const * p = (cli_t const *) self;
    const char delimiters[] = " \t,";
    char line[CLI_LINE_LENGTH + 2];
    int argc = 0;
    char * argv[CLI_MAX_ARGS];
    char * lineptr = line;
    embc_memset(&argv, 0, sizeof(argv));
    embc_memcpy(line, cmdline, sizeof(line));
    line[sizeof(line) - 1] = 0; // just to be sure.
    while (1) {
        // consume multiple delimiters
        while (*lineptr && _is_delimiter(*lineptr, delimiters)) {
            ++lineptr;
        }
        if (*lineptr == 0) {
            break;
        }
        if (argc >= CLI_MAX_ARGS) {
            return CLI_ERROR_PARAMETER_VALUE;
        }
        argv[argc++] = lineptr; // start of token
        // consume token
        while (*lineptr && !_is_delimiter(*lineptr, delimiters)) {
            ++lineptr;
        }
        if (*lineptr == 0) {
            break;
        }
        *lineptr++ = 0; // force token break
    }

    if (argc == 0) { // blank line!
        return CLI_SUCCESS;
    } else if (p->execute_args) {
        return p->execute_args(p->execute_cookie, argc, argv);
    } else { // not really valid...
        return CLI_ERROR_PARAMETER_VALUE;
    }
}

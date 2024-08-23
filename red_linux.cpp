#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "common.h"
#include "platform.h"
#include "common.cpp"
#include "editor.cpp"

static bool mem_equal(void *a, void *b, size_t sz)
{
    uint8_t *buffer_a = (uint8_t *)a;
    uint8_t *buffer_b = (uint8_t *)b;

    while (sz--) {
        if (*(buffer_a++) != *(buffer_b++)) return false;
    }

    return true;
}

static Handle fs_open(const char *path)
{
    int fd = open(path, O_RDWR);
    Handle handle = {0};
    handle.data = fd;
    return handle;
}

static void fs_close(Handle file)
{
    close(file.data);
}

static bool fs_valid(Handle file)
{
    return (int)file.data >= 0;
}

static size_t fs_size(Handle file)
{
    struct stat st;
    fstat(file.data, &st);
    return st.st_size;
}

static void *fs_map(Handle file, size_t sz, size_t offset)
{
    return mmap(0, sz, PROT_READ|PROT_WRITE, MAP_PRIVATE, file.data, offset);
}

static void fs_unmap(void *buf, size_t sz)
{
    munmap(buf, sz);
}

#define MEM_CAPACITY (640*1024)

static uint8_t mem_buffer[MEM_CAPACITY] = {0};
static size_t mem_used = 0;
static size_t mem_used_max = 0;

static void *mem_alloc_zeroed(size_t sz)
{
    ASSERT(mem_used + sz <= MEM_CAPACITY);
    uint8_t *buffer = &mem_buffer[mem_used];
    mem_used += sz;

    if (mem_used > mem_used_max) {
        mem_used_max = mem_used;
    } else if (mem_used < mem_used_max) {
        memset(buffer, 0, mem_used_max - mem_used);
    }

    return buffer;
}

static void *mem_realloc_zeroed(void *ptr, size_t sz, size_t newsz)
{
    if (!ptr) return mem_alloc_zeroed(newsz);
    if (newsz <= sz) return ptr;

    uint8_t *buffer = (uint8_t *)ptr;
    if (buffer == &mem_buffer[mem_used - sz]) {
        mem_alloc_zeroed(newsz - sz);
        return buffer;
    }

    return mem_alloc_zeroed(newsz);
}

static Frame front = {0};
static Frame back = {0};

static constexpr Platform pf_const_init(void)
{
    Platform pf = {0};

    pf.fs.open = fs_open;
    pf.fs.close = fs_close;
    pf.fs.valid = fs_valid;
    pf.fs.size = fs_size;
    pf.fs.map = fs_map;
    pf.fs.unmap = fs_unmap;

    pf.mem.alloc_zeroed = mem_alloc_zeroed;
    pf.mem.realloc_zeroed = mem_realloc_zeroed;

    pf.frame = &back;

    return pf;
}

static Platform pf = pf_const_init();

static const char *fg_escapes[] = {
    "\x1b[39m", // COLOR_DEFAULT
    "\x1b[30m", // COLOR_BLACK
    "\x1b[31m", // COLOR_RED
    "\x1b[33m", // COLOR_YELLOW
    "\x1b[34m", // COLOR_BLUE
    "\x1b[93m", // COLOR_LIGHT_YELLOW
    "\x1b[94m", // COLOR_LIGHT_BLUE
};
static_assert(LEN(fg_escapes) == COLOR_COUNT, "Please add color to 'fg_escapes'");

static const char *bg_escapes[] = {
    "\x1b[49m", // COLOR_DEFAULT
    "\x1b[40m", // COLOR_BLACK
    "\x1b[41m", // COLOR_RED
    "\x1b[43m", // COLOR_YELLOW
    "\x1b[44m", // COLOR_BLUE
    "\x1b[103m", // COLOR_LIGHT_YELLOW
    "\x1b[104m", // COLOR_LIGHT_BLUE
};
static_assert(LEN(bg_escapes) == COLOR_COUNT, "Please add color to 'bg_escapes'");

#define ESC_CLEAR "\x1b[2J"
#define ESC_HOME "\x1b[H"
#define ESC_RESET "\x1b[0m"
#define ESC_ALT_ON "\x1b[?1049h"
#define ESC_ALT_OFF "\x1b[?1049l"
#define ESC_CURSOR_ON "\x1b[25h"
#define ESC_CURSOR_OFF "\x1b[25l"

static bool check_if_term(void)
{
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "error: stdin is not a terminal\n");
        return false;
    }

    if (!isatty(STDOUT_FILENO)) {
        fprintf(stderr, "error: stdout is not a terminal\n");
        return false;
    }

    return true;
}

static size_t frame_buf_size(Frame *frame)
{
    return frame->width * frame->height * sizeof(*frame->glyphs);
}

static void frame_resize(Frame *frame, int width, int height)
{
    size_t old_size = frame_buf_size(frame);
    frame->width = width;
    frame->height = height;
    size_t new_size = frame_buf_size(frame);

    frame->glyphs = (Text_Glyph *)mem_realloc_zeroed(frame->glyphs, old_size, new_size);
}

static void frame_resize_to_fit_term(Frame *frame)
{
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size)) UNREACHABLE;
    frame_resize(frame, size.ws_col, size.ws_row);
}

static void frame_resize_all_to_fit_term(void)
{
    printf(ESC_CLEAR "" ESC_HOME);
    frame_resize_to_fit_term(&back);
    frame_resize_to_fit_term(&front);
}

static void set_term_cursor(int x, int y)
{
    printf("\x1b[%d;%dH", y+1, x+1);
}

static void display(void)
{
    printf(ESC_HOME);
    ASSERT(front.width == back.width);
    ASSERT(front.height == back.height);

    Color fg = COLOR_DEFAULT;
    Color bg = COLOR_DEFAULT;

    printf(ESC_RESET);

    for (int row = 0; row < back.height; ++row) {
        for (int col = 0; col < back.width; ++col) {
            size_t idx = row*back.width+col;
            Text_Glyph *g = &back.glyphs[idx];

            bool same = mem_equal(g, &front.glyphs[idx], sizeof(Text_Glyph));
            if (!same) {
                set_term_cursor(col, row);

                uint8_t buf[4];
                size_t len = utf32_to_8(g->codepoint, buf, sizeof(buf));

                if (g->fg != fg) {
                    fg = g->fg;
                    printf(fg_escapes[fg]);
                }

                if (g->bg != bg) {
                    bg = g->bg;
                    printf(bg_escapes[bg]);
                }

                for (size_t i = 0; i < len; ++i) fputc(buf[i], stdout);
            }
        }
    }

    set_term_cursor(back.width-1, back.height-1);
    fflush(stdout);
    memcpy(front.glyphs, back.glyphs, frame_buf_size(&back));
}

static Event event_null;
static Event *push_event(void)
{
    if (pf.events.count >= LEN(pf.events.data)) {
        return &event_null;
    }

    Event *event = &pf.events.data[pf.events.count++];
    memset(event, 0, sizeof(*event));
    return event;
}

static void sigwinch_handler(int sig)
{
    frame_resize_all_to_fit_term();
}

static bool term_setup(struct termios *termios_old)
{
    struct termios termios;
    if (tcgetattr(STDIN_FILENO, termios_old) != 0)
        return false;

    termios = *termios_old;
    termios.c_lflag &= ~ICANON;
    termios.c_lflag &= ~ECHO;

    memset(termios.c_cc, 0, sizeof(termios.c_cc));
    termios.c_cc[VMIN] = 1; // block for keypresses

    if (tcsetattr(0, 0, &termios) != 0)
        return false;

    return true;
}

static bool signal_setup(struct sigaction *oldact)
{
    struct sigaction act = {0};
    act.sa_handler = sigwinch_handler;

    if (sigaction(SIGWINCH, &act, oldact) != 0)
        return false;
    return true;
}

int main(void)
{
    editor_init(&pf);

    bool term_is_initialized = false;
    bool sig_is_initialized = false;

    if (!check_if_term()) return 1;

    struct termios termios_old;
    if (!term_setup(&termios_old)) goto reset_term_state;
    term_is_initialized = true;

    struct sigaction oldact;
    if (!signal_setup(&oldact)) goto reset_term_state;
    sig_is_initialized = true;

    printf(ESC_ALT_ON);
    printf(ESC_CURSOR_OFF);

    frame_resize_all_to_fit_term();

    while (!pf.should_close) {
        editor_frame();
        display();
        pf.events.count = 0;
        
        uint8_t input[16];
        errno = 0;
        int bytes = read(STDIN_FILENO, input, sizeof(input));

        if (errno != 0) continue;
        if (bytes <= 0) continue;

        if (input[0] == 'q') pf.should_close = true;

        // for (size_t i = 0; i < bytes; ++i) printf("0x%02x ", input[i]);
        // printf("\n");

        if (input[0] == 0x1b) {
            if (input[1] == 0x5b) {
                Key key = KEY_NONE;
                
                switch (input[2]) {
                case 0x41:
                    key = KEY_UP;
                    break;
                case 0x42:
                    key = KEY_DOWN;
                    break;
                case 0x43:
                    key = KEY_RIGHT;
                    break;
                case 0x44:
                    key = KEY_LEFT;
                    break;
                default:
                    break;
                }

                if (key) {
                    Event *event = push_event();
                    event->type = EVENT_KEY;
                    event->key = key;
                }
            }
        } else {
            size_t used = 0;
            while (used < bytes) {
                uint32_t cp = utf8_to_32(input, bytes-used, &used);
                Event *event = push_event();
                event->type = EVENT_CHAR;
                event->cp = cp;   
            }
        }
    }

reset_term_state:
    printf(ESC_ALT_OFF);
    printf(ESC_CURSOR_ON);

    if (sig_is_initialized)
        sigaction(SIGWINCH, &oldact, 0);
    if (term_is_initialized)
        tcsetattr(0, 0, &termios_old);

    return 0;
}

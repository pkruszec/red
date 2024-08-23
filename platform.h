#ifndef PLATFORM_H
#define PLATFORM_H

enum Color: uint8_t {
    COLOR_DEFAULT = 0,
    COLOR_BLACK,
    COLOR_RED,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_LIGHT_YELLOW,
    COLOR_LIGHT_BLUE,

    COLOR_COUNT,
};

struct Handle {
    size_t data;
};

struct Text_Glyph {
    uint32_t codepoint;
    Color fg;
    Color bg;
};

struct Frame {
    Text_Glyph *glyphs;
    int width;
    int height;
};

enum Event_Type: uint8_t {
    EVENT_NONE = 0,
    EVENT_CHAR,
    EVENT_KEY,
};

enum Key: uint32_t {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_RIGHT,
    KEY_LEFT,
};

struct Event {
    Event_Type type;
    union {
        uint32_t cp;
        uint32_t key;
    };
};

struct Platform {
    bool should_close;
    Frame *frame;
    struct {
        Event data[4];
        size_t count;
    } events;
    struct {
        void *(*alloc_zeroed)(size_t sz);
        void *(*realloc_zeroed)(void *ptr, size_t sz, size_t newsz);
    } mem;
    struct {
        Handle (*open)(const char *path);
        void (*close)(Handle file);
        bool (*valid)(Handle file);
        size_t (*size)(Handle file);
        void *(*map)(Handle file, size_t sz, size_t offset);
        void (*unmap)(void *buf, size_t sz);
    } fs;
};

#endif // PLATFORM_H

#include "common.h"
#include "platform.h"

template<typename T>
static void array_ensure(Platform *p, T *array, size_t count)
{
    if (array->count + count > array->capacity) {
        size_t old_capacity = array->capacity;
        array->capacity += MAX(array->capacity, count);
        void *buffer = p->mem.realloc_zeroed(array->data,
                                             old_capacity * sizeof(*array->data),
                                             array->capacity * sizeof(*array->data));
        array->data = (decltype(array->data))buffer;
    }
}

template<typename T>
static auto array_append_many(Platform *p, T *array, size_t count)
{
    array_ensure(p, array, count);
    auto ptr = &array->data[array->count];
    array->count += count;
    return ptr;
}

template<typename T>
static auto array_append(Platform *p, T *array)
{
    return array_append_many(p, array, 1);
}

struct Cursor {
    uint64_t line;
    uint64_t col;
};

struct Region {
    uint32_t *data;
    size_t count;
    size_t capacity;
};

struct Editor {
    Region region;
    Cursor primary;
    Cursor mark;
};

static void ed_load_file_region(Platform *p, Region *region, Handle file, size_t size, size_t offset)
{
    uint8_t *src = (uint8_t *)p->fs.map(file, size, offset);
    size_t pos = 0;
    
    array_ensure(p, region, size);

    bool prev_cr = 0;
    while (pos < size) {
        size_t used = 0;
        uint32_t cp = utf8_to_32(&src[pos], size-pos, &used);
        pos += used;

        if (cp == '\r') {
            prev_cr = true;
            continue;
        }
        
        if (prev_cr) {
            *array_append(p, region) = '\n';
            prev_cr = false;
            
            if (cp == '\n') {
                continue;
            }
        }
        
        uint32_t *ptr = array_append(p, region);
        *ptr = cp;
    }
    
    p->fs.unmap(src, size);
}

static void ed_pf_clear(Platform *p)
{
    int h = p->frame->height;
    int w = p->frame->width;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Text_Glyph *g = &p->frame->glyphs[y*w + x];
            g->codepoint = ' ';
            g->bg = COLOR_DEFAULT;
        }
    }
}

static void ed_pf_put(Platform *p, int x, int y, uint32_t cp, Color fg, Color bg)
{
    ASSERT((x < p->frame->width) && (y < p->frame->height));
    Text_Glyph *glyph = &p->frame->glyphs[y * p->frame->width + x];
    glyph->codepoint = cp;
    glyph->fg = fg;
    glyph->bg = bg;
}

static Event *get_event(Platform *p)
{
    if (p->events.count <= 0) return 0;
    return &p->events.data[--p->events.count];
}

static Editor *e;
static Platform *p;

void editor_init(Platform *platform)
{
    p = platform;
    e = (Editor *)p->mem.alloc_zeroed(sizeof(*e));

    Handle file = p->fs.open("./editor.cpp");
    if (!p->fs.valid(file)) return;

    ed_load_file_region(p, &e->region, file, p->fs.size(file), 0);
    e->primary.line = 3;
    e->primary.col = 6;
    
    p->fs.close(file);
}

void editor_frame(void)
{
    while (true) {
        Event *event = get_event(p);
        if (!event) break;

        switch (event->type) {
        case EVENT_CHAR:
            
            break;
        default:
            break;
        }
    }
    
    ed_pf_clear(p);

    int x_begin = 0;
    int y_begin = 0;
    
    int x = 0;
    int y = 0;
    for (size_t i = 0; i < e->region.count; ++i) {
        if (y >= p->frame->height) break;

        if (x < p->frame->width) {
            uint32_t cp = e->region.data[i];
            if (cp == '\n') {
                y++;
                x = 0;
            } else {
                bool at_primary = x == e->primary.col && y == e->primary.line;
                bool at_mark = x == e->mark.col && y == e->mark.line;
                
                Color fg = at_primary ? COLOR_BLACK : COLOR_DEFAULT;
                Color bg = at_primary ? COLOR_LIGHT_YELLOW : (at_mark ? COLOR_LIGHT_BLUE : COLOR_DEFAULT);
                ed_pf_put(p, x+x_begin, y+y_begin, cp, fg, bg);
                x++;
            }
        }
    }
}

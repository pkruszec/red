#ifndef COMMON_H
#define COMMON_H

#define PANIC (*(int *)0 = 0)
#define UNREACHABLE ASSERT(0 && "unreachable")

#if DEBUG
#define ASSERT(condition) do { if (!(condition)) PANIC; } while (0)
#else
#define ASSERT(condition)
#endif

#define LEN(arr) (sizeof(arr)/sizeof(arr[0]))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static size_t utf32_to_8(uint32_t cp, uint8_t *buf, size_t sz);
static uint32_t utf8_to_32(uint8_t *buf, size_t sz, size_t *used);

#endif // COMMON_H

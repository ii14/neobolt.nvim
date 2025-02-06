#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__)
# include <time.h>
#endif

#if defined(__clang__) || defined(__GNUC__)
# pragma GCC diagnostic push
# if defined(__clang__)
#  pragma GCC diagnostic ignored "-Wshorten-64-to-32"
# elif defined(__GNUC__)
#  pragma GCC diagnostic ignored "-Wconversion"
# endif
#endif
#include "data_directives.h"
#if defined(__clang__) || defined(__GNUC__)
# pragma GCC diagnostic pop
#endif


#define STRINGIFY(x) STRINGIFY_(x)
#define STRINGIFY_(x) #x
#define CONCAT(a, b) CONCAT_(a, b)
#define CONCAT_(a, b) a##b

#if defined(__GNUC__) || defined(__clang__)
# define INTERFACE static __attribute__((unused))
# define EXPORT __attribute__((visibility("default")))
# define INLINE __attribute__((always_inline)) inline
# define NOINLINE __attribute__((noinline))
# define NORETURN __attribute__((noreturn))
# define LIKELY(x) (__builtin_expect(!!(x), 1))
# define UNLIKELY(x) (__builtin_expect(!!(x), 0))
# define UADD(a, b, r) __builtin_add_overflow((a), (b), (r))
#elif defined(_MSC_VER)
# define INTERFACE static
# define EXPORT __declspec(dllexport)
# define INLINE __forceinline
# define NOINLINE __declspec(noinline)
# define NORETURN __declspec(noreturn)
# define LIKELY(x) (!!(x))
# define UNLIKELY(x) (!!(x))
# define UADD(a, b, r) (*(r) = (a) + (b), *(r) < (a))
#else
# define INTERFACE static
# define EXPORT
# define INLINE inline
# define NOINLINE
# define NORETURN
# define LIKELY(x) (!!(x))
# define UNLIKELY(x) (!!(x))
# define UADD(a, b, r) (*(r) = (a) + (b), *(r) < (a))
#endif

#if defined(__clang__)
# define BREAKPOINT() __builtin_debugtrap()
#elif defined(__GNUC__) && defined(__x86_64__)
# define BREAKPOINT() __asm__ volatile("int3")
#endif


typedef unsigned char byte;
typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;
typedef size_t usize;
typedef ptrdiff_t isize;

#define cast(T, ...) ((T)(__VA_ARGS__))

#define FREE(p) do { \
  free(p); \
  p = NULL; \
} while (0)


#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static u32 nextpow2(
    u32 n)
{
  --n;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  return ++n;
}

/// Current timestamp, in microseconds
static inline u64 get_time(void)
{
#if defined(__unix__)
  struct timespec ts = {0};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return cast(u64, ts.tv_nsec) / 1000 + cast(u64, ts.tv_sec) * 1000000;
#else
  return 0;
#endif
}


typedef struct {
  const byte* ptr;
  usize len;
} String;

/// Small string that references byte range in some other relocatable array.
typedef struct {
  u32 off; ///< Byte offset
  u32 len; ///< Byte length
} StrRef;

/// Dereference StrRef to String
#define STR(base, strref) \
  (String){ .ptr = &(base)[(strref).off], .len = (strref).len }

/// Compare two String-s
#define STREQ(a, b) \
  ((a).len == (b).len && ((a).len == 0 || memcmp((a).ptr, (b).ptr, (a).len) == 0))

/// Compare String and a string literal
#define STRTEST(str, lit) \
  ((str).len == (sizeof(lit) - 1) && memcmp((str).ptr, lit, (sizeof(lit) - 1)) == 0)

#define EOL '\n'

INLINE static bool is_digit(byte ch) { return ch >= '0' && ch <= '9'; }
INLINE static bool is_lower(byte ch) { return ch >= 'a' && ch <= 'z'; }
INLINE static bool is_upper(byte ch) { return ch >= 'A' && ch <= 'Z'; }
INLINE static bool is_alpha(byte ch) { return is_lower(ch) || is_upper(ch); }
INLINE static bool is_alnum(byte ch) { return is_digit(ch) || is_alpha(ch); }
INLINE static bool is_space(byte ch) { return ch == ' ' || ch == '\t'; }
INLINE static bool is_symbol(byte ch) { return is_alnum(ch) || ch == '_' || ch == '.' || ch == '$'; }
INLINE static bool is_symbol1(byte ch) { return is_alpha(ch) || ch == '_' || ch == '.'; }

static u32 fnv1a(
    String str)
{
  u32 hash = 0x811C9DC5;
  for (usize i = 0; i < str.len; ++i)
    hash = (hash ^ cast(u32, str.ptr[i])) * 0x01000193;
  return hash;
}


enum LineType {
  kLineUnknown = 0,
  kLineLabel,
  kLineLocalLabel,
  kLineData, ///< Data directive
  kLineDirective, ///< Other directives
  kLineInstruction,
  kLineComment,

  kLineTypeCount,
};

#define LINE_FLAG_SHOW 0x1
#define LINE_FLAG_LABEL_VISITED 0x2

// TODO: there is some stuff there that swaps between 0-based and 1-based indexing.
// would be nice if it could be simplified, either by using -1 for nulls or by putting
// an extra dummy element in arrays at index 0 so it's always 1-based. extra element
// sounds better imo

typedef struct {
  StrRef line; ///< Full line
  StrRef name; ///< Main name (instruction name, label name, directive name)
  u8 type; ///< Line type (LineType)
  u8 flags;
  u32 loc; ///< 1-based location index
} Line;

/// max line count. 250,000,000 lines is ought to be enough for anyone
#define LINE_LIMIT 0x10000000

typedef struct {
  Line* data;
  u32 size; ///< `data` element count
  u32 cap; ///< `data` allocation size. Always a power of two
} Lines;

// line array sometimes requires more memory (~1.5x?) than textual representation.
// there is room for improvement here. ideas:
//
// 1. remove line size - it can be deduced from the next line. a dummy element with just
//    the offset can be added at the end, so you never have to branch.
// 2. 32-bit byte offset for the line has to stay.
// 3. extra data can be moved out of line, to per-type tables. pointer to it can be
//    encoded in a single 32-bit integer, 3 bottom bits for type, and 29 bits for pointer.
//
//   typedef struct {
//     u32 line_offset; // byte offset into input.ptr
//     u32 type_pointer; // 3-bit type, 29-bit index into a table determined by the type
//   } Line;
//
// sums up to 8 bytes per line, down from 24


typedef struct {
  u32 hash;
  u32 line; ///< 1-based line number. Zero means the slot is empty.
} LabelHashSlot;

/// Open addressing hash table
typedef struct {
  LabelHashSlot* data;
  u32 size; ///< `data` element count
  u32 cap; ///< `data` allocation size. Always a power of two.
} LabelHash;

typedef struct {
  u32* data;
  u32 head;
  u32 tail;
  u32 cap; ///< `data` allocation size. Always a power of two.
} LabelQueue;


typedef struct {
  byte* data;
  u32 top; ///< Pointer to free memory in `data`
  u32 cap; ///< `data` allocation size
} Arena;


typedef struct {
  u32* ids; ///< Only needed during parsing to connect .loc to .file. Then we move to indices
  StrRef* paths; ///< Strings here point into the arena
  u32 size; ///< `ids` and `data` element count
  u32 cap; ///< `ids` and `data allocation size
} Files;

typedef struct {
  u32 file; ///< 1-based index into files.paths
  u32 line;
  u32 col;
} Location;

typedef struct {
  Location current; ///< Current location
  u32 current_id; ///< Current file ID

  Location* data;
  u32 size; ///< `data` element count
  u32 cap; ///< `data` allocation size
} Locations;

// instead of parsing .file and .loc directives eagerly, instructions could point at the
// associated .loc line (needs a file number to line number mapping), and .loc point at
// associated .file line. not sure if that gives me anything atm though.


typedef struct {
  const char* msg;
  const char* loc;
  jmp_buf jmpbuf;
} Exception;


typedef struct State {
  String input;
  Lines lines;
  LabelHash label_hash;
  LabelQueue label_queue;
  Files files;
  Locations loc;
  Arena arena;
  Exception exception;

#if defined(NEOBOLT_STATS)
  u64 time_pass1;
  u64 time_pass2;
  u64 time_pass3;
  u64 hash_lookups;
  u64 hash_misses;
#endif
} State;


#ifndef NEOBOLT_LINES_INITIAL_CAP
// on gcc hello world in C is 291 lines, 28 labels
// C++ with iostream is 6211 lines, 395 labels
# define NEOBOLT_LINES_INITIAL_CAP 2048
#endif
#ifndef NEOBOLT_LABEL_HASH_INITIAL_CAP
# define NEOBOLT_LABEL_HASH_INITIAL_CAP 1024
#endif
#ifndef NEOBOLT_LABEL_QUEUE_INITIAL_CAP
# define NEOBOLT_LABEL_QUEUE_INITIAL_CAP 64
#endif
#ifndef NEOBOLT_FILES_INITIAL_CAP
# define NEOBOLT_FILES_INITIAL_CAP 64
#endif
#ifndef NEOBOLT_LOCATIONS_INITIAL_CAP
# define NEOBOLT_LOCATIONS_INITIAL_CAP 256
#endif


INTERFACE bool neobolt_init(
    State* const restrict s,
    const byte* data,
    usize size);
INTERFACE bool neobolt_parse(
    State* const restrict s);
INTERFACE void neobolt_destroy(
    State* const restrict s);


INTERFACE bool neobolt_init(
    State* const restrict s,
    const byte* data,
    usize size)
{
  if (size >= cast(usize, UINT32_MAX) || data == NULL || size == 0)
    return false;
  *s = (State) {
    .input = { .ptr = data, .len = cast(u32, size) },
    .loc = { .current_id = cast(u32, -1) },
  };
  return true;
}

INTERFACE void neobolt_destroy(
    State* const restrict s)
{
  FREE(s->lines.data);
  FREE(s->label_hash.data);
  FREE(s->label_queue.data);
  FREE(s->files.ids);
  FREE(s->files.paths);
  FREE(s->loc.data);
  FREE(s->arena.data);
}

NORETURN NOINLINE static void fail(
    State* const restrict s,
    const char* msg,
    const char* loc)
{
  s->exception.msg = msg;
  s->exception.loc = loc;
  longjmp(s->exception.jmpbuf, 1);
}

#define FATAL(msg) fail(s, msg, __FILE__ ":" STRINGIFY(__LINE__))
#define CHECK(cond) (LIKELY(cond) ? cast(void, 0) : FATAL("assertion failed: " #cond))


static Line* line_push(
    State* const restrict s)
{
  Lines* const self = &s->lines;

  if UNLIKELY (self->size == self->cap) {
    CHECK(self->cap < LINE_LIMIT); // hard line count cap
    u32 ncap = self->cap == 0 ? NEOBOLT_LINES_INITIAL_CAP : self->cap << 1;
    // CHECK(ncap != 0); // overflow not possible with the hard cap
    void* ndata = realloc(self->data, cast(usize, ncap) * sizeof(*self->data));
    CHECK(ndata != NULL);
    self->data = ndata;
    self->cap = ncap;
  }

  return &self->data[self->size++];
}


/// Returns label slot. If the slot is not occupied, it will have `line` set to zero.
static LabelHashSlot* label_hash_search(
    State* const restrict s,
    String name,
    u32 hash)
{
  LabelHash* const self = &s->label_hash;

#if defined(NEOBOLT_STATS)
  s->hash_lookups += 1;
#endif

  const u32 mask = self->cap - 1;
  u32 inc = 1;
  u32 idx = hash & mask;
  for (;;) {
    if (self->data[idx].line == 0)
      break;

    if (self->data[idx].hash == hash) {
      u32 pline = self->data[idx].line;
      StrRef pname = s->lines.data[pline - 1].name;
      if (STREQ(name, STR(s->input.ptr, pname)))
        break;
    }

    idx = (idx + inc++) & mask;

#if defined(NEOBOLT_STATS)
    s->hash_misses += 1;
#endif
  }

  return &self->data[idx];
}

/// Add label to the hash map. Line is 1-based.
static void label_hash_set(
    State* const restrict s,
    String name,
    u32 line)
{
  LabelHash* const self = &s->label_hash;

  CHECK(line != 0); // line numbers are 1-based here

  if UNLIKELY (self->size * 100 >= self->cap * 65) { // % max load factor
    u32 ncap = self->cap << 1;
    CHECK(ncap != 0); // overflow
    LabelHashSlot* nlabels = calloc(ncap, sizeof(*nlabels));
    CHECK(nlabels != NULL);

    // rehash the table
    const u32 mask = ncap - 1;
    for (u32 i = 0; i < self->cap; ++i) {
      if (self->data[i].line == 0)
        continue;

      u32 inc = 1;
      u32 idx = self->data[i].hash & mask;
      while (nlabels[idx].line != 0)
        idx = (idx + inc++) & mask;

      nlabels[idx] = self->data[i];
    }

    free(self->data);
    self->data = nlabels;
    self->cap = ncap;
  }

  const u32 hash = fnv1a(name);
  LabelHashSlot* slot = label_hash_search(s, name, hash);
  if (slot->line != 0)
    return; // already in the set
  slot->hash = hash;
  slot->line = line;
  self->size += 1;
}

/// Returns 1-based line number, zero if label was not found
static u32 label_hash_get(
    State* const restrict s,
    String name)
{
  const u32 hash = fnv1a(name);
  LabelHashSlot* slot = label_hash_search(s, name, hash);
  return slot->line;
}


static void label_queue_grow(
    State* const restrict s)
{
  LabelQueue* const self = &s->label_queue;

  u32 ncap = self->cap << 1;
  CHECK(ncap != 0); // overflow
  u32* ndata = malloc(cast(usize, ncap) * sizeof(*self->data));
  CHECK(ndata != NULL);

  u32 mask = self->cap - 1;
  u32 size = 0;
  if (self->head != self->tail) {
    u32 head = self->head & mask;
    u32 tail = self->tail & mask;
    if (head < tail) {
      size = tail - head;
      memcpy(ndata, self->data + head, cast(usize, size) * sizeof(*self->data));
    } else {
      size = self->cap - head;
      if (size != 0)
        memcpy(ndata, self->data + head, cast(usize, size) * sizeof(*self->data));
      if (tail != 0)
        memcpy(ndata + size, self->data, cast(usize, tail) * sizeof(*self->data));
      size += tail;
    }
  }

  free(self->data);
  self->data = ndata;
  self->cap = ncap;
  self->tail = size;
  self->head = 0;
}

static void label_queue_push(
    State* const restrict s,
    u32 value)
{
  LabelQueue* const self = &s->label_queue;
  if UNLIKELY (self->tail == self->head + self->cap)
    label_queue_grow(s);
  self->data[self->tail++ & (self->cap - 1)] = value;
}

static bool label_queue_pop(
    State* const restrict s,
    u32* res)
{
  LabelQueue* const self = &s->label_queue;
  if (self->tail == self->head)
    return false;
  *res = self->data[self->head++ & (self->cap - 1)];
  return true;
}


static u32 arena_alloc(
    State* const restrict s,
    u32 size)
{
  Arena* const self = &s->arena;

  u32 base = self->top;
  u32 ntop;
  CHECK(!UADD(base, size, &ntop)); // overflow

  if UNLIKELY (ntop > self->cap) {
    u32 ncap = nextpow2(ntop);
    CHECK(ncap != 0); // overflow
    byte* ndata = realloc(self->data, ncap);
    CHECK(ndata != NULL);
    self->data = ndata;
    self->cap = ncap;
  }

  self->top = ntop;
  return base;
}


static void file_add(
    State* const restrict s,
    u32 id,
    StrRef path)
{
  Files* const self = &s->files;

  if UNLIKELY (self->size == self->cap) {
    u32 ncap = self->cap == 0 ? NEOBOLT_FILES_INITIAL_CAP : self->cap << 1;
    CHECK(ncap != 0); // overflow

    void* nids = realloc(self->ids, cast(usize, ncap) * sizeof(*self->ids));
    CHECK(nids != NULL);
    self->ids = nids;

    void* npaths = realloc(self->paths, cast(usize, ncap) * sizeof(*self->paths));
    CHECK(npaths != NULL);
    self->paths = npaths;

    self->cap = ncap;
  }

  u32 file_idx = self->size++;
  self->ids[file_idx] = id;
  self->paths[file_idx] = path;
}

/// Return 1-based file index, or zero when ID was not found
static u32 file_search(
    State* const restrict s,
    u32 id)
{
  Files* const self = &s->files;

  // just linear search for now
  for (u32 i = 0; i < self->size; ++i)
    if (self->ids[i] == id)
      return i + 1;
  return 0;
}


/// Push current location and return 1-based location index
static u32 loc_push(
    State* const restrict s)
{
  Locations* const self = &s->loc;

  Location* curr = &self->current;
  if (curr->file == 0)
    return 0;

  if (self->size != 0) {
    Location* last = &self->data[self->size - 1];
    if (curr->file == last->file && curr->line == last->line && curr->col == last->col)
      return self->size;
  }

  if UNLIKELY (self->size == self->cap) {
    u32 ncap = self->cap == 0 ? NEOBOLT_LOCATIONS_INITIAL_CAP : self->cap << 1;
    CHECK(ncap != 0);
    void* ndata = realloc(self->data, cast(usize, ncap) * sizeof(*self->data));
    CHECK(ndata != NULL);
    self->data = ndata;
    self->cap = ncap;
  }

  self->data[self->size++] = *curr;
  return self->size;
}


static bool is_data_directive(
    const byte* data,
    usize len)
{
  return is_data_directive_lookup(cast(const char*, data), len) != NULL;
}

/// First pass.
/// Populates lines and labels hash map.
static void pass_1(
    State* const restrict s)
{
  const byte* const text = s->input.ptr;
  const u32 size = cast(u32, s->input.len);

  // TODO: "/* */" comments?

  // ~75% of time is spent in this function.
  // this loop can be multithreaded actually, but only if "/* */" comments are not
  // handled. unless those are handled in a separate pass. that should be fast though.

  for (u32 pos = 0; pos < size; ++pos) {
    u32 line_off = pos;

    // skip leading indentation, usually a single hard tab
    while (is_space(text[pos]))
      if UNLIKELY (++pos >= size)
        return; // last line, whitespace only - bail out

    StrRef name = { .off = pos, .len = 0 };
    enum LineType type = kLineUnknown;
    u8 flags = 0;

    if (is_symbol(text[pos])) {
      // TODO: spends some time in this loop, might be worth vectorizing?
      while (++pos < size && is_symbol(text[pos])) {}
      name.len = pos - name.off;

      if (pos < size && text[pos] == ':') {
        // local labels require a different lookup strategy.
        // not handled for now, they don't seem to be generated for normal code.
        bool is_local = !is_symbol1(text[name.off]);
        type = is_local ? kLineLocalLabel : kLineLabel;
        pos += 1; // skip ':'
        // i suppose you can technically have an instruction after the
        // label, on the same line. i don't think compilers do that though.
      } else if (pos >= size || text[pos] == EOL || is_space(text[pos])) {
        if (name.len > 1 && text[name.off] == '.') {
          name.off += 1; // remove '.' from name
          name.len -= 1;
          // TODO: would it be better if is_data_directive was checked lazily?
          bool is_data = is_data_directive(&text[name.off], (usize)name.len);
          type = is_data ? kLineData : kLineDirective;
        } else {
          type = kLineInstruction;
          flags = LINE_FLAG_SHOW; // always show instructions
        }
      }
    } else if (text[pos] == '#') {
      type = kLineComment;
      pos += 1;
      // TODO: handle #APP/#NO_APP, that wraps inline asm. always show every line inside it
    } else if (text[pos] == '/') {
      if (pos + 1 < size && text[pos + 1] == '/') {
        type = kLineComment;
        pos += 2;
      }
    }

    // skip until eol
    const byte* nl = memchr(text + pos, EOL, size - pos);
    if UNLIKELY (nl == NULL) // reject last line, if it's not terminated with a newline.
      return;                // simplifies parsing, bound checks are now not necessary.
    pos = cast(u32, nl - text);

    Line* line = line_push(s);
    line->line = (StrRef){ .off = line_off, .len = pos - line_off };
    line->name = name;
    line->type = cast(u8, type);
    line->flags = flags;
    line->loc = 0;

    // gather all labels into a hash map
    if (type == kLineLabel) {
      label_hash_set(s, STR(text, name), s->lines.size); // 1-based line index
    }
  }
}


static void check_potential_label(
    State* const restrict s,
    String name)
{
  u32 label = label_hash_get(s, name);
  if (label == 0)
    return;

  label -= 1; // back off to 0-based indexing
  Line* line = &s->lines.data[label];

  // TODO: lables could be removed from the hash map here, maybe switch to robin hood.
  line->flags |= LINE_FLAG_SHOW; // show the label
  // TODO: actually this could just check if LINE_FLAG_SHOW wasn't set yet
  if (!(line->flags & LINE_FLAG_LABEL_VISITED)) {
    line->flags |= LINE_FLAG_LABEL_VISITED;
    label_queue_push(s, label);
  }
}


static inline const byte* line_args_ptr(
    State* const restrict s,
    const Line* line)
{
  return s->input.ptr + line->name.off + line->name.len;
}

static inline bool parse_byte(
    const byte** restrict ptr,
    byte v)
{
  if (*(*ptr) != v)
    return false;
  (*ptr) += 1;
  return true;
}

static inline bool parse_spaces(
    const byte** const restrict ptr)
{
  if (!is_space(*(*ptr)))
    return false;
  do {
    (*ptr) += 1;
  } while (is_space(*(*ptr)));
  return true;
}

static inline bool parse_u32(
    const byte** const restrict ptr,
    u32* restrict res)
{
  if (!is_digit(*(*ptr)))
    return false;
  u64 value = cast(u64, *(*ptr) - '0');
  while (is_digit(*(++(*ptr)))) {
    // ignore overflow. it doesn't really matter
    value *= 10;
    value += cast(u64, *(*ptr) - '0');
  }

  if (res != NULL)
    *res = cast(u32, value);
  return true;
}

static inline bool parse_string(
    const byte** const restrict ptr,
    String* restrict res)
{
  if (*(*ptr) != '"')
    return false;
  String value;
  value.ptr = ++(*ptr);
  for (;;) {
    if (*(*ptr) == '"') {
      value.len = cast(usize, (*ptr) - value.ptr);
      (*ptr) += 1;
      if (res != NULL)
        *res = value;
      return true;
    }

    // TODO: should i bother with interpreting escaped chars?
    if (*(*ptr) == '\\')
      (*ptr) += 1;

    if (*(*ptr) == EOL)
      return false;
    (*ptr) += 1;
  }
}

static inline bool parse_symname(
    const byte** const restrict ptr,
    String* restrict res)
{
  if (!is_symbol1(*(*ptr)))
    return false;
  String value;
  value.ptr = (*ptr);
  do {
    (*ptr) += 1;
  } while (is_symbol(*(*ptr)) || *(*ptr) == '@');
  value.len = cast(usize, (*ptr) - value.ptr);
  if (res != NULL)
    *res = value;
  return true;
}


static void directive_loc(
    State* const restrict s,
    const byte* p)
{
  u32 id = 0;
  u32 lnum = 0;
  u32 col = 0;

  if UNLIKELY (!parse_spaces(&p))
    return;
  if UNLIKELY (!parse_u32(&p, &id))
    return;
  if UNLIKELY (!parse_spaces(&p))
    return;
  if UNLIKELY (!parse_u32(&p, &lnum))
    return;
  if (parse_spaces(&p))
    parse_u32(&p, &col);

  if (s->loc.current_id != id) {
    s->loc.current_id = id;
    s->loc.current.file = file_search(s, id);
  }
  s->loc.current.line = lnum;
  s->loc.current.col = col;
}

static void directive_file(
    State* const restrict s,
    const byte* p)
{
  u32 id = 0;
  String f1 = {0};
  String f2 = {0};

  if UNLIKELY (!parse_spaces(&p))
    return;
  if UNLIKELY (!parse_u32(&p, &id))
    return;
  if UNLIKELY (!parse_spaces(&p))
    return;
  if UNLIKELY (!parse_string(&p, &f1))
    return;
  if (parse_spaces(&p))
    parse_string(&p, &f2);

  // paths aren't normalized, they can often go up and down to the
  // same directory. but this can be taken care of on the neovim side.
  StrRef fname = {0};
  if (f2.len == 0) {
    if (f1.len == 0)
      return;
    fname.len = cast(u32, f1.len);
    fname.off = arena_alloc(s, fname.len);
    memcpy(s->arena.data + fname.off, f1.ptr, f1.len);
  } else if (f2.ptr[0] == '/') { // absolute path
    fname.len = cast(u32, f2.len);
    fname.off = arena_alloc(s, fname.len);
    memcpy(s->arena.data + fname.off, f2.ptr, f2.len);
  } else { // relative path
    fname.len = cast(u32, f1.len + f2.len + 1);
    fname.off = arena_alloc(s, fname.len);
    memcpy(s->arena.data + fname.off, f1.ptr, f1.len);
    s->arena.data[fname.off + f1.len] = '/';
    memcpy(s->arena.data + fname.off + f1.len + 1, f2.ptr, f2.len);
  }

  file_add(s, id, fname);
}

static void directive_globl(
    State* const restrict s,
    const byte* p)
{
  String symname;

  if UNLIKELY (!parse_spaces(&p))
    return;
  if UNLIKELY (!parse_symname(&p, &symname))
    return;

  check_potential_label(s, symname);
}

static void directive_type(
    State* const restrict s,
    const byte* p)
{
  // .type <name>,#<type>
  // .type <name>,@<type>
  // .type <name>,%<type>
  // .type <name>,"<type>"
  //      function
  //      gnu_indirect_function
  //      object
  //      tls_object
  //      common
  //      notype
  //      gnu_unique_object
  // .type <name> STT_<TYPE_IN_UPPER_CASE>
  //      STT_FUNC
  //      STT_GNU_IFUNC
  //      STT_OBJECT
  //      STT_TLS
  //      STT_COMMON
  //      STT_NOTYPE

  String symname;
  String type;

  if UNLIKELY (!parse_spaces(&p))
    return;
  if UNLIKELY (!parse_symname(&p, &symname))
    return;
  parse_spaces(&p);
  if UNLIKELY (!parse_byte(&p, ','))
    return;
  parse_spaces(&p);
  if (parse_byte(&p, '"')) {
    if UNLIKELY (!parse_symname(&p, &type))
      return;
    if UNLIKELY (!parse_byte(&p, '"'))
      return;
  } else {
    if UNLIKELY (!parse_byte(&p, '#')
              && !parse_byte(&p, '@')
              && !parse_byte(&p, '%'))
      return;
    if UNLIKELY (!parse_symname(&p, &type))
      return;
  }

  if (STRTEST(type, "function")) {
    check_potential_label(s, symname);
  }
}


static void parse_label_references(
    State* const restrict s,
    const byte* p)
{
  while (*p != EOL) {
    // bail on comments
    if (p[0] == '#')
      return;
    if (p[0] == '/' && p[1] == '/')
      return;

    String symname;
    if (parse_symname(&p, &symname)) {
      check_potential_label(s, symname);
      continue;
    }

    // skip strings
    if (parse_string(&p, NULL))
      continue;
    // parse_string can increment the pointer _and_ return
    // failure because of EOL, so it has to be checked again.
    // TODO: return a more meaningful status from parse_* functions
    if (*p == EOL)
      return;

    ++p;
  }
}

/// Second pass.
/// Parses directives and initial label references.
static void pass_2(
    State* const restrict s)
{
  for (u32 lnum = 0; lnum < s->lines.size; ++lnum) {
    Line* line = &s->lines.data[lnum];

    if (line->type == kLineInstruction) {
      // search for labels referenced in the instruction
      parse_label_references(s, line_args_ptr(s, line));
      // set source location
      line->loc = loc_push(s);
    } else if (line->type == kLineDirective) {
      // https://sourceware.org/binutils/docs/as/Pseudo-Ops.html
      String name = STR(s->input.ptr, line->name); // directive name
      if (STRTEST(name, "loc")) {
        directive_loc(s, line_args_ptr(s, line));
      } else if (STRTEST(name, "file")) {
        directive_file(s, line_args_ptr(s, line));
      } else if (STRTEST(name, "global")
              || STRTEST(name, "globl")
              || STRTEST(name, "weak")) {
        directive_globl(s, line_args_ptr(s, line));
      } else if (STRTEST(name, "type")) {
        directive_type(s, line_args_ptr(s, line));
      } else if (STRTEST(name, "data")
              || STRTEST(name, "text")
              || STRTEST(name, "section")
              || STRTEST(name, "cfi_endproc")) {
        // reset source location
        s->loc.current_id = cast(u32, -1);
        s->loc.current.file = 0;
      }
    }
  }
}

/// Third pass.
/// Parses label references from all queued labels.
static void pass_3(
    State* const restrict s)
{
  // process all queued labels
  u32 lnum;
  while (label_queue_pop(s, &lnum)) {
    for (; lnum < s->lines.size; ++lnum) {
      Line* line = &s->lines.data[lnum];

      if (line->type == kLineInstruction || line->type == kLineDirective)
        break; // end of data label

      if (line->type == kLineData) {
        line->flags |= LINE_FLAG_SHOW; // show the data directive

        // search for more labels in the data directive
        // may push more labels onto a queue
        parse_label_references(s, line_args_ptr(s, line));
      }
    }
  }
}

INTERFACE bool neobolt_parse(
    State* const restrict s)
{
  // catch exceptions
  if (setjmp(s->exception.jmpbuf) != 0)
    return false;

  CHECK(s->label_hash.data == NULL);
  s->label_hash.data = calloc(NEOBOLT_LABEL_HASH_INITIAL_CAP, sizeof(*s->label_hash.data));
  CHECK(s->label_hash.data != NULL);
  s->label_hash.cap = NEOBOLT_LABEL_HASH_INITIAL_CAP;

  CHECK(s->label_queue.data == NULL);
  s->label_queue.data = malloc(NEOBOLT_LABEL_QUEUE_INITIAL_CAP * sizeof(*s->label_queue.data));
  CHECK(s->label_queue.data != NULL);
  s->label_queue.cap = NEOBOLT_LABEL_QUEUE_INITIAL_CAP;

#if !defined(NEOBOLT_STATS)
  pass_1(s);
  pass_2(s);
  pass_3(s);
#else
  u64 ts1, ts2;
  ts1 = get_time();

  pass_1(s);

  ts2 = get_time();
  s->time_pass1 = ts2 - ts1;
  ts1 = ts2;

  pass_2(s);

  ts2 = get_time();
  s->time_pass2 = ts2 - ts1;
  ts1 = ts2;

  pass_3(s);

  ts2 = get_time();
  s->time_pass3 = ts2 - ts1;
#endif

  return true;
}

// vim: sw=2 sts=2 et

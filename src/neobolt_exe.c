// #define NEOBOLT_STATS
#include "neobolt.c"

#include <assert.h>
#include <stdio.h>

static void read_file(
    FILE* file,
    byte** rdata,
    usize* rsize)
{
  usize cap = 4096;
  usize size = 0;
  byte* data = malloc(cap);
  if (data == NULL) {
    perror("malloc");
    exit(1);
  }

  for (;;) {
    if (size + 4096 >= cap) {
      cap <<= 1;
      if (cap > 0x100000000ULL) {
        fprintf(stderr, "input is too big\n");
        exit(1);
      }

      data = realloc(data, cap);
      if (data == NULL) {
        perror("realloc");
        exit(1);
      }
    }

    usize n = fread(data + size, 1, 4096, file);
    if (n == 0) {
      if (ferror(file)) {
        perror("fread");
        exit(1);
      }
      break;
    }

    size += n;
  }

  assert(size < cast(usize, UINT32_MAX));
  *rdata = data;
  *rsize = size;
}

static void print_lines(
    State* const s,
    bool source)
{
  for (u32 i = 0; i < s->lines.size; ++i) {
    Line* line = &s->lines.data[i];

    if (!(line->flags & LINE_FLAG_SHOW))
      continue;

    if (source && line->loc != 0) {
      assert(line->loc <= s->loc.size);
      Location* loc = &s->loc.data[line->loc - 1];
      assert(loc->file != 0);
      assert(loc->file <= s->files.size);
      StrRef fname = s->files.paths[loc->file - 1];

      printf("%.*s:%u:%u: ",
          cast(int, fname.len),
          s->arena.data + fname.off,
          loc->line,
          loc->col);
    }

    String text = STR(s->input.ptr, line->line);
    printf("%.*s\n", cast(int, text.len), text.ptr);
  }
}

static void print_stats(
    State* const s)
{
  usize input = s->input.len;

  usize lines = s->lines.size;
  usize lines_b = s->lines.size * sizeof(*s->lines.data);
  usize lines_r = s->lines.cap * sizeof(*s->lines.data);

  usize label_hash = s->label_hash.size;
  usize label_hash_b = s->label_hash.size * sizeof(*s->label_hash.data);
  usize label_hash_r = s->label_hash.cap * sizeof(*s->label_hash.data);

  usize label_queue = s->label_queue.cap;
  usize label_queue_b = s->label_queue.cap * sizeof(*s->label_queue.data);

  usize files = s->files.size;
  usize files_b = s->files.size * (sizeof(*s->files.ids) + sizeof(*s->files.paths));
  usize files_r = s->files.cap * (sizeof(*s->files.ids) + sizeof(*s->files.paths));

  usize locations = s->loc.size;
  usize locations_b = s->loc.size * sizeof(*s->loc.data);
  usize locations_r = s->loc.cap * sizeof(*s->loc.data);

  usize arena = s->arena.top;
  usize arena_r = s->arena.cap;

  usize mem_used = lines_b + label_hash_b + label_queue_b + files_b + locations_b + arena;
  usize mem_reserved = lines_r + label_hash_r + label_queue_b + files_r + locations_r + arena_r;
  double mem_used_p = cast(double, mem_used) / cast(double, input) * 100.0;
  double mem_reserved_p = cast(double, mem_reserved) / cast(double, input) * 100.0;

  usize line_counts[kLineTypeCount] = {0};
  for (u32 i = 0; i < s->lines.size; ++i) {
    u32 type = s->lines.data[i].type;
    assert(type < kLineTypeCount);
    line_counts[type] += 1;
  }
  double line_counts_p[kLineTypeCount] = {0};
  for (usize i = 0; i < kLineTypeCount; ++i) {
    line_counts_p[i] = cast(double, line_counts[i]) / cast(double, lines) * 100.0;
  }

  fprintf(stderr, "Stats:\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "  Input                  %10zu bytes\n", input);
  fprintf(stderr, "  Lines                  %10zu (%zu bytes)\n", lines, lines_b);
  fprintf(stderr, "  - Instructions         %10zu (%.2f%%)\n", line_counts[kLineInstruction], line_counts_p[kLineInstruction]);
  fprintf(stderr, "  - Labels               %10zu (%.2f%%)\n", line_counts[kLineLabel], line_counts_p[kLineLabel]);
  fprintf(stderr, "  - Local labels         %10zu (%.2f%%)\n", line_counts[kLineLocalLabel], line_counts_p[kLineLocalLabel]);
  fprintf(stderr, "  - Data directives      %10zu (%.2f%%)\n", line_counts[kLineData], line_counts_p[kLineData]);
  fprintf(stderr, "  - Other directives     %10zu (%.2f%%)\n", line_counts[kLineDirective], line_counts_p[kLineDirective]);
  fprintf(stderr, "  - Comments             %10zu (%.2f%%)\n", line_counts[kLineComment], line_counts_p[kLineComment]);
  fprintf(stderr, "  - Unknown              %10zu (%.2f%%)\n", line_counts[kLineUnknown], line_counts_p[kLineUnknown]);
  fprintf(stderr, "  Label hash             %10zu (%zu bytes)\n", label_hash, label_hash_b);
  fprintf(stderr, "  Label queue            %10zu (%zu bytes)\n", label_queue, label_queue_b);
  fprintf(stderr, "  Files                  %10zu (%zu bytes)\n", files, files_b);
  fprintf(stderr, "  Locations              %10zu (%zu bytes)\n", locations, locations_b);
  fprintf(stderr, "  Arena                  %10zu bytes\n", arena);
  fprintf(stderr, "  Used memory            %10zu bytes (%.2f%%)\n", mem_used, mem_used_p);
  fprintf(stderr, "  Reserved memory        %10zu bytes (%.2f%%)\n", mem_reserved, mem_reserved_p);
#if defined(NEOBOLT_STATS)
  fprintf(stderr, "\n");
  fprintf(stderr, "  Hash lookups           %10zu\n", s->hash_lookups);
  fprintf(stderr, "  Hash misses            %10zu\n", s->hash_misses);
  fprintf(stderr, "  Pass 1          %10zu.%06zu seconds\n",
      cast(usize, s->time_pass1 / 1000000),
      cast(usize, s->time_pass1 % 1000000));
  fprintf(stderr, "  Pass 2          %10zu.%06zu seconds\n",
      cast(usize, s->time_pass2 / 1000000),
      cast(usize, s->time_pass2 % 1000000));
  fprintf(stderr, "  Pass 3          %10zu.%06zu seconds\n",
      cast(usize, s->time_pass3 / 1000000),
      cast(usize, s->time_pass3 % 1000000));
#endif

  // for (usize i = 0; i < s->label_hash.cap; ++i) {
  //   LabelHashSlot* slot = &s->label_hash.data[i];
  //   if (slot->line == 0)
  //     continue;
  //   u32 line = s->label_hash.data[i].line;
  //   StrRef name = s->lines.data[line - 1].name;
  //   printf("%.*s\n", cast(int, name.len), s->input.ptr + name.off);
  // }
}

static void print_help(
    const char* progname)
{
  fprintf(stderr, "usage: %s [options] [input]\n", progname);
  fprintf(stderr, "\n");
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -l  print source locations\n");
  fprintf(stderr, "  -q  hide asm output\n");
  fprintf(stderr, "  -s  print statistics\n");
}

int main(
    int argc,
    char** argv)
{
  bool show_stats = false;
  bool show_loc = false;
  bool quiet_asm = false;
  const char* file_path = NULL;

  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    if (arg[0] == '-') {
      if (arg[1] == '\0')
        goto invalid_option;
      for (const char* p = arg + 1; *p != '\0'; ++p) {
        if (*p == 'h') {
          print_help(argv[0]);
          return 0;
        } else if (*p == 's') {
          show_stats = true;
        } else if (*p == 'l') {
          show_loc = true;
        } else if (*p == 'q') {
          quiet_asm = true;
        } else {
          goto invalid_option;
        }
      }
    } else {
      if (file_path != NULL) {
        fprintf(stderr, "invalid argument: %s\n", arg);
        print_help(argv[0]);
        return 1;
      }
      file_path = arg;
    }

    continue;

invalid_option:
    fprintf(stderr, "invalid argument: %s\n", arg);
    print_help(argv[0]);
    return 1;
  }

  byte* data;
  usize size;
  if (file_path == NULL) {
    read_file(stdin, &data, &size);
  } else {
    FILE* file = fopen(file_path, "rb");
    if (file == NULL) {
      perror("fopen");
      return 1;
    }

    read_file(file, &data, &size);

    fclose(file);
  }

  State state;
  bool ok = neobolt_init(&state, data, size);
  assert(ok);

  u64 time = get_time();
  if (!neobolt_parse(&state)) {
    fprintf(stderr, "Fatal error: %s\n", state.exception.msg);
    fprintf(stderr, "  in %s\n", state.exception.loc);
    goto cleanup;
  }
  time = get_time() - time;

  if (!quiet_asm)
    print_lines(&state, show_loc);

  if (show_stats) {
    print_stats(&state);
    fprintf(stderr, "\n");
    fprintf(stderr, "Took %zu.%06zu seconds\n",
        cast(usize, time / 1000000),
        cast(usize, time % 1000000));
  }

cleanup:
  neobolt_destroy(&state);
  free(data);
}

// vim: sw=2 sts=2 et

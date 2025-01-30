#include "neobolt.c"

#include <lua.h>
#include <lauxlib.h>

static int lneobolt_parse(
    lua_State* L)
{
  usize size;
  // TODO: accept array of strings too
  const byte* data = cast(const byte*, luaL_checklstring(L, 1, &size));

  State state;

  if (!neobolt_init(&state, data, size)) {
    lua_pushnil(L);
    lua_pushstring(L, "libneobolt: invalid input");
    return 2;
  }

  if (!neobolt_parse(&state)) {
    lua_pushnil(L);
    lua_pushfstring(L, "libneobolt: %s (%s)", state.exception.msg, state.exception.loc);
    return 2;
  }

  lua_createtable(L, 0, 5);

  int line_count = 0;

  // repurpose file ids array to mark what files actually get referenced
  for (u32 i = 0; i < state.files.size; ++i)
    state.files.ids[i] = 0;

  {
    lua_createtable(L, 1024, 0); // TODO: more accurate narr parameter?
    int idx = 1;
    for (u32 i = 0; i < state.lines.size; ++i) {
      Line* line = &state.lines.data[i];
      if (!(line->flags & LINE_FLAG_SHOW))
        continue;

      // quick hack, repurpose offset to store new line numbers
      line->name.off = cast(u32, idx);

      String text = STR(data, line->line);
      lua_pushlstring(L, cast(const char*, text.ptr), text.len);
      lua_rawseti(L, -2, idx++);

      ++line_count;
    }
    lua_setfield(L, -2, "lines");
  }

  {
    lua_createtable(L, line_count, 0);
    for (u32 i = 0; i < state.lines.size; ++i) {
      Line* line = &state.lines.data[i];
      if (line->loc == 0)
        continue;

      // quick hack, repurpose offset to store new line numbers
      int lnum = cast(int, line->name.off);
      lua_pushinteger(L, cast(lua_Integer, line->loc));
      lua_rawseti(L, -2, lnum);
    }
    lua_setfield(L, -2, "location_map");
  }

  {
    lua_createtable(L, line_count, 0); // TODO: more accurate narr parameter?
    int idx = 1;
    for (u32 i = 0; i < state.lines.size; ++i) {
      Line* line = &state.lines.data[i];
      u32 loc = line->loc;
      if (loc == 0)
        continue;

      // quick hack, repurpose offset to store new line numbers
      int lnum_first = cast(int, line->name.off);
      int lnum_last = lnum_first;
      while (i + 1 < state.lines.size) {
        line = &state.lines.data[i + 1];

        if (line->type != kLineInstruction) {
          ++i;
          continue;
        }

        if (line->loc != loc)
          break;

        lnum_last = cast(int, line->name.off);
        ++i;
      }

      lua_createtable(L, 2, 0);
      lua_pushinteger(L, lnum_first);
      lua_rawseti(L, -2, 1);
      lua_pushinteger(L, lnum_last);
      lua_rawseti(L, -2, 2);
      // location index is always increasing by 1.
      // we know it implicitly, don't need to store it
      // lua_pushinteger(L, cast(lua_Integer, loc));
      // lua_rawseti(L, -2, 3);
      lua_rawseti(L, -2, idx++);
    }
    lua_setfield(L, -2, "location_ranges");
  }

  {
    lua_createtable(L, cast(int, state.loc.size), 0);
    for (u32 i = 0; i < state.loc.size; ++i) {
      Location* loc = &state.loc.data[i];

      if (loc->file != 0)
        state.files.ids[loc->file - 1] = 1; // mark file as used

      lua_createtable(L, 3, 0);
      lua_pushinteger(L, cast(lua_Integer, loc->file));
      lua_rawseti(L, -2, 1);
      lua_pushinteger(L, cast(lua_Integer, loc->line));
      lua_rawseti(L, -2, 2);
      lua_pushinteger(L, cast(lua_Integer, loc->col));
      lua_rawseti(L, -2, 3);
      lua_rawseti(L, -2, cast(int, i + 1));
    }
    lua_setfield(L, -2, "locations");
  }

  {
    lua_createtable(L, cast(int, state.files.size), 0);
    const byte* arena = state.arena.data;
    for (u32 i = 0; i < state.files.size; ++i) {
      if (state.files.ids[i] == 0) // skip unused files
        continue;
      String text = STR(arena, state.files.paths[i]);
      lua_pushlstring(L, cast(const char*, text.ptr), text.len);
      lua_rawseti(L, -2, cast(int, i + 1));
    }
    lua_setfield(L, -2, "files");
  }

  neobolt_destroy(&state);
  return 1;
}

EXPORT int luaopen_libneobolt(
    lua_State* L)
{
  lua_createtable(L, 0, 1);

  lua_pushcfunction(L, lneobolt_parse);
  lua_setfield(L, -2, "parse");

  return 1;
}

// vim: sw=2 sts=2 et

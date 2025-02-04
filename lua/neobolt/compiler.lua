local api = vim.api
local fn = vim.fn
local uv = vim.loop

local spawn = require('neobolt.spawn')
local lib_ok, lib = pcall(require, 'libneobolt')
-- TODO: embed version in the library and check if lua scripts are compatible,
--       or should the library be recompiled
if not lib_ok then
  -- TODO: compile the library automatically?
  api.nvim_echo({
    {'neobolt.nvim: libneobolt module is missing! Run `make` in the plugin directory.', 'ErrorMsg'},
  }, true, {})
  error(lib)
end


-- TODO: try to make the time adaptive. maybe the first change should start compiling
--       instantly, and only if more changes happen during that, it can start debouncing
local DEBOUNCE = 100


local t_insert = table.insert
local t_concat = table.concat

local w_get_cursor = api.nvim_win_get_cursor
local w_set_cursor = api.nvim_win_set_cursor

local b_get = api.nvim_get_current_buf
local b_valid = api.nvim_buf_is_valid
local b_changedtick = api.nvim_buf_get_changedtick
local b_get_lines = api.nvim_buf_get_lines
local b_set_lines = api.nvim_buf_set_lines
local b_get_opt = api.nvim_buf_get_option
local b_set_opt = api.nvim_buf_set_option
local b_set_mark = api.nvim_buf_set_extmark
local b_get_mark = api.nvim_buf_get_extmark_by_id
local b_get_marks = api.nvim_buf_get_extmarks
local function b_changenr(bufnr)
  if bufnr == nil or bufnr == 0 then
    return fn.changenr()
  else
    local changenr
    api.nvim_buf_call(bufnr, function()
      changenr = fn.changenr()
    end)
    return changenr
  end
end


-- for random stuff
local NS = api.nvim_create_namespace('neobolt')
-- only for extmarks with associated source locations
local NS_LOC = api.nvim_create_namespace('neobolt_loc')


local g_id = 0
local g_asm_map = {}
local g_src_map = {}


local function normalize_bufnr(bufnr)
  if bufnr == nil or bufnr == 0 then
    return b_get()
  elseif type(bufnr) == 'number' and b_valid(bufnr) then
    return bufnr
  else
    error('invalid buffer number: ' .. tostring(bufnr))
  end
end

local function find_buf_win(buf)
  local wins = api.nvim_tabpage_list_wins(0)
  for _, win in ipairs(wins) do
    if api.nvim_win_get_buf(win) == buf then
      return win
    end
  end
  return nil
end

local function new_update_state()
  return {
    -- command line
    exe = '',
    base_args = {},
    user_args = {},
    cwd = '',

    -- source buffer changedtick
    changedtick = nil,
    -- source buffer changenr
    changenr = nil,

    -- maps extmark ids onto {file, line, column} tuples
    mark_to_loc = {},
    -- nested map of: file -> line -> extmark[]
    loc_to_mark = {},
    -- currently highlighted marks in asm buffer
    asm_hls = {},
  }
end


local Compiler = {}
local CompilerMT = { __index = Compiler }

function Compiler.new(asm_buf, src_buf, opts)
  assert(b_valid(asm_buf))
  assert(b_valid(src_buf))

  assert(type(opts) == 'table')
  assert(type(opts.exe) == 'string')
  assert(type(opts.base_args) == 'table')
  assert(type(opts.user_args) == 'table')

  opts = opts or {}
  local cwd = opts.cwd or uv.cwd()
  local exe = opts.exe
  local base_args = vim.deepcopy(opts.base_args)
  local user_args = vim.deepcopy(opts.user_args)

  return setmetatable({
    id = nil,
    -- asm buffer
    asm_buf = asm_buf,
    -- source buffer
    src_buf = src_buf,

    -- working directory
    cwd = cwd,
    -- compiler executable
    exe = exe,
    -- base arguments
    base_args = base_args,
    -- user provided arguments
    user_args = user_args,

    changed = true,
    in_insert = false,
    user_paused = false, -- TODO: unused, expose it
    debounce = uv.new_timer(),

    -- running compiler process
    proc = nil,
    -- recreated on every update
    update_state = new_update_state(),
    -- array of used autocmd IDs
    autocmds = {},

    _destroyed = false,
  }, CompilerMT)
end


function Compiler:init()
  -- _G.bb = self -- for debugging only

  g_id = g_id + 1
  self.id = g_id
  b_set_opt(self.asm_buf, 'buftype', 'nofile')
  b_set_opt(self.asm_buf, 'swapfile', false)
  b_set_opt(self.asm_buf, 'undofile', false)
  -- TODO: unlikely, but nvim_buf_set_name can fail if name is already taken
  api.nvim_buf_set_name(self.asm_buf, ('neobolt://%d/%d'):format(self.src_buf, self.id))
  b_set_opt(self.asm_buf, 'filetype', 'asm')


  local AUTOCMD_DESC = ('neobolt %d'):format(self.asm_buf)
  local function autocmd(events, buffer, callback)
    t_insert(self.autocmds, api.nvim_create_autocmd(events, {
      desc = AUTOCMD_DESC,
      buffer = buffer,
      callback = callback,
    }))
  end

  autocmd('BufDelete', self.asm_buf, function() self:destroy() end)
  autocmd('BufDelete', self.src_buf, function() self:destroy() end)

  -- autocmd('CursorMoved', self.src_buf, function()
  --   if not self:destroyed() then
  --     if b_changedtick(self.src_buf) == self.update_state.changedtick then
  --       self:highlight_asm(w_get_cursor(0)[1])
  --     end
  --   end
  -- end)

  -- autocmd('BufLeave', self.src_buf, function()
  --   if not self:destroyed() then
  --     self:highlight_asm(nil)
  --   end
  -- end)

  autocmd({'TextChanged', 'TextChangedI', 'TextChangedP'}, self.src_buf, function()
    if not self:destroyed() then
      self.changed = true
      self:highlight_asm(nil) -- TODO: call update_hls?
      self:schedule_update()
    end
  end)

  autocmd('InsertEnter', self.src_buf, function()
    if not self:destroyed() then
      self.in_insert = true
    end
  end)

  autocmd('InsertLeave', self.src_buf, function()
    if not self:destroyed() then
      self.in_insert = false
      self:schedule_update()
    end
  end)


  api.nvim_buf_set_keymap(self.asm_buf, 'n', '<CR>', '', {
    noremap = true,
    callback = function()
      self:go_to_src(w_get_cursor(0)[1])
    end,
  })

  api.nvim_buf_set_keymap(self.asm_buf, 'n', 'gO', '', {
    noremap = true,
    callback = function()
      local curr = t_concat(self.user_args, ' ')
      local ok, opts = pcall(fn.input, {
        prompt = 'options: ',
        default = curr,
        cancelreturn = curr,
      })
      if ok and curr ~= opts then
        -- TODO: handle escaped whitespace, quotes
        self.user_args = vim.split(opts, '%s+', {trimempty = true})
        self:update()
      end
    end,
  })

  -- register in the asm map
  g_asm_map[self.asm_buf] = self
  -- register in the src map
  if g_src_map[self.src_buf] == nil then
    g_src_map[self.src_buf] = {}
  end
  g_src_map[self.src_buf][self.asm_buf] = self
end


function Compiler:destroy()
  if self._destroyed then return end
  self._destroyed = true

  -- unregister from the asm map
  if g_asm_map[self.asm_buf] == self then
    g_asm_map[self.asm_buf] = nil
  end

  -- unregister from the src map
  if g_src_map[self.src_buf] then
    if g_src_map[self.src_buf][self.asm_buf] == self then
      g_src_map[self.src_buf][self.asm_buf] = nil
      if next(g_src_map[self.src_buf]) == nil then
        g_src_map[self.src_buf] = nil
      end
    end
  end

  -- close debounce timer
  if self.debounce then
    self.debounce:stop()
    self.debounce:close()
    self.debounce = nil
  end

  -- kill running process
  if self.proc then
    self.proc:abort()
    self.proc = nil
  end

  -- remove all autocmds
  for i = 1, #self.autocmds do
    pcall(api.nvim_del_autocmd, self.autocmds[i])
    self.autocmds[i] = nil
  end
end

function Compiler:destroyed()
  if self._destroyed then
    return true
  elseif not b_valid(self.asm_buf) or not b_valid(self.src_buf) then
    -- i wish this was simpler, and i could just listen on BufDelete to know when buffers
    -- get deleted, so i can clean stuff up properly. but autocmds aren't reliable because
    -- they can be silenced, and we might find out about that well after the fact, which
    -- is here. potential alternative is nvim_buf_attach, but iirc it kinda breaks when
    -- you reload the file with :e.
    self:destroy()
    return true
  else
    return false
  end
end


function Compiler:update()
  -- kill currently running process
  if self.proc then
    self.proc:abort()
    self.proc = nil
  end
  if self:destroyed() then
    return
  end

  self.changed = false

  -- TODO: expose the status to the user, so they can have a nice statusline or whatever

  local state = new_update_state()

  state.changedtick = b_changedtick(self.src_buf)
  state.changenr = b_changenr(self.src_buf)

  local src = b_get_lines(self.src_buf, 0, -1, false)
  t_insert(src, '\n')
  src = t_concat(src, '\n')

  state.cwd = self.cwd
  state.exe = fn.exepath(self.exe)
  assert(state.exe and state.exe ~= '', 'invalid executable') -- TODO: error handling
  state.base_args = vim.deepcopy(self.base_args)
  state.user_args = vim.deepcopy(self.user_args) -- TODO: copy might not be necessary?

  local args = {}
  for i = 1, #self.base_args do
    t_insert(args, self.base_args[i])
  end
  for i = 1, #self.user_args do
    t_insert(args, self.user_args[i])
  end

  -- TODO: limit the number of max parallel jobs. eg when you have multiple compilers
  --       attached to a single source buffer
  -- TODO: handle errors
  -- TODO: timeout
  self.proc = spawn(state.exe, args, state.cwd, src, function(proc)
    if self.proc == proc then
      self.proc = nil
    end
    self:render(proc, state)
  end)
end

function Compiler:schedule_update()
  -- TODO: suppress also when asm_buf is not visible in the current tab
  if not self.changed or self.in_insert or self.user_paused then
    return
  end
  self.changed = false

  self.debounce:start(DEBOUNCE, 0, function()
    vim.schedule(function()
      self:update()
    end)
  end)
end

function Compiler:render(proc, state)
  if self:destroyed() then
    return
  end

  -- TODO: option to disable filtering
  local parse_time = uv.hrtime()
  -- TODO: should this run on a separate thread?
  -- c and c++ translation units should be generally small. but zig's hello world is
  -- already ~4 MB, and right now it parses in about ~20-15ms, so ~200-260 MiB/s.
  -- this is still acceptable, but if it takes any longer then it becomes a problem.
  -- not sure how big input should i reasonably expect. i could optimize it and halve
  -- the running time or more probably, but maybe making this async is just the safest
  -- option.
  local asm, asm_err = lib.parse(proc.stdout)
  parse_time = (uv.hrtime() - parse_time) / 1e+9


  -- discard previous state
  self.update_state = state

  if asm then
    -- normalize paths
    for i, path in ipairs(asm.files) do
      -- TODO: there is also "<built-in>", check wtf is that
      if path ~= '<stdin>' then
        asm.files[i] = fn.fnamemodify(path, ':p')
      end
    end

    -- replace file IDs with paths
    for _, loc in ipairs(asm.locations) do
      loc[1] = asm.files[loc[1]]
    end
  end


  local stderr = vim.split(proc.stderr, '\n', { plain = true })
  -- trim trailing empty lines
  for i = #stderr, 1, -1 do
    if stderr[i] == '' then
      stderr[i] = nil
    else
      break
    end
  end
  for i = 1, #stderr do
    stderr[i] = '# ' .. stderr[i]
  end


  local summary = {
    ('#      cwd: %s'):format(state.cwd),
    ('# compiler: %s'):format(state.exe),
    ('#    flags: %s'):format(t_concat(state.user_args, ' ')),
    '',
    ('# exited with code %d, signal %d'):format(proc.code, proc.signal),
    ('# compiler %.6fs, process %.6fs'):format(proc.time, parse_time),
  }


  -- extmark IDs overflow after UINT32_MAX, and if i'm reading it right it's
  -- per buffer+namespace. if we clear them every time, it's not a problem.
  api.nvim_buf_clear_namespace(self.asm_buf, NS, 0, -1)
  api.nvim_buf_clear_namespace(self.asm_buf, NS_LOC, 0, -1)

  -- reset undo history, so the user can still edit
  -- the buffer, but can't over-undo to previous states
  local undolevels = b_get_opt(self.asm_buf, 'undolevels')
  b_set_opt(self.asm_buf, 'undolevels', -1)


  -- replacing all lines will reset cursor position,
  -- so overwrite existing lines instead.
  local line_count = api.nvim_buf_line_count(self.asm_buf)
  local lnum_curr, lnum_last = 0, 0
  local function append_lines(lines)
    local last = lnum_curr + #lines
    if last >= line_count then
      last = -1
    end
    b_set_lines(self.asm_buf, lnum_curr, last, false, lines)
    lnum_last = lnum_curr
    lnum_curr = lnum_curr + #lines
  end

  append_lines(summary)
  -- b_set_mark(self.asm_buf, NS, lnum_last, 0, {
  --   end_row = lnum_curr,
  --   hl_group = 'Normal', -- TODO: clears bg likely
  --   hl_eol = true,
  -- })

  if #stderr > 0 then
    append_lines({''})
    append_lines(stderr)
    b_set_mark(self.asm_buf, NS, lnum_last, 0, {
      end_row = lnum_curr,
      hl_group = 'Error',
      hl_eol = true,
    })
    -- TODO: hitting enter on a warning should go to that line in the source
  end

  if not asm then
    append_lines({'', '# ' .. asm_err})
    b_set_mark(self.asm_buf, NS, lnum_last, 0, {
      end_row = lnum_curr,
      hl_group = 'Error',
      hl_eol = true,
    })
  end

  if asm and #asm.lines > 0 then
    append_lines({''})
    append_lines(asm.lines)
    -- store source locations as extmarks
    for i, range in ipairs(asm.location_ranges) do
      local first, last = range[1] + lnum_last, range[2] + lnum_last
      local mark = b_set_mark(self.asm_buf, NS_LOC, first - 1, 0, { end_row = last })
      state.mark_to_loc[mark] = asm.locations[i]
    end
  end

  -- trim remaining lines
  if line_count >= lnum_curr then
    b_set_lines(self.asm_buf, lnum_curr, -1, false, {})
  end


  -- restore undolevels
  b_set_opt(self.asm_buf, 'undolevels', undolevels)

  -- populate file map
  for mark, loc in pairs(state.mark_to_loc) do
    local file, line = loc[1], loc[2]
    if not state.loc_to_mark[file] then
      state.loc_to_mark[file] = {}
    end
    if not state.loc_to_mark[file][line] then
      state.loc_to_mark[file][line] = {}
    end
    t_insert(state.loc_to_mark[file][line], mark)
  end

  -- update highlighting from the source buffer
  if b_get() == self.src_buf then
    self:highlight_asm(w_get_cursor(0)[1])
  end
end

function Compiler:in_sync()
  return b_valid(self.asm_buf) and
         b_valid(self.src_buf) and
         b_changedtick(self.src_buf) == self.update_state.changedtick
end


local function update_mark(buf, ns, id, opts)
  local mark = b_get_mark(buf, ns, id, { details = true })
  assert(#mark >= 3) -- { row, col, details }
  opts.id = id
  opts.end_row = mark[3].end_row
  opts.end_col = mark[3].end_col
  b_set_mark(buf, ns, mark[1], mark[2], opts)
end

local function get_mark_on_line(buf, ns, lnum, details)
  local mark = b_get_marks(buf, ns, {lnum, 0}, -1, {
    limit = 1,
    overlap = true,
    details = details,
  })[1]

  if mark then
    assert(#mark >= 3) -- { id, row, col, details? }
    if lnum > mark[2] then
      return mark
    end
  end

  return nil
end

function Compiler:get_src_line(lnum)
  local mark = get_mark_on_line(self.asm_buf, NS_LOC, lnum)
  if not mark then return end
  local loc = assert(self.update_state.mark_to_loc[mark[1]])
  local file, line, col = loc[1], loc[2], loc[3]
  if file == '<stdin>' then
    return line, col
  end
end

function Compiler:go_to_src(lnum)
  local line, col = self:get_src_line(lnum)
  if not line then return end
  local win = find_buf_win(self.src_buf)
  if not win then return end
  -- TODO: temporarily set scrolljump=-50 to center the cursor?
  local ok = pcall(w_set_cursor, win, {line, col - 1})
  if ok then
    api.nvim_set_current_win(win)
  end
end

function Compiler:highlight_asm(lnum)
  if not b_valid(self.asm_buf) then
    return
  end

  -- clear previous highlights
  for i, mark in ipairs(self.update_state.asm_hls) do
    update_mark(self.asm_buf, NS_LOC, mark, {})
    self.update_state.asm_hls[i] = nil
  end

  -- clear only
  if lnum == nil then
    return
  end

  local file = self.update_state.loc_to_mark['<stdin>']
  if not file then return end
  local marks = file[lnum]
  if not marks then return end
  for _, mark in ipairs(marks) do
    update_mark(self.asm_buf, NS_LOC, mark, {
      hl_group = 'Visual',
      hl_eol = true,
    })
    t_insert(self.update_state.asm_hls, mark)
  end
end


if true then
  local g_src_hl = nil
  local g_asm_hls = {}

  local function update_hls(bufnr)
    bufnr = normalize_bufnr(bufnr)
    local lnum = w_get_cursor(0)[1]

    do -- update asm highlights
      -- clear previous highlights
      for i = 1, #g_asm_hls do
        g_asm_hls[i]:highlight_asm(nil)
        g_asm_hls[i] = nil
      end

      local asm_bufs = g_src_map[bufnr]
      if asm_bufs ~= nil then
        for _, compiler in pairs(asm_bufs) do
          if compiler:in_sync() then
            compiler:highlight_asm(lnum)
            t_insert(g_asm_hls, compiler)
          end
        end
      end
    end

    do -- update source highlights
      -- clear previous highlights
      if g_src_hl ~= nil then
        if b_valid(g_src_hl) then
          api.nvim_buf_clear_namespace(g_src_hl, NS_LOC, 0, -1)
        end
        g_src_hl = nil
      end

      local compiler = g_asm_map[bufnr]
      if compiler and compiler:in_sync() then
        local line = compiler:get_src_line(lnum)
        if line then
          if pcall(b_set_mark, compiler.src_buf, NS_LOC, line - 1, 0, {
            end_row = line,
            hl_group = 'Visual',
            hl_eol = true,
          }) then
            g_src_hl = compiler.src_buf
          end
        end
      end
    end
  end

  api.nvim_create_autocmd('CursorMoved', {
    desc = 'neobolt: update highlights',
    callback = function(ev)
      update_hls(ev.buf)
    end,
  })
end


local function get_config(bufnr)
  bufnr = normalize_bufnr(bufnr)

  local compiler = g_asm_map[bufnr]
  if not compiler then
    return nil
  end

  return {
    cwd = compiler.cwd,
    exe = compiler.exe,
    args = vim.deepcopy(compiler.user_args), -- don't leak table ref to the outside world
  }
end

local function set_config(bufnr, config)
  bufnr = normalize_bufnr(bufnr)

  local compiler = g_asm_map[bufnr]
  if not compiler then
    error('invalid bufnr, not an asm buffer')
    return
  end

  assert(type(config) == 'table', 'expected table')

  local cwd = config.cwd
  local exe = config.exe
  local args = config.args

  if exe ~= nil then
    assert(type(exe) == 'string', 'expected string for `config.exe`')
    exe = fn.exepath(exe)
    assert(exe ~= '', 'invalid compiler executable')
  end
  if cwd ~= nil then
    assert(type(cwd) == 'string', 'expected string for `config.cwd`')
  end
  if args ~= nil then
    assert(type(args) == 'table', 'expected string array for `config.args`')
    for _, arg in ipairs(args) do
      assert(type(arg) == 'string', 'expected string array for `config.args`')
    end
  end

  if exe ~= nil then
    compiler.exe = exe
  end
  if cwd ~= nil then
    compiler.cwd = cwd
  end
  if args ~= nil then
    compiler.user_args = vim.deepcopy(args) -- don't leak table ref to the outside world
  end

  compiler:update()
end


return {
  -- TODO: for debugging. hide this in a separate module
  _asm_map = g_asm_map,
  _src_map = g_src_map,

  _new_compiler = Compiler.new, -- TODO: not final

  get_config = get_config,
  set_config = set_config,
}

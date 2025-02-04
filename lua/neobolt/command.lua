local function print_err(msg)
  vim.api.nvim_err_writeln('neobolt: ' .. msg)
end

local function is_string_array(v)
  if type(v) ~= 'table' then
    return false
  end
  for i = 1, #v do
    if type(v[i]) ~= 'string' then
      return false
    end
  end
  return true
end

local function config_err(name, msg)
  return ('Invalid configuration for compiler %q: %s'):format(name, msg)
end

local function make_opts(name, config, args)
  if type(config.path) ~= 'string' then
    return nil, config_err(name, 'Invalid path, expected string')
  end
  local path = vim.fn.exepath(config.path)
  if not path or path == '' then
    return nil, config_err(name, 'Invalid path, executable not found')
  end

  if not is_string_array(config.base_args) then
    return nil, config_err(name, 'Invalid base_args, expected string array')
  end
  local base_args = vim.deepcopy(config.base_args)

  local user_args = {}
  if config.user_args then
    if not is_string_array(config.user_args) then
      return nil, config_err(name, 'Invalid user_args, expected string array or nil')
    end
    for i = 1, #config.user_args do
      table.insert(user_args, config.user_args[i])
    end
  end
  if args then
    for i = 1, #args do
      table.insert(user_args, args[i])
    end
  end

  return {
    exe = path,
    base_args = base_args,
    user_args = user_args,
  }
end

local function cmd_execute(ev)
  local config = nil ---@type neobolt.Config?
  local name = nil ---@type string?
  local args = nil ---@type string[]?

  if #ev.fargs == 0 then
    local filetype = vim.o.filetype
    config, name = require('neobolt.compilers').get_default_compiler(filetype)
    if not config then
      print_err(('No default compiler found for filetype %q'):format(filetype))
      return
    end
  else
    args = ev.fargs
    name = table.remove(args, 1)
    config = require('neobolt.compilers').get_compilers()[name]
    if not config then
      print_err(('Compiler %q not found'):format(name))
      return
    end
  end

  local opts, err = make_opts(name, config, args)
  if err then
    print_err(err)
    return
  end


  local win = vim.api.nvim_get_current_win()
  local src_buf = vim.api.nvim_get_current_buf()
  vim.cmd('vnew') -- TODO: configurable position
  local asm_buf = vim.api.nvim_get_current_buf()
  local compiler = require('neobolt.compiler')._new_compiler(asm_buf, src_buf, opts)
  compiler:init()
  vim.api.nvim_set_current_win(win)
  compiler:update()
end

local function parse_cmdline(cmdname, arglead, cmdline, pos)
  -- create regex for removing the command name
  assert(cmdname:find('^%u%w*$'))
  local re = '^[: \t]*' .. cmdname:sub(1, 1)
  if #cmdname > 1 then
    re = re .. '\\%[' .. cmdname:sub(2) .. ']'
  end
  re = re .. '[ \t]*\\zs'
  re = vim.regex(re)

  -- get leading arguments
  local args = cmdline:sub(1, #cmdline - #arglead)
  local skip = re:match_str(args)
  if not skip then return end
  args = args:sub(skip + 1)
  args = vim.split(args, '[ \t]+', { trimempty = true }) -- TODO: handle backslashes

  -- get last (currently completed) argument
  local last = cmdline:sub(#cmdline - #arglead + 1, pos)

  return args, last
end

local function cmd_complete(cmdname, arglead, cmdline, pos)
  local args, last = parse_cmdline(cmdname, arglead, cmdline, pos)
  if not args then return end

  if #args == 0 then
    local candidates = {}
    for name, _ in pairs(require('neobolt.compilers').get_compilers()) do
      -- TODO: filter by filetype?
      if name:sub(1, #last) == last then
        table.insert(candidates, name)
      end
    end
    table.sort(candidates)
    return candidates
  end
end

return {
  _execute = cmd_execute,
  _complete = cmd_complete,
}

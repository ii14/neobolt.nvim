local M = {}


---@class neobolt.Config
---@field base_args string[]
---@field user_args string[]

---@alias neobolt.ConfigMap table<string, neobolt.Config>

--- New compiler configuration
---@param config neobolt.Config Configuration
---@param base neobolt.Config Base configuration
---@return neobolt.Config
function M.Config(config, base)
  config.__index = config
  if base ~= nil then
    assert(type(base) == 'table')
    ---@diagnostic disable-next-line: undefined-field
    assert(base.__index)
    setmetatable(config, base)
  end
  return config
end

-- on x86 use intel syntax by default
-- TODO: detect cross compilers
local x86 = not not vim.loop.os_uname().machine:find('x86')
local function gnu_default_user_args()
  if x86 then
    return { '-masm=intel', '-Wall', '-Wextra' }
  else
    return { '-Wall', '-Wextra' }
  end
end

--- Base compiler configurations
---@type neobolt.ConfigMap
M.templates = {

  gnu_c = M.Config {
    base_args = { '-S', '-g', '-x', 'c', '-o', '-', '-' },
    user_args = gnu_default_user_args(),
  },

  gnu_cpp = M.Config {
    base_args = { '-S', '-g', '-x', 'c++', '-o', '-', '-' },
    user_args = gnu_default_user_args(),
  },

  -- TODO: complains about no main function
  -- rustc = M.Config {
  --   base_args = {
  --     '--emit', 'asm',
  --     '-C', 'debuginfo=1',
  --     -- '-C', 'llvm-args=-x86-asm-syntax=intel',
  --     '-o', '-',
  --     '-',
  --   },
  -- },

}


---@type neobolt.ConfigMap
local name_lookup = {
  ['cc'] = M.templates.gnu_c,
  ['gcc'] = M.templates.gnu_c,
  ['clang'] = M.templates.gnu_c,

  ['c++'] = M.templates.gnu_cpp,
  ['g++'] = M.templates.gnu_cpp,
  ['clang++'] = M.templates.gnu_cpp,

  -- ['rustc'] = M.templates.rustc,

  -- ['zig'] = ..., -- TODO: stdin not supported
}

-- matches names like gcc-9 or aarch64-linux-gnu-gcc
---@param str string
---@param pat string
---@return boolean
local function match_alt_names(str, pat)
  local pos = str:find(pat .. '%-[%d%.]+$')
  if not pos then
    return false
  elseif pos == 1 then
    return true
  else
    return str:sub(pos - 1, pos - 1) == '-'
  end
end

---@param name string
---@return neobolt.Config?
local function match_name(name)
  if name_lookup[name] then
    return name_lookup[name]
  elseif match_alt_names(name, 'gcc') or match_alt_names(name, 'clang') then
    return M.templates.gnu_c
  elseif match_alt_names(name, 'g%+%+') or match_alt_names(name, 'clang%+%+') then
    return M.templates.gnu_cpp
  else
    return nil
  end
end

--- Automatic compiler detection
---@return neobolt.ConfigMap
M.detect_compilers = function()
  local PATH = vim.split(assert(vim.env.PATH), ':', { trimempty = true, plain = true })
  local res = {} ---@type neobolt.ConfigMap

  for i = #PATH, 1, -1 do
    local dir = vim.loop.fs_opendir(PATH[i], nil, 64)
    if dir then
      while true do
        local ents = dir:readdir()
        if ents == nil then
          break
        end

        for _, ent in ipairs(ents) do
          local template = match_name(ent.name)
          if template then
            local path = PATH[i] .. '/' .. ent.name
            -- i believe this can be somewhat expensive on windows?
            -- thankfully we don't support windows
            if vim.fn.executable(path) then
              -- inherit from template
              res[ent.name] = M.Config({ path = path }, template)
            end
          end
        end
      end
      dir:closedir()
    end
  end

  return res
end


---@param config neobolt.Config?
local function validate_compiler_config(config)
  assert(type(config) == 'table')
  assert(type(config.base_args) == 'table')
  for _, v in ipairs(config.base_args) do
    assert(type(v) == 'string')
  end
end

---@type neobolt.ConfigMap
local compilers = {}
---@type neobolt.ConfigMap
local detected_compilers = nil

--- Set compiler configuration
---@param name string
---@param config neobolt.Config
function M.set_compiler(name, config)
  assert(type(name) == 'string')
  if config then
    validate_compiler_config(config)
  end -- let nil and false through
  compilers[name] = config
end

--- Get compilers
---@return neobolt.ConfigMap
function M.get_compilers()
  if detected_compilers == nil then
    detected_compilers = M.detect_compilers()
    assert(type(detected_compilers) == 'table')
    for name, config in pairs(detected_compilers) do
      validate_compiler_config(config)
      if compilers[name] == nil then
        compilers[name] = config
      end
    end
  end
  return compilers
end


--- Maps filetype to default compiler name(s)
---@type table<string, string|string[]>
M.default_compilers = {
  c = { 'gcc', 'clang', 'cc' },
  cpp = { 'g++', 'clang++', 'c++' },
  -- rust = 'rustc',
}

--- Get default compiler for a filetype
---@param filetype string
---@return neobolt.Config?
function M.get_default_compiler(filetype)
  assert(type(filetype) == 'string')
  assert(type(M.default_compilers) == 'table')

  local default = M.default_compilers[filetype]

  if type(default) == 'string' then
    return M.get_compilers()[default], default
  elseif type(default) == 'table' then
    local configs = M.get_compilers()
    for _, name in ipairs(default) do
      assert(type(name) == 'string')
      if configs[name] then
        return configs[name], name
      end
    end
  end

  return nil
end


return M

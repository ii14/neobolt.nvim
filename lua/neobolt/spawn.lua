local uv = vim.loop

local Process = { __index = {} }

function Process.__index:abort()
  if self._proc and not self._abort then
    self._proc:kill(uv.constants.SIGKILL)
    self._abort = true
  end
end

return function(exe, args, cwd, input, callback)
  assert(type(exe) == 'string')
  assert(type(args) == 'table')
  assert(type(cwd) == 'string')
  assert(type(input) == 'string')
  assert(type(callback) == 'function')

  local self = setmetatable({
    _proc = nil,
    _abort = false,

    cwd = cwd,
    exe = exe,
    args = args,

    code = nil,
    signal = nil,
    time = nil,

    stdout = nil,
    stderr = nil,
  }, Process)

  local out = {}
  local err = {}

  local stdin = assert(uv.new_pipe())
  local stdout = assert(uv.new_pipe())
  local stderr = assert(uv.new_pipe())

  local ts = uv.hrtime()

  local spawn_err
  self._proc, spawn_err = uv.spawn(exe, {
    args = args,
    stdio = { stdin, stdout, stderr },
  }, function(code, signal)
    stdin:close()
    stdout:close()
    stderr:close()

    self._proc:close()
    self._proc = nil

    if self._abort then
      return
    end

    self.time = (uv.hrtime() - ts) / 1e+9
    self.code = code
    self.signal = signal
    self.stderr = table.concat(err)
    self.stdout = table.concat(out)

    vim.schedule(function()
      callback(self)
    end)
  end)

  if not self._proc then
    stdin:close()
    stdout:close()
    stderr:close()
    return nil, spawn_err
  end

  stdout:read_start(function(_, data)
    if data then
      table.insert(out, data)
    end
  end)

  stderr:read_start(function(_, data)
    if data then
      table.insert(err, data)
    end
  end)

  stdin:write(input, function()
    stdin:shutdown(function() end)
  end)

  return self
end

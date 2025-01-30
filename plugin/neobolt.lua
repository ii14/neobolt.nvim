if vim.g.neobolt_loaded ~= nil then return end
vim.g.neobolt_loaded = true

vim.api.nvim_create_user_command('Neobolt', function(ev)
  require('neobolt.compiler')._cmd_execute(ev)
end, {
  force = true,
  nargs = '*',
  complete = function(ev)
    return require('neobolt.compiler')._cmd_complete(ev)
  end
})

vim.api.nvim_create_autocmd('BufReadCmd', {
  pattern = 'neobolt://*',
  nested = true,
  callback = function(ev)
    local err = require('neobolt.compiler')._buf_read_cmd(ev)
    if err then
      vim.api.nvim_err_writeln(err)
    end
  end,
})

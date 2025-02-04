if vim.g.neobolt_loaded ~= nil then return end
vim.g.neobolt_loaded = true

vim.api.nvim_create_user_command('Neobolt', function(ev)
  require('neobolt.command')._execute(ev)
end, {
  force = true,
  nargs = '*',
  complete = function(arglead, cmdline, pos)
    return require('neobolt.command')._complete('Neobolt', arglead, cmdline, pos)
  end
})

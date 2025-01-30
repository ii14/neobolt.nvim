local M = {}

function M.get_config(bufnr)
  return require('neobolt.compiler').get_config(bufnr)
end

function M.set_config(bufnr, config)
  return require('neobolt.compiler').set_config(bufnr, config)
end

return M

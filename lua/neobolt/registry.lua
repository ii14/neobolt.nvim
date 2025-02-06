local asm_map = {}
local src_map = {}

local function register(compiler)
  local asm_buf = assert(compiler.asm_buf)
  local src_buf = assert(compiler.src_buf)
  -- register in the asm map
  asm_map[asm_buf] = compiler
  -- register in the src map
  if src_map[src_buf] == nil then
    src_map[src_buf] = {}
  end
  src_map[src_buf][asm_buf] = compiler
end

local function unregister(compiler)
  local asm_buf = assert(compiler.asm_buf)
  local src_buf = assert(compiler.src_buf)
  -- unregister from the asm map
  if asm_map[asm_buf] == compiler then
    asm_map[asm_buf] = nil
  end
  -- unregister from the src map
  if src_map[src_buf] then
    if src_map[src_buf][asm_buf] == compiler then
      src_map[src_buf][asm_buf] = nil
      if next(src_map[src_buf]) == nil then
        src_map[src_buf] = nil
      end
    end
  end
end

return {
  asm_map = asm_map,
  src_map = src_map,
  register = register,
  unregister = unregister,
}

#include "neobolt.c"

int LLVMFuzzerTestOneInput(
    const u8* data,
    usize size)
{
  State state;
  if (neobolt_init(&state, data, size)) {
    neobolt_parse(&state);
    neobolt_destroy(&state);
  }
  return 0;
}

// vim: sw=2 sts=2 et

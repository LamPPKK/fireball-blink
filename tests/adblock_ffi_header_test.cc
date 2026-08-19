#include "fireball/components/adblock/include/fireball_adblock_ffi.h"

#include <cassert>
#include <cstdint>
#include <type_traits>

int main() {
  static_assert(std::is_standard_layout_v<FireballAdblockDecision>);
  static_assert(sizeof(FireballAdblockDecision::status) == sizeof(std::int32_t));
  static_assert(sizeof(FireballAdblockDecision::flags) == sizeof(std::uint32_t));
  assert(FIREBALL_ADBLOCK_STATUS_OK == 0);
  assert(FIREBALL_ADBLOCK_FLAG_BLOCK == 1u);
  return 0;
}

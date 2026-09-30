#pragma once

#include "std/target_os.hpp"

#include <mutex>
#include <shared_mutex>

namespace classificator
{
// Desktop Designer reloads style data while type identities remain immutable. Nested read scopes
// share one lock, so compound visibility checks and type sorting observe one complete rule set.
class DrawingRulesReadGuard
{
public:
#ifdef OMIM_OS_DESKTOP
  DrawingRulesReadGuard();
  ~DrawingRulesReadGuard();

  DrawingRulesReadGuard(DrawingRulesReadGuard const &) = delete;
  DrawingRulesReadGuard & operator=(DrawingRulesReadGuard const &) = delete;

private:
  bool m_enabled = false;
#else
  DrawingRulesReadGuard() {}
  ~DrawingRulesReadGuard() {}
#endif
};

#ifdef OMIM_OS_DESKTOP
std::unique_lock<std::shared_mutex> LockDrawingRulesForReload();
#endif
}  // namespace classificator

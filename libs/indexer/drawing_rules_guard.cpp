#include "indexer/drawing_rules_guard.hpp"

#include "indexer/map_style_reader.hpp"

#include "base/assert.hpp"

#ifdef OMIM_OS_DESKTOP
namespace classificator
{
namespace
{
std::shared_mutex g_drawingRulesMutex;
thread_local unsigned g_readDepth = 0;
}  // namespace

DrawingRulesReadGuard::DrawingRulesReadGuard() : m_enabled(GetStyleReader().IsDesignerMode())
{
  if (m_enabled && g_readDepth++ == 0)
    g_drawingRulesMutex.lock_shared();
}

DrawingRulesReadGuard::~DrawingRulesReadGuard()
{
  if (m_enabled && --g_readDepth == 0)
    g_drawingRulesMutex.unlock_shared();
}

std::unique_lock<std::shared_mutex> LockDrawingRulesForReload()
{
  CHECK_EQUAL(g_readDepth, 0, ("A rule reload cannot run inside a reader"));
  return std::unique_lock(g_drawingRulesMutex);
}
}  // namespace classificator
#endif

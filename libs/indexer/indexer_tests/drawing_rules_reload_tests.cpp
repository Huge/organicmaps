#include "std/target_os.hpp"

#ifdef OMIM_OS_DESKTOP
#include "testing/testing.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/drawing_rules.hpp"
#include "indexer/feature_data.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/map_style_reader.hpp"

#include "platform/platform.hpp"

#include "base/scope_guard.hpp"

#include <algorithm>
#include <atomic>
#include <thread>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>

namespace drawing_rules_reload_tests
{
UNIT_TEST(Designer_RuleReloadPreservesTypeIdentitiesAndConcurrentReaders)
{
  auto & reader = GetStyleReader();
  auto const style = reader.GetCurrentStyle();
  auto const designer = reader.IsDesignerMode();
  reader.SetDesignerMode(true);
  reader.SetCurrentStyle(MapStyleDefaultLight);
  SCOPE_GUARD(restore, [&]
  {
    reader.SetDesignerMode(designer);
    reader.SetCurrentStyle(style);
  });
  classificator::Load();

  auto const type = classif().GetTypeByPath({"amenity", "bench"});
  auto const index = classif().GetIndexForType(type);
  auto const object = classif().GetObject(type);
  auto const name = classif().GetReadableObjectName(type);
  auto const range = feature::GetDrawableScaleRange(type);
  auto const priority = object->GetMaxOverlaysPriority();
  auto const background = drule::GetCurrentRules().GetBgColor(10);
  feature::TypesHolder input(feature::GeomType::Point);
  input.Add(type);
  input.Add(classif().GetTypeByPath({"amenity", "hospital"}));
  input.Add(classif().GetTypeByPath({"building"}));
  auto expected = input;
  expected.SortBySpec();

  std::atomic<bool> done = false;
  std::atomic<bool> consistent = true;
  std::atomic<unsigned> reads = 0;
  std::thread worker([&]
  {
    while (!done.load())
    {
      if (classif().GetIndexForType(type) != index || classif().GetTypeForIndex(index) != type ||
          classif().GetReadableObjectName(type) != name || classif().GetObject(type) != object ||
          feature::GetDrawableScaleRange(type) != range || object->GetMaxOverlaysPriority() != priority ||
          drule::GetCurrentRules().GetBgColor(10) != background)
        consistent = false;
      auto types = input;
      types.SortBySpec();
      if (!std::equal(types.begin(), types.end(), expected.begin()))
        consistent = false;
      ++reads;
    }
  });
  for (int i = 0; i < 10; ++i)
    classificator::ReloadDrawingRules();
  done = true;
  worker.join();
  TEST(consistent.load(), ());
  TEST_GREATER(reads.load(), 0, ());
}

UNIT_TEST(Designer_RejectsChangedTypeSourcesBeforePublication)
{
  auto & reader = GetStyleReader();
  auto const style = reader.GetCurrentStyle();
  auto const designer = reader.IsDesignerMode();
  reader.SetDesignerMode(true);
  GetStyleReader().SetCurrentStyle(MapStyleDefaultLight);
  SCOPE_GUARD(restore, [&]
  {
    reader.SetDesignerMode(designer);
    reader.SetCurrentStyle(style);
  });
  classificator::Load();
  QTemporaryDir dir;
  TEST(dir.isValid(), ());
  for (char const * name : {"classificator.txt", "types.txt"})
  {
    auto const source = GetPlatform().ReadPathForFile(name);
    TEST(QFile::copy(QString::fromStdString(source), dir.path() + "/" + name), ());
  }
  classificator::CheckTypesCompatible(dir.path().toStdString());
  QFile file(dir.path() + "/types.txt");
  TEST(file.open(QIODevice::Append), ());
  TEST_GREATER(file.write("changed mapping"), 0, ());
  file.close();
  bool rejected = false;
  try
  {
    classificator::CheckTypesCompatible(dir.path().toStdString());
  }
  catch (std::runtime_error const &)
  {
    rejected = true;
  }
  TEST(rejected, ());
}
}  // namespace drawing_rules_reload_tests
#endif

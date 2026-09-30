#include "testing/testing.hpp"

#include "qt/build_style/build_common.h"
#include "qt/build_style/build_style.h"

#include "tools/skin_generator/generator.hpp"

#include "map/framework.hpp"
#include "search/search_params.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryDir>
#include <QtWidgets/QApplication>

#include <stdexcept>

namespace designer_tests
{
namespace
{
void EnsureApp()
{
  if (!QCoreApplication::instance())
  {
    static int argc = 0;
    static QApplication app(argc, nullptr);
  }
}

void Write(QString const & path, QByteArray const & data)
{
  TEST(QDir().mkpath(QFileInfo(path).absolutePath()), (path.toStdString()));
  QFile file(path);
  TEST(file.open(QIODevice::WriteOnly), (path.toStdString()));
  TEST_EQUAL(file.write(data), data.size(), (path.toStdString()));
}

bool Throws(auto && action)
{
  try
  {
    action();
    return false;
  }
  catch (std::runtime_error const &)
  {
    return true;
  }
}

struct StyleFixture
{
  QTemporaryDir m_dir;
  QString m_mapcss;
  QString m_output;
  build_style::StyleInfo m_info;

  explicit StyleFixture(bool symbols = false)
  {
    m_mapcss = m_dir.path() + "/data/styles/default/light/style.mapcss";
    m_output = QFileInfo(m_mapcss).absolutePath() + "/out";
    Write(m_mapcss, "style source");
    TEST(build_style::TryParseStyleInfo(m_mapcss, m_info), ());
    Write(m_output + "/drules_default.bin", "drawing rules");
    if (symbols)
      TEST(QDir().mkpath(QFileInfo(m_mapcss).absolutePath() + "/symbols"), ());
  }
};
}  // namespace

UNIT_TEST(Designer_PhoneExportRejectsOverlappingSourcesBeforeConfirmation)
{
  StyleFixture fixture;
  bool asked = false;
  TEST(Throws(
           [&]
  {
    build_style::ExportPhonePackage(fixture.m_mapcss, fixture.m_info, fixture.m_dir.path() + "/data",
                                    [&](QString const &)
    {
      asked = true;
      return true;
    });
  }),
       ());
  TEST(!asked, ());
  TEST(QFileInfo::exists(fixture.m_mapcss), ());
  TEST(QFileInfo::exists(fixture.m_output + "/drules_default.bin"), ());
}

UNIT_TEST(Designer_MissingAtlasPreservesExistingExport)
{
  StyleFixture fixture(true);
  auto const target = fixture.m_dir.path() + "/export";
  Write(target + "/styles/keep", "existing package");
  bool asked = false;
  TEST(Throws(
           [&]
  {
    build_style::ExportPhonePackage(fixture.m_mapcss, fixture.m_info, target, [&](QString const &)
    {
      asked = true;
      return true;
    });
  }),
       ());
  TEST(!asked, ());
  TEST(QFileInfo::exists(target + "/styles/keep"), ());
}

UNIT_TEST(Designer_DeclinedOverwritePreservesExistingExport)
{
  StyleFixture fixture;
  auto const target = fixture.m_dir.path() + "/export";
  Write(target + "/styles/keep", "existing package");
  auto const result =
      build_style::ExportPhonePackage(fixture.m_mapcss, fixture.m_info, target, [](QString const &) { return false; });
  TEST(result.isEmpty(), ());
  TEST(QFileInfo::exists(target + "/styles/keep"), ());
}

UNIT_TEST(Designer_ExportsCompleteDefaultAtlas)
{
  StyleFixture fixture(true);
  auto const target = fixture.m_dir.path() + "/export";
  Write(target + "/styles/keep", "existing package");
  for (char const * dpi : {"mdpi", "hdpi", "xhdpi", "6plus", "xxhdpi", "xxxhdpi"})
    for (char const * leaf : {"symbols.png", "symbols.xml"})
      Write(fixture.m_output + "/symbols/" + dpi + "/light/" + leaf, "atlas");
  auto const result =
      build_style::ExportPhonePackage(fixture.m_mapcss, fixture.m_info, target, [](QString const &) { return true; });
  TEST_EQUAL(result.toStdString(), QDir(target + "/styles").canonicalPath().toStdString(), ());
  TEST(!QFileInfo::exists(result + "/keep"), ());
  TEST(QFileInfo::exists(result + "/drules_default.bin"), ());
  for (char const * dpi : {"mdpi", "hdpi", "xhdpi", "6plus", "xxhdpi", "xxxhdpi"})
    for (char const * leaf : {"symbols.png", "symbols.xml"})
      TEST(QFileInfo::exists(result + "/symbols/" + dpi + "/light/" + leaf), ());
  TEST(QFileInfo::exists(fixture.m_mapcss), ());
}

UNIT_TEST(Designer_SkinRejectsMalformedSvgAndOversizedInitialPage)
{
  EnsureApp();
  QTemporaryDir dir;
  auto const sources = dir.path() + "/symbols";
  auto const output = dir.path() + "/out";
  TEST(QDir().mkpath(output), ());
  Write(sources + "/good.svg",
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"18\" height=\"18\">"
        "<rect width=\"18\" height=\"18\" fill=\"red\"/></svg>");
  Write(sources + "/bad.svg", "<svg malformed");
  TEST(Throws([&] { tools::BuildSkin(sources, 18, 4096, output); }), ());
  TEST(QFile::remove(sources + "/bad.svg"), ());
  TEST(Throws([&] { tools::BuildSkin(sources, 18, 16, output); }), ());
  tools::BuildSkin(sources, 18, 4096, output);
  TEST(QFileInfo::exists(output + "/symbols.xml"), ());
}

UNIT_TEST(Designer_FixedStyleSurvivesDebugSearchCommands)
{
  FrameworkParams params;
  params.m_fixedMapStyle = MapStyleOutdoorsDark;
  Framework framework(params, false);
  TEST_EQUAL(framework.GetMapStyle(), MapStyleOutdoorsDark, ());
  search::SearchParams query;
  query.m_query = "?light";
  TEST(framework.ParseSearchQueryCommand(query), ());
  TEST_EQUAL(framework.GetMapStyle(), MapStyleOutdoorsDark, ());
  framework.MarkMapStyle(MapStyleVehicleLight);
  TEST_EQUAL(framework.GetMapStyle(), MapStyleOutdoorsDark, ());
}
}  // namespace designer_tests

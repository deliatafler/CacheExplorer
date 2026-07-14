#include "QtHelpers.h"

#include <QByteArray>
#include <QStandardPaths>

QString ToQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString PathToQString(const std::filesystem::path& path)
{
#ifdef _WIN32
    return QString::fromStdWString(path.wstring());
#else
    return QString::fromUtf8(path.u8string().c_str());
#endif
}

std::filesystem::path PathFromQString(const QString& value)
{
    const QByteArray utf8 = value.toUtf8();
    return std::filesystem::u8path(utf8.constData());
}

std::filesystem::path ResolveTextureCacheDirectory(
    const std::filesystem::path& suppliedPath)
{
    if (std::filesystem::exists(suppliedPath / "texture.entries"))
    {
        return suppliedPath;
    }

    const std::filesystem::path childTextureCache =
        suppliedPath / "texturecache";

    if (std::filesystem::exists(childTextureCache / "texture.entries"))
    {
        return childTextureCache;
    }

    return suppliedPath;
}

QString DefaultCachePath()
{
#ifdef _WIN32
    const QString localAppData =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

    if (!localAppData.isEmpty())
    {
        return localAppData + "/FirestormOS_x64/texturecache";
    }
#endif

    return {};
}

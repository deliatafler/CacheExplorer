#pragma once

#include <filesystem>
#include <string>

#include <QString>

QString ToQString(const std::string& value);

QString PathToQString(const std::filesystem::path& path);

std::filesystem::path PathFromQString(const QString& value);

std::filesystem::path ResolveTextureCacheDirectory(
    const std::filesystem::path& suppliedPath);

QString DefaultCachePath();

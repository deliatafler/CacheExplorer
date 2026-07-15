#pragma once

#include "PreviewDecodeWorker.h"
#include "TextureCacheDatabase.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include <QString>

QString ToQString(const std::string& value);

QString PathToQString(const std::filesystem::path& path);

std::filesystem::path PathFromQString(const QString& value);

std::filesystem::path ResolveTextureCacheDirectory(
    const std::filesystem::path& suppliedPath);

QString DefaultCachePath();

QString PreferredCachePath();

bool DefaultCachePathExists();

void RememberOpenedCachePath(const QString& cachePath);

const char* CacheErrorMessage(CacheError error);

QString LoadedCacheStatus(
    std::size_t validEntryCount,
    std::uint32_t slotCount,
    float cacheVersion);

QString PreviewReadyStatus(
    const std::string& uuidText,
    std::uint32_t width,
    std::uint32_t height);

QString PreviewUnavailablePanelMessage(PreviewDecodeStatus status);

QString PreviewUnavailableStatus(
    PreviewDecodeStatus status,
    const std::string& detailMessage);

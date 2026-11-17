#pragma once

#include <filesystem>

#include <QString>

class PreviewCache;
class LegacyTextureCacheSource;

std::filesystem::path PreviewStateFilePath(
    const std::filesystem::path& cacheDirectory);

bool LoadPreviewState(
    const std::filesystem::path& stateFile,
    const LegacyTextureCacheSource& source,
    PreviewCache& previewCache,
    QString& errorMessage);

bool SavePreviewState(
    const std::filesystem::path& stateFile,
    const LegacyTextureCacheSource& source,
    const PreviewCache& previewCache,
    QString& errorMessage);

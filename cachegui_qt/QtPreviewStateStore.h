#pragma once

#include <filesystem>

#include <QString>

class PreviewCache;
class TextureCacheDatabase;

std::filesystem::path PreviewStateFilePath(
    const std::filesystem::path& cacheDirectory);

bool LoadPreviewState(
    const std::filesystem::path& stateFile,
    const TextureCacheDatabase& database,
    PreviewCache& previewCache,
    QString& errorMessage);

bool SavePreviewState(
    const std::filesystem::path& stateFile,
    const TextureCacheDatabase& database,
    const PreviewCache& previewCache,
    QString& errorMessage);

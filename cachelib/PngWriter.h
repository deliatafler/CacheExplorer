#pragma once

#include <filesystem>

#include "J2CDecoder.h"

enum class PngWriteError
{
    None,
    InvalidImage,
    DirectoryCreationFailed,
    WriteFailed
};

class PngWriter
{
public:
    PngWriteError Write(
        const std::filesystem::path& outputFile,
        const DecodedImage& image) const;

    static const char* ErrorMessage(
        PngWriteError error);
};

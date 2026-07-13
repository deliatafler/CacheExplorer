#include "PngWriter.h"

#include <png.h>

#include <cstdint>
#include <limits>
#include <system_error>

PngWriteError PngWriter::Write(
    const std::filesystem::path& outputFile,
    const DecodedImage& image) const
{
    if (image.width == 0 ||
        image.height == 0)
    {
        return PngWriteError::InvalidImage;
    }

    const std::uint64_t expectedSize =
        static_cast<std::uint64_t>(image.width)
        * static_cast<std::uint64_t>(image.height)
        * 4;

    if (expectedSize != image.rgba.size())
    {
        return PngWriteError::InvalidImage;
    }

    const std::filesystem::path parent =
        outputFile.parent_path();

    if (!parent.empty())
    {
        std::error_code error;

        std::filesystem::create_directories(
            parent,
            error);

        if (error)
        {
            return PngWriteError::DirectoryCreationFailed;
        }
    }

    png_image png{};

    png.version = PNG_IMAGE_VERSION;
    png.width = image.width;
    png.height = image.height;
    png.format = PNG_FORMAT_RGBA;

#ifdef _WIN32
    const std::string filename =
        outputFile.string();
#else
    const std::string filename =
        outputFile.string();
#endif

    const int success =
        png_image_write_to_file(
            &png,
            filename.c_str(),
            0,
            image.rgba.data(),
            0,
            nullptr);

    png_image_free(&png);

    if (!success)
    {
        return PngWriteError::WriteFailed;
    }

    return PngWriteError::None;
}

const char* PngWriter::ErrorMessage(
    PngWriteError error)
{
    switch (error)
    {
        case PngWriteError::None:
            return "No error.";

        case PngWriteError::InvalidImage:
            return "The decoded image buffer is invalid.";

        case PngWriteError::DirectoryCreationFailed:
            return "The output directory could not be created.";

        case PngWriteError::WriteFailed:
            return "libpng could not write the PNG file.";
    }

    return "Unknown PNG writing error.";
}

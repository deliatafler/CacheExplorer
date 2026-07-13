#include "J2CDecoder.h"

#include <openjpeg.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>

namespace
{
    struct MemoryStream
    {
        const std::uint8_t* data = nullptr;
        std::size_t size = 0;
        std::size_t position = 0;
    };

    OPJ_SIZE_T ReadFromMemory(
        void* destination,
        OPJ_SIZE_T requestedBytes,
        void* userData)
    {
        auto* stream =
            static_cast<MemoryStream*>(userData);

        if (stream == nullptr ||
            destination == nullptr ||
            stream->position >= stream->size)
        {
            return static_cast<OPJ_SIZE_T>(-1);
        }

        const std::size_t available =
            stream->size - stream->position;

        const std::size_t bytesToRead =
            std::min<std::size_t>(
                available,
                static_cast<std::size_t>(requestedBytes));

        std::copy_n(
            stream->data + stream->position,
            bytesToRead,
            static_cast<std::uint8_t*>(destination));

        stream->position += bytesToRead;

        return static_cast<OPJ_SIZE_T>(bytesToRead);
    }

    OPJ_OFF_T SkipMemory(
        OPJ_OFF_T requestedOffset,
        void* userData)
    {
        auto* stream =
            static_cast<MemoryStream*>(userData);

        if (stream == nullptr ||
            requestedOffset < 0)
        {
            return static_cast<OPJ_OFF_T>(-1);
        }

        const std::size_t offset =
            static_cast<std::size_t>(requestedOffset);

        const std::size_t available =
            stream->size - stream->position;

        const std::size_t skipped =
            std::min(offset, available);

        stream->position += skipped;

        return static_cast<OPJ_OFF_T>(skipped);
    }

    OPJ_BOOL SeekMemory(
        OPJ_OFF_T requestedPosition,
        void* userData)
    {
        auto* stream =
            static_cast<MemoryStream*>(userData);

        if (stream == nullptr ||
            requestedPosition < 0)
        {
            return OPJ_FALSE;
        }

        const std::size_t position =
            static_cast<std::size_t>(requestedPosition);

        if (position > stream->size)
        {
            return OPJ_FALSE;
        }

        stream->position = position;

        return OPJ_TRUE;
    }

    void OpenJpegWarning(
        const char* message,
        void*)
    {
        if (message != nullptr)
        {
            std::cerr
                << "OpenJPEG warning: "
                << message;
        }
    }

    void OpenJpegError(
        const char* message,
        void*)
    {
        if (message != nullptr)
        {
            std::cerr
                << "OpenJPEG error: "
                << message;
        }
    }

    struct StreamDeleter
    {
        void operator()(opj_stream_t* stream) const
        {
            if (stream != nullptr)
            {
                opj_stream_destroy(stream);
            }
        }
    };

    struct CodecDeleter
    {
        void operator()(opj_codec_t* codec) const
        {
            if (codec != nullptr)
            {
                opj_destroy_codec(codec);
            }
        }
    };

    struct ImageDeleter
    {
        void operator()(opj_image_t* image) const
        {
            if (image != nullptr)
            {
                opj_image_destroy(image);
            }
        }
    };

    using StreamPtr =
        std::unique_ptr<opj_stream_t, StreamDeleter>;

    using CodecPtr =
        std::unique_ptr<opj_codec_t, CodecDeleter>;

    using ImagePtr =
        std::unique_ptr<opj_image_t, ImageDeleter>;

    bool ComponentsHaveData(
        const opj_image_t& image)
    {
        if (image.comps == nullptr || image.numcomps == 0)
        {
            return false;
        }

        for (OPJ_UINT32 i = 0; i < image.numcomps; ++i)
        {
            if (image.comps[i].data == nullptr)
            {
                return false;
            }
        }

        return true;
    }

    bool ComponentsHaveMatchingDimensions(
        const opj_image_t& image)
    {
        if (image.numcomps == 0)
        {
            return false;
        }

        const opj_image_comp_t& first =
            image.comps[0];

        for (OPJ_UINT32 i = 1; i < image.numcomps; ++i)
        {
            const opj_image_comp_t& component =
                image.comps[i];

            if (component.w != first.w ||
                component.h != first.h ||
                component.dx != first.dx ||
                component.dy != first.dy)
            {
                return false;
            }
        }

        return true;
    }

    std::uint8_t ConvertComponentSample(
        const opj_image_comp_t& component,
        std::size_t index)
    {
        std::int64_t value =
            static_cast<std::int64_t>(component.data[index]);

        const std::uint32_t precision = component.prec;

        if (precision == 0 || precision > 31)
        {
            return 0;
        }

        std::int64_t minimum = 0;
        std::int64_t maximum = 0;

        if (component.sgnd != 0)
        {
            minimum =
                -(static_cast<std::int64_t>(1) << (precision - 1));

            maximum =
                (static_cast<std::int64_t>(1) << (precision - 1)) - 1;
        }
        else
        {
            maximum =
                (static_cast<std::int64_t>(1) << precision) - 1;
        }

        value = std::clamp(value, minimum, maximum);

        const std::int64_t range = maximum - minimum;

        if (range <= 0)
        {
            return 0;
        }

        const std::int64_t normalized =
            ((value - minimum) * 255 + range / 2) / range;

        return static_cast<std::uint8_t>(
            std::clamp<std::int64_t>(normalized, 0, 255));
    }
}

DecodeError J2CDecoder::Decode(
    const std::vector<std::uint8_t>& encodedData,
    DecodedImage& outputImage,
    bool verbose) const
{
    outputImage = {};

    if (encodedData.empty())
    {
        return DecodeError::EmptyInput;
    }

    MemoryStream memoryStream;
    memoryStream.data = encodedData.data();
    memoryStream.size = encodedData.size();

    StreamPtr stream(
        opj_stream_create(64 * 1024, OPJ_TRUE));

    if (!stream)
    {
        return DecodeError::StreamCreationFailed;
    }

    opj_stream_set_user_data(stream.get(), &memoryStream, nullptr);
    opj_stream_set_user_data_length(
        stream.get(),
        static_cast<OPJ_UINT64>(encodedData.size()));
    opj_stream_set_read_function(stream.get(), ReadFromMemory);
    opj_stream_set_skip_function(stream.get(), SkipMemory);
    opj_stream_set_seek_function(stream.get(), SeekMemory);

    CodecPtr codec(opj_create_decompress(OPJ_CODEC_J2K));

    if (!codec)
    {
        return DecodeError::CodecCreationFailed;
    }

    opj_set_info_handler(codec.get(), nullptr, nullptr);

    if (verbose)
    {
        opj_set_warning_handler(codec.get(), OpenJpegWarning, nullptr);
        opj_set_error_handler(codec.get(), OpenJpegError, nullptr);
    }
    else
    {
        opj_set_warning_handler(codec.get(), nullptr, nullptr);
        opj_set_error_handler(codec.get(), nullptr, nullptr);
    }

    opj_dparameters_t parameters{};
    opj_set_default_decoder_parameters(&parameters);

    if (!opj_setup_decoder(codec.get(), &parameters))
    {
        return DecodeError::DecodeFailed;
    }

    opj_image_t* rawImage = nullptr;

    if (!opj_read_header(stream.get(), codec.get(), &rawImage))
    {
        return DecodeError::HeaderReadFailed;
    }

    ImagePtr image(rawImage);

    if (!opj_decode(codec.get(), stream.get(), image.get()))
    {
        return DecodeError::DecodeFailed;
    }

    // Progressive cache streams may lack a clean end marker. If decoding
    // produced components, keep the pixels instead of failing finalization.
    static_cast<void>(opj_end_decompress(codec.get(), stream.get()));

    if (image == nullptr || !ComponentsHaveData(*image))
    {
        return DecodeError::MissingComponentData;
    }

    if (image->numcomps < 1 || image->numcomps > 4)
    {
        return DecodeError::UnsupportedComponentCount;
    }

    if (!ComponentsHaveMatchingDimensions(*image))
    {
        return DecodeError::UnsupportedComponentLayout;
    }

    const opj_image_comp_t& first = image->comps[0];

    if (first.w == 0 || first.h == 0)
    {
        return DecodeError::InvalidDimensions;
    }

    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(first.w) *
        static_cast<std::uint64_t>(first.h);

    if (pixelCount >
        static_cast<std::uint64_t>(
            std::numeric_limits<std::size_t>::max() / 4))
    {
        return DecodeError::ImageTooLarge;
    }

    outputImage.width = first.w;
    outputImage.height = first.h;
    outputImage.rgba.resize(static_cast<std::size_t>(pixelCount * 4));

    for (std::size_t pixel = 0;
         pixel < static_cast<std::size_t>(pixelCount);
         ++pixel)
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
        std::uint8_t alpha = 255;

        switch (image->numcomps)
        {
            case 1:
                red = green = blue =
                    ConvertComponentSample(image->comps[0], pixel);
                break;

            case 2:
                red = green = blue =
                    ConvertComponentSample(image->comps[0], pixel);
                alpha =
                    ConvertComponentSample(image->comps[1], pixel);
                break;

            case 3:
                red = ConvertComponentSample(image->comps[0], pixel);
                green = ConvertComponentSample(image->comps[1], pixel);
                blue = ConvertComponentSample(image->comps[2], pixel);
                break;

            case 4:
                red = ConvertComponentSample(image->comps[0], pixel);
                green = ConvertComponentSample(image->comps[1], pixel);
                blue = ConvertComponentSample(image->comps[2], pixel);
                alpha = ConvertComponentSample(image->comps[3], pixel);
                break;

            default:
                return DecodeError::UnsupportedComponentCount;
        }

        const std::size_t outputOffset = pixel * 4;

        outputImage.rgba[outputOffset + 0] = red;
        outputImage.rgba[outputOffset + 1] = green;
        outputImage.rgba[outputOffset + 2] = blue;
        outputImage.rgba[outputOffset + 3] = alpha;
    }

    return DecodeError::None;
}

const char* J2CDecoder::ErrorMessage(
    DecodeError error)
{
    switch (error)
    {
        case DecodeError::None:
            return "No error.";

        case DecodeError::EmptyInput:
            return "The JPEG2000 input is empty.";

        case DecodeError::StreamCreationFailed:
            return "The OpenJPEG input stream could not be created.";

        case DecodeError::CodecCreationFailed:
            return "The OpenJPEG decoder could not be created.";

        case DecodeError::HeaderReadFailed:
            return "The JPEG2000 header could not be read.";

        case DecodeError::DecodeFailed:
            return "OpenJPEG could not decode the image.";

        case DecodeError::UnsupportedComponentCount:
            return "The decoded image has an unsupported component count.";

        case DecodeError::UnsupportedComponentLayout:
            return "The image uses unsupported component dimensions or subsampling.";

        case DecodeError::InvalidDimensions:
            return "The decoded image has invalid dimensions.";

        case DecodeError::ImageTooLarge:
            return "The decoded image is too large.";

        case DecodeError::MissingComponentData:
            return "OpenJPEG did not produce usable component data.";
    }

    return "Unknown JPEG2000 decoding error.";
}

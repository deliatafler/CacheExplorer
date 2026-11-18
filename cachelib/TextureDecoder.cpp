#include "TextureDecoder.h"

#include "Bc7Decoder.h"

TextureDecodeResult TextureDecoder::Decode(
    const TexturePayload& payload,
    DecodedImage& outputImage,
    const TextureDecodeOptions& options) const
{
    outputImage = {};

    switch (payload.format)
    {
        case TexturePayloadFormat::Jpeg2000:
        {
            J2CDecoder decoder;
            const DecodeError error = decoder.Decode(
                payload.bytes,
                outputImage,
                options.verboseJpeg2000Errors);

            if (error != DecodeError::None)
            {
                return {
                    TextureDecodeError::Incomplete,
                    J2CDecoder::ErrorMessage(error)};
            }

            return {};
        }

        case TexturePayloadFormat::Bc7:
        {
            Bc7Decoder decoder;
            const Bc7DecodeError error = decoder.Decode(
                payload.bytes,
                outputImage,
                options.maximumDimension);

            if (error != Bc7DecodeError::None)
            {
                return {
                    TextureDecodeError::Incomplete,
                    Bc7Decoder::ErrorMessage(error)};
            }

            return {};
        }
    }

    return {
        TextureDecodeError::UnsupportedFormat,
        TexturePayloadErrorMessage(
            TexturePayloadError::UnsupportedFormat)};
}

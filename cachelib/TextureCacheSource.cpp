#include "TextureCacheSource.h"

const char* TextureCacheOpenErrorMessage(TextureCacheOpenError error)
{
    switch (error)
    {
        case TextureCacheOpenError::None:
            return "No error.";

        case TextureCacheOpenError::FileNotFound:
            return "The texture cache index could not be found.";

        case TextureCacheOpenError::ReadError:
            return "The texture cache could not be read.";

        case TextureCacheOpenError::InvalidFormat:
            return "The selected folder is not a supported texture cache.";

        case TextureCacheOpenError::UnsupportedVersion:
            return "The texture cache version is not supported.";

        case TextureCacheOpenError::CredentialsUnavailable:
            return "The Aurora cache key could not be read from the system credential store.";
    }

    return "Unknown texture cache error.";
}

const char* TexturePayloadErrorMessage(TexturePayloadError error)
{
    switch (error)
    {
        case TexturePayloadError::None:
            return "No error.";

        case TexturePayloadError::SourceNotOpen:
            return "The texture cache source is not open.";

        case TexturePayloadError::EntryNotFound:
            return "The texture is no longer present in the cache.";

        case TexturePayloadError::InvalidEntry:
            return "The texture entry contains invalid metadata.";

        case TexturePayloadError::MissingData:
            return "The cached texture data is no longer available.";

        case TexturePayloadError::ReadError:
            return "The cached texture data could not be read.";

        case TexturePayloadError::UnsupportedFormat:
            return "The cached texture payload format is not supported.";
    }

    return "Unknown texture payload error.";
}

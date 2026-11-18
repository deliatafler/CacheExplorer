#include "AuroraCredentialInternal.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include <openssl/crypto.h>

#include <libsecret/secret.h>

namespace
{
    void UnrefObject(gpointer value)
    {
        g_object_unref(value);
    }

    class HashTableHandle
    {
    public:
        explicit HashTableHandle(GHashTable* value)
            : value_(value)
        {
        }

        ~HashTableHandle()
        {
            if (value_ != nullptr)
            {
                g_hash_table_unref(value_);
            }
        }

        HashTableHandle(const HashTableHandle&) = delete;
        HashTableHandle& operator=(const HashTableHandle&) = delete;

        GHashTable* Get() const
        {
            return value_;
        }

    private:
        GHashTable* value_ = nullptr;
    };

    class ItemListHandle
    {
    public:
        explicit ItemListHandle(GList* value)
            : value_(value)
        {
        }

        ~ItemListHandle()
        {
            g_list_free_full(value_, UnrefObject);
        }

        ItemListHandle(const ItemListHandle&) = delete;
        ItemListHandle& operator=(const ItemListHandle&) = delete;

        GList* Get() const
        {
            return value_;
        }

    private:
        GList* value_ = nullptr;
    };

    class ErrorHandle
    {
    public:
        ~ErrorHandle()
        {
            if (value != nullptr)
            {
                g_error_free(value);
            }
        }

        GError* value = nullptr;
    };
}

AuroraCredentialError LoadAuroraNativeCredential(
    AuroraCacheKey& output)
{
    output = {};

    HashTableHandle attributes(g_hash_table_new(
        g_str_hash,
        g_str_equal));

    if (attributes.Get() == nullptr)
    {
        return AuroraCredentialError::ReadError;
    }

    g_hash_table_insert(
        attributes.Get(),
        const_cast<char*>("service"),
        const_cast<char*>(AuroraCredentialService));
    g_hash_table_insert(
        attributes.Get(),
        const_cast<char*>("username"),
        const_cast<char*>(AuroraCredentialAccount));

    ErrorHandle error;
    ItemListHandle items(secret_service_search_sync(
        nullptr,
        nullptr,
        attributes.Get(),
        static_cast<SecretSearchFlags>(
            SECRET_SEARCH_ALL |
            SECRET_SEARCH_UNLOCK |
            SECRET_SEARCH_LOAD_SECRETS),
        nullptr,
        &error.value));

    if (error.value != nullptr)
    {
        return AuroraCredentialError::ReadError;
    }

    const guint count = g_list_length(items.Get());
    if (count == 0)
    {
        return AuroraCredentialError::NotFound;
    }

    if (count != 1)
    {
        return AuroraCredentialError::ReadError;
    }

    auto* item = SECRET_ITEM(items.Get()->data);
    SecretValue* value = secret_item_get_secret(item);
    if (value == nullptr)
    {
        return AuroraCredentialError::ReadError;
    }

    gsize length = 0;
    const gchar* data = secret_value_get(value, &length);

    if (data == nullptr || length != AuroraCacheKey::Size)
    {
        secret_value_unref(value);
        return AuroraCredentialError::InvalidKey;
    }

    std::array<std::uint8_t, AuroraCacheKey::Size> bytes{};
    std::copy_n(
        reinterpret_cast<const std::uint8_t*>(data),
        bytes.size(),
        bytes.begin());
    secret_value_unref(value);

    output = AuroraCacheKey::FromBytes(bytes);
    OPENSSL_cleanse(bytes.data(), bytes.size());
    return AuroraCredentialError::None;
}

std::filesystem::path DefaultAuroraCacheKeyFile()
{
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path base;

    if (configHome != nullptr && *configHome != '\0')
    {
        base = configHome;
    }
    else
    {
        const char* home = std::getenv("HOME");
        if (home == nullptr || *home == '\0')
        {
            return {};
        }

        base = std::filesystem::path(home) / ".config";
    }

    return base / "auroraviewer" / "cache.key";
}

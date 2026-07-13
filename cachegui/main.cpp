#include "TextureCacheDatabase.h"
#include "TextureExporter.h"
#include "TextureExportState.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#define NOMINMAX
#define UUID WindowsSdkUUID
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <gdiplus.h>
#undef UUID

namespace fs = std::filesystem;
namespace gdip = Gdiplus;

namespace
{
    constexpr int PathEditId = 1001;
    constexpr int BrowseButtonId = 1002;
    constexpr int OpenButtonId = 1003;
    constexpr int ExportButtonId = 1004;
    constexpr int ExportVisibleButtonId = 1013;
    constexpr int ScanVisibleButtonId = 1017;
    constexpr int LikelyCompleteCheckId = 1009;
    constexpr int PreviewableCheckId = 1016;
    constexpr int GalleryCheckId = 1018;
    constexpr int OverwriteCheckId = 1010;
    constexpr int RetryIncompleteCheckId = 1011;
    constexpr int ProgressBarId = 1012;
    constexpr int UuidFilterEditId = 1014;
    constexpr int ClearUuidFilterButtonId = 1015;
    constexpr int TextureListId = 1005;
    constexpr int StatusTextId = 1006;
    constexpr int DetailsTextId = 1007;
    constexpr int PreviewControlId = 1008;
    constexpr int GalleryControlId = 1019;
    constexpr UINT AutoOpenMessage = WM_APP + 1;
    constexpr UINT PreviewCompleteMessage = WM_APP + 2;
    constexpr UINT ExportProgressMessage = WM_APP + 3;
    constexpr UINT ExportCompleteMessage = WM_APP + 4;
    constexpr UINT ScanProgressMessage = WM_APP + 5;
    constexpr UINT ScanCompleteMessage = WM_APP + 6;

    struct PreviewScanRecord
    {
        bool previewable = false;
        std::int32_t imageSize = 0;
        std::int32_t bodySize = 0;
        std::time_t timestamp = 0;
    };

    struct AppState
    {
        HINSTANCE instance = nullptr;
        HWND window = nullptr;
        HWND pathEdit = nullptr;
        HWND browseButton = nullptr;
        HWND openButton = nullptr;
        HWND exportButton = nullptr;
        HWND exportVisibleButton = nullptr;
        HWND scanVisibleButton = nullptr;
        HWND likelyCompleteCheck = nullptr;
        HWND previewableCheck = nullptr;
        HWND galleryCheck = nullptr;
        HWND overwriteCheck = nullptr;
        HWND retryIncompleteCheck = nullptr;
        HWND uuidFilterEdit = nullptr;
        HWND clearUuidFilterButton = nullptr;
        HWND textureList = nullptr;
        HWND galleryControl = nullptr;
        HWND detailsText = nullptr;
        HWND previewControl = nullptr;
        HWND statusText = nullptr;
        HWND progressBar = nullptr;

        TextureCacheDatabase database;
        std::vector<const CacheEntry*> visibleEntries;
        bool likelyCompleteOnly = false;
        bool previewableOnly = false;
        bool galleryMode = false;
        int galleryScrollY = 0;
        std::wstring uuidFilter;
        int sortColumn = 3;
        bool sortAscending = false;
        std::unique_ptr<gdip::Image> previewImage;
        std::string previewImageUuid;
        std::wstring previewText;
        fs::path previewDirectory;
        std::uint64_t previewRequestId = 0;
        std::wstring detailsBase;
        bool exportInProgress = false;
        bool scanInProgress = false;
        bool listUpdateInProgress = false;
        std::unordered_map<std::string, bool> previewableByUuid;
        std::unordered_map<std::string, PreviewScanRecord> previewScanByUuid;
    };

    struct PreviewResult
    {
        std::uint64_t requestId = 0;
        std::string uuid;
        fs::path pngFile;
        std::wstring message;
        bool exported = false;
    };

    struct ExportProgressMessageData
    {
        TextureExportProgress progress;
    };

    struct ExportCompleteMessageData
    {
        BulkExportResults results;
        std::wstring failureMessage;
        bool overwriteExisting = false;
        bool retryIncomplete = false;
    };
    struct ScanProgressMessageData
    {
        std::vector<std::pair<std::string, bool>> statuses;
        std::size_t processed = 0;
        std::size_t total = 0;
        std::size_t previewableCount = 0;
        std::size_t unavailableCount = 0;
        std::size_t skippedKnownCount = 0;
    };

    struct ScanCompleteMessageData
    {
        std::size_t total = 0;
        std::size_t previewableCount = 0;
        std::size_t unavailableCount = 0;
        std::size_t skippedKnownCount = 0;
    };

    void PopulateList(AppState& app);
    void UpdateDetails(AppState& app);
    void ResizeControls(AppState& app, int width, int height);
    std::wstring ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int length = MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            nullptr,
            0);

        std::wstring converted(static_cast<std::size_t>(length), L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            value.data(),
            static_cast<int>(value.size()),
            converted.data(),
            length);

        return converted;
    }

    std::wstring FormatTime(std::time_t timestamp)
    {
        std::tm localTime{};

        if (localtime_s(&localTime, &timestamp) != 0)
        {
            return {};
        }

        std::wostringstream stream;
        stream << std::put_time(&localTime, L"%Y-%m-%d %H:%M:%S");
        return stream.str();
    }

    std::wstring DefaultCachePath()
    {
        wchar_t* localAppData = nullptr;

        if (SHGetKnownFolderPath(
                FOLDERID_LocalAppData,
                0,
                nullptr,
                &localAppData) != S_OK)
        {
            return {};
        }

        fs::path path(localAppData);
        CoTaskMemFree(localAppData);

        path /= L"FirestormOS_x64";
        path /= L"texturecache";
        return path.wstring();
    }

    std::wstring GetText(HWND window)
    {
        const int length = GetWindowTextLengthW(window);
        std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');

        if (length > 0)
        {
            GetWindowTextW(window, text.data(), length + 1);
        }

        text.resize(static_cast<std::size_t>(length));
        return text;
    }

    fs::path ResolveTextureCacheDirectory(const fs::path& suppliedPath)
    {
        if (fs::exists(suppliedPath / L"texture.entries"))
        {
            return suppliedPath;
        }

        const fs::path childTextureCache = suppliedPath / L"texturecache";

        if (fs::exists(childTextureCache / L"texture.entries"))
        {
            return childTextureCache;
        }

        return suppliedPath;
    }

    void SetStatus(AppState& app, const std::wstring& message)
    {
        SetWindowTextW(app.statusText, message.c_str());
    }

    void SetDetails(AppState& app, const std::wstring& message)
    {
        SetWindowTextW(app.detailsText, message.c_str());
    }

    void SetEntryStatus(AppState& app)
    {
        if (app.visibleEntries.empty() && !app.uuidFilter.empty())
        {
            if (app.previewableOnly)
            {
                SetStatus(
                    app,
                    L"No found previews match this search. Turn off Show found to test the search.");
                return;
            }

            SetStatus(app, L"No UUID matches in the current cache.");
            return;
        }

        if (app.previewableOnly && app.visibleEntries.empty())
        {
            SetStatus(
                app,
                L"No found previews yet. Click Find Previews, then enable Show found.");
            return;
        }

        std::wostringstream status;
        status
            << L"Showing "
            << app.visibleEntries.size()
            << L" of "
            << app.database.Entries().size()
            << L" texture entries. Narrow results, then Find Previews for visual browsing.";
        SetStatus(app, status.str());
    }

    bool HasConfirmedPreviewable(const AppState& app)
    {
        for (const auto& item : app.previewableByUuid)
        {
            if (item.second)
            {
                return true;
            }
        }

        return false;
    }

    void UpdatePreviewableControl(AppState& app, bool enabled)
    {
        const bool hasPreviewable = HasConfirmedPreviewable(app);
        EnableWindow(app.previewableCheck, enabled && hasPreviewable);

        if (!hasPreviewable && app.previewableOnly)
        {
            app.previewableOnly = false;
            SendMessageW(app.previewableCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        }
    }

    class ScopedWaitCursor
    {
    public:
        ScopedWaitCursor()
            : previousCursor_(SetCursor(LoadCursorW(nullptr, IDC_WAIT)))
        {
        }

        ~ScopedWaitCursor()
        {
            SetCursor(previousCursor_);
        }

    private:
        HCURSOR previousCursor_ = nullptr;
    };

    void SetExportControlsEnabled(AppState& app, bool enabled)
    {
        EnableWindow(app.browseButton, enabled);
        EnableWindow(app.openButton, enabled);
        EnableWindow(app.exportButton, enabled);
        EnableWindow(app.exportVisibleButton, enabled);
        EnableWindow(app.scanVisibleButton, enabled);
        EnableWindow(app.likelyCompleteCheck, enabled);
        UpdatePreviewableControl(app, enabled);
        EnableWindow(app.galleryCheck, TRUE);
        EnableWindow(app.overwriteCheck, enabled);
        EnableWindow(app.retryIncompleteCheck, enabled);
        EnableWindow(app.uuidFilterEdit, enabled);
        EnableWindow(app.clearUuidFilterButton, enabled);
        EnableWindow(app.textureList, TRUE);
        EnableWindow(app.galleryControl, TRUE);
    }

    void SetScanProgress(
        AppState& app,
        std::size_t processed,
        std::size_t total,
        std::size_t previewableCount,
        std::size_t unavailableCount,
        std::size_t skippedKnownCount)
    {
        SendMessageW(
            app.progressBar,
            PBM_SETRANGE32,
            0,
            static_cast<LPARAM>(std::min<std::size_t>(
                total,
                static_cast<std::size_t>(std::numeric_limits<int>::max()))));
        SendMessageW(
            app.progressBar,
            PBM_SETPOS,
            static_cast<WPARAM>(std::min<std::size_t>(
                processed,
                static_cast<std::size_t>(std::numeric_limits<int>::max()))),
            0);

        std::wostringstream status;
        status
            << L"Scanning "
            << processed
            << L" of "
            << total
            << L" new. Skipped "
            << skippedKnownCount
            << L" cached. Found "
            << previewableCount
            << L", no preview "
            << unavailableCount
            << L".";
        SetStatus(app, status.str());
        UpdateWindow(app.statusText);
        UpdateWindow(app.progressBar);
    }

    void SetExportProgress(AppState& app, const TextureExportProgress& progress)
    {
        SendMessageW(
            app.progressBar,
            PBM_SETRANGE32,
            0,
            static_cast<LPARAM>(std::min<std::size_t>(
                progress.total,
                static_cast<std::size_t>(std::numeric_limits<int>::max()))));
        SendMessageW(
            app.progressBar,
            PBM_SETPOS,
            static_cast<WPARAM>(std::min<std::size_t>(
                progress.processed,
                static_cast<std::size_t>(std::numeric_limits<int>::max()))),
            0);

        std::wostringstream status;
        status
            << L"Exporting "
            << progress.processed
            << L" of "
            << progress.total
            << L". Exported "
            << progress.exported
            << L", skipped "
            << (progress.skippedExisting + progress.skippedKnownIncomplete)
            << L", incomplete "
            << progress.incompleteTextures
            << L", errors "
            << progress.errors
            << L".";
        SetStatus(app, status.str());
        UpdateWindow(app.statusText);
        UpdateWindow(app.progressBar);
    }

    class ScopedWindowRedraw
    {
    public:
        explicit ScopedWindowRedraw(HWND window)
            : window_(window)
        {
            SendMessageW(window_, WM_SETREDRAW, FALSE, 0);
        }

        ~ScopedWindowRedraw()
        {
            Resume();
        }

        void Resume()
        {
            if (!active_)
            {
                return;
            }

            active_ = false;
            SendMessageW(window_, WM_SETREDRAW, TRUE, 0);
            RedrawWindow(
                window_,
                nullptr,
                nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        }

    private:
        HWND window_ = nullptr;
        bool active_ = true;
    };

    fs::path PreviewDirectory(AppState& app)
    {
        if (!app.previewDirectory.empty())
        {
            return app.previewDirectory;
        }

        std::error_code tempError;
        fs::path directory = fs::temp_directory_path(tempError);

        if (tempError)
        {
            return {};
        }

        directory /= L"CacheExplorerPreview";

        std::error_code createError;
        fs::create_directories(directory, createError);

        if (createError)
        {
            return {};
        }

        app.previewDirectory = directory;
        return app.previewDirectory;
    }

    fs::path PreviewIndexFile(AppState& app)
    {
        const fs::path directory = PreviewDirectory(app);

        if (directory.empty())
        {
            return {};
        }

        return directory / L"preview-index.tsv";
    }

    bool EntryMatchesRecord(const CacheEntry& entry, const PreviewScanRecord& record)
    {
        return entry.imageSize == record.imageSize &&
            entry.bodySize == record.bodySize &&
            entry.timestamp == record.timestamp;
    }

    void RecordPreviewStatus(AppState& app, const std::string& uuid, bool previewable)
    {
        const std::optional<UUID> parsedUuid = UUID::FromString(uuid);

        if (!parsedUuid)
        {
            return;
        }

        const CacheEntry* entry = app.database.Find(*parsedUuid);

        if (entry == nullptr)
        {
            return;
        }

        app.previewableByUuid[uuid] = previewable;
        app.previewScanByUuid[uuid] = PreviewScanRecord{
            previewable,
            entry->imageSize,
            entry->bodySize,
            entry->timestamp};
    }

    void SavePreviewIndex(AppState& app)
    {
        const fs::path indexFile = PreviewIndexFile(app);

        if (indexFile.empty())
        {
            return;
        }

        std::ofstream stream(indexFile, std::ios::binary | std::ios::trunc);

        if (!stream)
        {
            return;
        }

        for (const auto& item : app.previewScanByUuid)
        {
            const PreviewScanRecord& record = item.second;
            stream
                << item.first << '\t'
                << (record.previewable ? 1 : 0) << '\t'
                << record.imageSize << '\t'
                << record.bodySize << '\t'
                << static_cast<long long>(record.timestamp) << '\n';
        }
    }

    void LoadPreviewIndex(AppState& app)
    {
        app.previewableByUuid.clear();
        app.previewScanByUuid.clear();

        const fs::path indexFile = PreviewIndexFile(app);

        if (indexFile.empty())
        {
            return;
        }

        std::ifstream stream(indexFile, std::ios::binary);

        if (!stream)
        {
            return;
        }

        std::string uuid;
        int previewable = 0;
        PreviewScanRecord record;
        long long timestamp = 0;

        while (stream >> uuid >> previewable >> record.imageSize >> record.bodySize >> timestamp)
        {
            const std::optional<UUID> parsedUuid = UUID::FromString(uuid);

            if (!parsedUuid)
            {
                continue;
            }

            const CacheEntry* entry = app.database.Find(*parsedUuid);

            if (entry == nullptr)
            {
                continue;
            }

            record.previewable = previewable != 0;
            record.timestamp = static_cast<std::time_t>(timestamp);

            if (!EntryMatchesRecord(*entry, record))
            {
                continue;
            }

            if (record.previewable)
            {
                const fs::path previewFile =
                    PreviewDirectory(app) / (ToWide(uuid) + L".png");
                std::error_code existsError;

                if (!fs::exists(previewFile, existsError) || existsError)
                {
                    continue;
                }
            }

            app.previewableByUuid[uuid] = record.previewable;
            app.previewScanByUuid[uuid] = record;
        }
    }
    void ClearPreview(AppState& app)
    {
        app.previewImage.reset();
        app.previewImageUuid.clear();
        app.previewText.clear();
        InvalidateRect(app.previewControl, nullptr, TRUE);
    }

    void DrawPreview(AppState& app, const DRAWITEMSTRUCT& item)
    {
        RECT bounds = item.rcItem;
        FillRect(item.hDC, &bounds, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

        if (!app.previewImage || app.previewImage->GetLastStatus() != gdip::Ok)
        {
            SetBkMode(item.hDC, TRANSPARENT);
            DrawTextW(
                item.hDC,
                L"No preview available",
                -1,
                &bounds,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return;
        }

        const int availableWidth = bounds.right - bounds.left;
        const int availableHeight = bounds.bottom - bounds.top;
        const UINT imageWidth = app.previewImage->GetWidth();
        const UINT imageHeight = app.previewImage->GetHeight();

        if (availableWidth <= 0 || availableHeight <= 0 ||
            imageWidth == 0 || imageHeight == 0)
        {
            return;
        }

        const double scale = std::min(
            static_cast<double>(availableWidth) / static_cast<double>(imageWidth),
            static_cast<double>(availableHeight) / static_cast<double>(imageHeight));
        const int drawWidth = std::max(1, static_cast<int>(imageWidth * scale));
        const int drawHeight = std::max(1, static_cast<int>(imageHeight * scale));
        const int drawLeft = bounds.left + (availableWidth - drawWidth) / 2;
        const int drawTop = bounds.top + (availableHeight - drawHeight) / 2;

        gdip::Graphics graphics(item.hDC);
        graphics.SetInterpolationMode(gdip::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(
            app.previewImage.get(),
            drawLeft,
            drawTop,
            drawWidth,
            drawHeight);
    }

    bool HasPreviewPng(AppState& app, const CacheEntry& entry)
    {
        const auto status = app.previewableByUuid.find(entry.uuid.ToString());

        if (status == app.previewableByUuid.end() || !status->second)
        {
            return false;
        }

        const fs::path previewDirectory = PreviewDirectory(app);

        if (previewDirectory.empty())
        {
            return false;
        }

        std::error_code existsError;
        return fs::exists(
            previewDirectory / (ToWide(entry.uuid.ToString()) + L".png"),
            existsError) && !existsError;
    }

    int SelectedVisibleIndex(AppState& app)
    {
        const int item = ListView_GetNextItem(
            app.textureList,
            -1,
            LVNI_SELECTED);

        if (item < 0 || static_cast<std::size_t>(item) >= app.visibleEntries.size())
        {
            return -1;
        }

        return item;
    }

    void SelectVisibleIndex(AppState& app, int index)
    {
        if (index < 0 || static_cast<std::size_t>(index) >= app.visibleEntries.size())
        {
            return;
        }

        ListView_SetItemState(
            app.textureList,
            -1,
            0,
            LVIS_SELECTED | LVIS_FOCUSED);
        ListView_SetItemState(
            app.textureList,
            index,
            LVIS_SELECTED | LVIS_FOCUSED,
            LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(app.textureList, index, FALSE);
        UpdateDetails(app);
        InvalidateRect(app.galleryControl, nullptr, TRUE);
    }

    int GalleryColumns(HWND gallery)
    {
        RECT client{};
        GetClientRect(gallery, &client);
        constexpr int cellWidth = 132;
        return std::max(1, static_cast<int>(client.right - client.left) / cellWidth);
    }

    std::vector<std::size_t> GalleryEntryIndices(AppState& app)
    {
        std::vector<std::size_t> indices;
        indices.reserve(app.visibleEntries.size());

        for (std::size_t index = 0; index < app.visibleEntries.size(); ++index)
        {
            const CacheEntry* entry = app.visibleEntries[index];

            if (entry != nullptr && HasPreviewPng(app, *entry))
            {
                indices.push_back(index);
            }
        }

        return indices;
    }

    int GalleryContentHeight(AppState& app, HWND gallery)
    {
        constexpr int cellHeight = 154;
        const int columns = GalleryColumns(gallery);
        const std::vector<std::size_t> galleryEntries = GalleryEntryIndices(app);
        const int rows = static_cast<int>(
            (galleryEntries.size() + static_cast<std::size_t>(columns) - 1)
            / static_cast<std::size_t>(columns));
        return rows * cellHeight;
    }

    void UpdateGalleryScroll(AppState& app)
    {
        if (app.galleryControl == nullptr)
        {
            return;
        }

        RECT client{};
        GetClientRect(app.galleryControl, &client);
        const int page = std::max(1, static_cast<int>(client.bottom - client.top));
        const int contentHeight = GalleryContentHeight(app, app.galleryControl);
        const int maxScroll = std::max(0, contentHeight - page);
        app.galleryScrollY = std::clamp(app.galleryScrollY, 0, maxScroll);

        SCROLLINFO scroll{};
        scroll.cbSize = sizeof(scroll);
        scroll.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        scroll.nMin = 0;
        scroll.nMax = std::max(0, contentHeight - 1);
        scroll.nPage = static_cast<UINT>(page);
        scroll.nPos = app.galleryScrollY;
        SetScrollInfo(app.galleryControl, SB_VERT, &scroll, TRUE);
    }

    void DrawGallery(AppState& app, HWND gallery, HDC dc)
    {
        RECT client{};
        GetClientRect(gallery, &client);
        FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

        constexpr int cellWidth = 132;
        constexpr int cellHeight = 154;
        constexpr int margin = 10;
        constexpr int thumbSize = 104;
        const int columns = GalleryColumns(gallery);
        const int selectedIndex = SelectedVisibleIndex(app);
        const std::vector<std::size_t> galleryEntries = GalleryEntryIndices(app);
        gdip::Graphics graphics(dc);
        graphics.SetInterpolationMode(gdip::InterpolationModeHighQualityBicubic);

        if (galleryEntries.empty())
        {
            SetBkMode(dc, TRANSPARENT);
            DrawTextW(
                dc,
                L"No found previews in the current results. Narrow results, then Find Previews.",
                -1,
                &client,
                DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            return;
        }

        for (std::size_t cellIndex = 0; cellIndex < galleryEntries.size(); ++cellIndex)
        {
            const std::size_t index = galleryEntries[cellIndex];
            const int row = static_cast<int>(cellIndex) / columns;
            const int column = static_cast<int>(cellIndex) % columns;
            const int left = margin + (column * cellWidth);
            const int top = margin + (row * cellHeight) - app.galleryScrollY;

            if (top > client.bottom || top + cellHeight < client.top)
            {
                continue;
            }

            RECT card{left, top, left + cellWidth - margin, top + cellHeight - margin};
            HBRUSH background = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            FillRect(dc, &card, background);
            FrameRect(dc, &card, reinterpret_cast<HBRUSH>(COLOR_3DSHADOW + 1));

            if (static_cast<int>(index) == selectedIndex)
            {
                HPEN pen = CreatePen(PS_SOLID, 2, GetSysColor(COLOR_HIGHLIGHT));
                HGDIOBJ oldPen = SelectObject(dc, pen);
                HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
                Rectangle(dc, card.left + 1, card.top + 1, card.right - 1, card.bottom - 1);
                SelectObject(dc, oldBrush);
                SelectObject(dc, oldPen);
                DeleteObject(pen);
            }

            const CacheEntry& entry = *app.visibleEntries[index];
            RECT thumb{left + 8, top + 8, left + 8 + thumbSize, top + 8 + thumbSize};
            FillRect(dc, &thumb, reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1));

            if (HasPreviewPng(app, entry))
            {
                const fs::path file = PreviewDirectory(app) / (ToWide(entry.uuid.ToString()) + L".png");
                gdip::Image image(file.wstring().c_str());

                if (image.GetLastStatus() == gdip::Ok && image.GetWidth() > 0 && image.GetHeight() > 0)
                {
                    const double scale = std::min(
                        static_cast<double>(thumb.right - thumb.left) / static_cast<double>(image.GetWidth()),
                        static_cast<double>(thumb.bottom - thumb.top) / static_cast<double>(image.GetHeight()));
                    const int drawWidth = std::max(1, static_cast<int>(image.GetWidth() * scale));
                    const int drawHeight = std::max(1, static_cast<int>(image.GetHeight() * scale));
                    const int drawLeft = thumb.left + ((thumb.right - thumb.left) - drawWidth) / 2;
                    const int drawTop = thumb.top + ((thumb.bottom - thumb.top) - drawHeight) / 2;
                    graphics.DrawImage(&image, drawLeft, drawTop, drawWidth, drawHeight);
                }
            }
            else
            {
                SetBkMode(dc, TRANSPARENT);
                DrawTextW(dc, L"No preview", -1, &thumb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            RECT label{card.left + 5, thumb.bottom + 8, card.right - 5, card.bottom - 5};
            const std::wstring uuid = ToWide(entry.uuid.ToString()).substr(0, 8);
            SetBkMode(dc, TRANSPARENT);
            DrawTextW(dc, uuid.c_str(), -1, &label, DT_CENTER | DT_TOP | DT_SINGLELINE);
        }
    }

    int GalleryHitTest(AppState& app, HWND gallery, int x, int y)
    {
        constexpr int cellWidth = 132;
        constexpr int cellHeight = 154;
        constexpr int margin = 10;
        const int columns = GalleryColumns(gallery);
        const int column = std::max(0, (x - margin) / cellWidth);
        const int row = std::max(0, (y + app.galleryScrollY - margin) / cellHeight);

        if (column >= columns)
        {
            return -1;
        }

        const std::vector<std::size_t> galleryEntries = GalleryEntryIndices(app);
        const std::size_t cellIndex = static_cast<std::size_t>(row * columns + column);
        return cellIndex < galleryEntries.size() ? static_cast<int>(galleryEntries[cellIndex]) : -1;
    }

    LRESULT CALLBACK GalleryProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        HWND parent = GetParent(window);
        AppState* app = parent == nullptr
            ? nullptr
            : reinterpret_cast<AppState*>(GetWindowLongPtrW(parent, GWLP_USERDATA));

        switch (message)
        {
            case WM_PAINT:
            {
                PAINTSTRUCT paint{};
                HDC dc = BeginPaint(window, &paint);

                if (app != nullptr)
                {
                    DrawGallery(*app, window, dc);
                }

                EndPaint(window, &paint);
                return 0;
            }

            case WM_SIZE:
                if (app != nullptr)
                {
                    UpdateGalleryScroll(*app);
                }
                return 0;

            case WM_MOUSEWHEEL:
                if (app != nullptr)
                {
                    const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    app->galleryScrollY -= (delta / WHEEL_DELTA) * 48;
                    UpdateGalleryScroll(*app);
                    InvalidateRect(window, nullptr, TRUE);
                }
                return 0;

            case WM_VSCROLL:
                if (app != nullptr)
                {
                    SCROLLINFO scroll{};
                    scroll.cbSize = sizeof(scroll);
                    scroll.fMask = SIF_ALL;
                    GetScrollInfo(window, SB_VERT, &scroll);

                    int position = app->galleryScrollY;
                    switch (LOWORD(wParam))
                    {
                        case SB_LINEUP: position -= 24; break;
                        case SB_LINEDOWN: position += 24; break;
                        case SB_PAGEUP: position -= static_cast<int>(scroll.nPage); break;
                        case SB_PAGEDOWN: position += static_cast<int>(scroll.nPage); break;
                        case SB_THUMBTRACK: position = scroll.nTrackPos; break;
                    }

                    app->galleryScrollY = position;
                    UpdateGalleryScroll(*app);
                    InvalidateRect(window, nullptr, TRUE);
                }
                return 0;

            case WM_LBUTTONDOWN:
                if (app != nullptr)
                {
                    const int index = GalleryHitTest(
                        *app,
                        window,
                        GET_X_LPARAM(lParam),
                        GET_Y_LPARAM(lParam));
                    SelectVisibleIndex(*app, index);
                }
                return 0;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    void StartPreview(AppState& app, const CacheEntry& entry)
    {
        ClearPreview(app);

        const fs::path previewDirectory = PreviewDirectory(app);

        if (previewDirectory.empty())
        {
            SetDetails(app, app.detailsBase + L"\r\n\r\nPreview\r\nTemporary preview directory is unavailable.");
            return;
        }

        const std::uint64_t requestId = ++app.previewRequestId;
        const fs::path previewFile =
            previewDirectory / (ToWide(entry.uuid.ToString()) + L".png");
        TextureCacheDatabase database = app.database;
        CacheEntry previewEntry = entry;
        HWND window = app.window;

        SetDetails(app, app.detailsBase + L"\r\n\r\nPreview\r\nLoading...");

        std::thread(
            [database = std::move(database), previewEntry, previewFile, requestId, window]() mutable
            {
                auto resultMessage = std::make_unique<PreviewResult>();
                resultMessage->requestId = requestId;
                resultMessage->uuid = previewEntry.uuid.ToString();
                resultMessage->pngFile = previewFile;

                TextureExportOptions options;
                options.overwriteExisting = true;
                options.verboseDecoderErrors = false;
                options.maximumStoredMessages = 0;

                TextureExporter exporter;
                const TexturePngExportResult result = exporter.ExportPngEntry(
                    database,
                    previewEntry,
                    previewFile,
                    options);

                resultMessage->exported = result.status == TexturePngExportStatus::Exported;
                resultMessage->message = ToWide(result.message);

                auto* rawResult = resultMessage.release();
                if (!PostMessageW(
                        window,
                        PreviewCompleteMessage,
                        0,
                        reinterpret_cast<LPARAM>(rawResult)))
                {
                    delete rawResult;
                }
            }).detach();
    }

    void ApplyPreviewResult(AppState& app, std::unique_ptr<PreviewResult> result)
    {
        if (!result)
        {
            return;
        }

        const bool isCurrentPreview = result->requestId == app.previewRequestId;

        if (!result->exported)
        {
            RecordPreviewStatus(app, result->uuid, false);
            SavePreviewIndex(app);

            if (isCurrentPreview)
            {
                SetDetails(app, app.detailsBase + L"\r\n\r\nPreview\r\n" + result->message);
                SetStatus(app, L"Preview unavailable: " + result->message);
            }

            return;
        }

        auto image = std::make_unique<gdip::Image>(result->pngFile.wstring().c_str());

        if (image->GetLastStatus() != gdip::Ok)
        {
            RecordPreviewStatus(app, result->uuid, false);
            SavePreviewIndex(app);

            if (isCurrentPreview)
            {
                SetDetails(app, app.detailsBase + L"\r\n\r\nPreview\r\nPNG was exported but could not be loaded.");
            }

            return;
        }

        RecordPreviewStatus(app, result->uuid, true);
        SavePreviewIndex(app);
        if (!app.exportInProgress && !app.scanInProgress)
        {
            UpdatePreviewableControl(app, true);
        }

        if (!isCurrentPreview)
        {
            if (app.previewableOnly)
            {
                PopulateList(app);
                SetEntryStatus(app);
            }

            return;
        }

        std::wostringstream previewText;
        previewText
            << L"Preview\r\n"
            << image->GetWidth()
            << L" x "
            << image->GetHeight();

        app.previewImageUuid = result->uuid;
        app.previewText = previewText.str();
        app.previewImage = std::move(image);
        SetDetails(app, app.detailsBase + L"\r\n\r\n" + app.previewText);
        SetStatus(app, L"Preview ready: " + ToWide(result->uuid));
        InvalidateRect(app.previewControl, nullptr, TRUE);
    }

    void AddColumn(HWND list, int index, int width, const wchar_t* title)
    {
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.pszText = const_cast<LPWSTR>(title);
        column.cx = width;
        column.iSubItem = index;
        ListView_InsertColumn(list, index, &column);
    }

    void SetItemText(
        HWND list,
        int item,
        int subItem,
        const std::wstring& text)
    {
        ListView_SetItemText(
            list,
            item,
            subItem,
            const_cast<LPWSTR>(text.c_str()));
    }

    void ConfigureListView(HWND list)
    {
        ListView_SetExtendedListViewStyle(
            list,
            LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        AddColumn(list, 0, 300, L"UUID");
        AddColumn(list, 1, 84, L"Image");
        AddColumn(list, 2, 84, L"Body");
        AddColumn(list, 3, 150, L"Last Used");
    }

    std::optional<fs::path> PickFolder(HWND owner, const wchar_t* title)
    {
        IFileDialog* dialog = nullptr;

        if (CoCreateInstance(
                CLSID_FileOpenDialog,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&dialog)) != S_OK)
        {
            return std::nullopt;
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dialog->SetTitle(title);

        std::optional<fs::path> selectedPath;

        if (dialog->Show(owner) == S_OK)
        {
            IShellItem* item = nullptr;

            if (dialog->GetResult(&item) == S_OK)
            {
                PWSTR filePath = nullptr;

                if (item->GetDisplayName(SIGDN_FILESYSPATH, &filePath) == S_OK)
                {
                    selectedPath = fs::path(filePath);
                    CoTaskMemFree(filePath);
                }

                item->Release();
            }
        }

        dialog->Release();
        return selectedPath;
    }

    void UpdateDetails(AppState& app)
    {
        const int item = ListView_GetNextItem(
            app.textureList,
            -1,
            LVNI_SELECTED);

        if (item < 0)
        {
            ++app.previewRequestId;
            app.detailsBase = L"No texture selected.";
            SetDetails(app, app.detailsBase);
            ClearPreview(app);
            return;
        }

        const std::size_t index = static_cast<std::size_t>(item);

        if (index >= app.visibleEntries.size())
        {
            ++app.previewRequestId;
            app.detailsBase = L"No texture selected.";
            SetDetails(app, app.detailsBase);
            ClearPreview(app);
            return;
        }

        const CacheEntry& entry = *app.visibleEntries[index];

        std::wostringstream details;
        details
            << L"Selected texture\r\n"
            << L"\r\n"
            << L"UUID\r\n"
            << ToWide(entry.uuid.ToString())
            << L"\r\n\r\n"
            << L"Cache index\r\n"
            << entry.cacheIndex
            << L"\r\n\r\n"
            << L"Image bytes\r\n"
            << entry.imageSize
            << L"\r\n\r\n"
            << L"Body bytes\r\n"
            << entry.bodySize
            << L"\r\n\r\n"
            << L"Last used\r\n"
            << FormatTime(entry.timestamp);

        app.detailsBase = details.str();

        if (app.previewImage &&
            app.previewImage->GetLastStatus() == gdip::Ok &&
            app.previewImageUuid == entry.uuid.ToString())
        {
            SetDetails(app, app.detailsBase + L"\r\n\r\n" + app.previewText);
            InvalidateRect(app.previewControl, nullptr, TRUE);
            return;
        }

        StartPreview(app, entry);
    }

    bool IsLikelyComplete(const CacheEntry& entry)
    {
        return entry.imageSize > 0 &&
            entry.bodySize > 0 &&
            entry.imageSize > entry.bodySize;
    }

    bool IsConfirmedPreviewable(AppState& app, const CacheEntry& entry)
    {
        const auto status = app.previewableByUuid.find(entry.uuid.ToString());
        return status != app.previewableByUuid.end() && status->second;
    }

    bool MatchesUuidFilter(const CacheEntry& entry, const std::wstring& filter)
    {
        if (filter.empty())
        {
            return true;
        }

        std::wstring uuid = ToWide(entry.uuid.ToString());
        std::wstring needle = filter;

        std::transform(
            uuid.begin(),
            uuid.end(),
            uuid.begin(),
            [](wchar_t value) { return static_cast<wchar_t>(towlower(value)); });
        std::transform(
            needle.begin(),
            needle.end(),
            needle.begin(),
            [](wchar_t value) { return static_cast<wchar_t>(towlower(value)); });

        return uuid.find(needle) != std::wstring::npos;
    }

    std::wstring SelectedUuidText(HWND list)
    {
        const int item = ListView_GetNextItem(list, -1, LVNI_SELECTED);

        if (item < 0)
        {
            return {};
        }

        wchar_t buffer[64]{};
        ListView_GetItemText(
            list,
            item,
            0,
            buffer,
            static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
        return buffer;
    }

    int CompareEntriesByColumn(
        const CacheEntry& left,
        const CacheEntry& right,
        int column)
    {
        switch (column)
        {
            case 1:
                if (left.imageSize != right.imageSize)
                {
                    return left.imageSize < right.imageSize ? -1 : 1;
                }
                break;

            case 2:
                if (left.bodySize != right.bodySize)
                {
                    return left.bodySize < right.bodySize ? -1 : 1;
                }
                break;

            case 3:
                if (left.timestamp != right.timestamp)
                {
                    return left.timestamp < right.timestamp ? -1 : 1;
                }
                break;

            case 0:
            default:
            {
                const std::string leftUuid = left.uuid.ToString();
                const std::string rightUuid = right.uuid.ToString();

                if (leftUuid != rightUuid)
                {
                    return leftUuid < rightUuid ? -1 : 1;
                }
                break;
            }
        }

        if (left.cacheIndex != right.cacheIndex)
        {
            return left.cacheIndex < right.cacheIndex ? -1 : 1;
        }

        return 0;
    }

    void SortVisibleEntries(AppState& app)
    {
        std::sort(
            app.visibleEntries.begin(),
            app.visibleEntries.end(),
            [&app](const CacheEntry* left, const CacheEntry* right)
            {
                const int comparison =
                    CompareEntriesByColumn(*left, *right, app.sortColumn);

                if (comparison == 0)
                {
                    return false;
                }

                return app.sortAscending ? comparison < 0 : comparison > 0;
            });
    }

    void UpdateSortIndicators(AppState& app)
    {
        HWND header = ListView_GetHeader(app.textureList);

        if (header == nullptr)
        {
            return;
        }

        for (int column = 0; column < 4; ++column)
        {
            HDITEMW item{};
            item.mask = HDI_FORMAT;

            if (!Header_GetItem(header, column, &item))
            {
                continue;
            }

            item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);

            if (column == app.sortColumn)
            {
                item.fmt |= app.sortAscending ? HDF_SORTUP : HDF_SORTDOWN;
            }

            Header_SetItem(header, column, &item);
        }
    }

    void PopulateList(AppState& app)
    {
        const std::wstring selectedUuid = SelectedUuidText(app.textureList);
        app.listUpdateInProgress = true;
        ScopedWindowRedraw listRedraw(app.textureList);
        ListView_DeleteAllItems(app.textureList);
        app.visibleEntries.clear();

        const std::vector<CacheEntry>& entries = app.database.Entries();
        app.visibleEntries.reserve(entries.size());

        for (const CacheEntry& entry : entries)
        {
            if (app.likelyCompleteOnly && !IsLikelyComplete(entry))
            {
                continue;
            }

            if (!MatchesUuidFilter(entry, app.uuidFilter))
            {
                continue;
            }

            if (app.previewableOnly && !IsConfirmedPreviewable(app, entry))
            {
                continue;
            }

            app.visibleEntries.push_back(&entry);
        }

        SortVisibleEntries(app);
        UpdateSortIndicators(app);

        int selectedIndex = -1;

        for (std::size_t index = 0; index < app.visibleEntries.size(); ++index)
        {
            const CacheEntry& entry = *app.visibleEntries[index];
            const int itemIndex = static_cast<int>(index);
            const std::wstring uuid = ToWide(entry.uuid.ToString());

            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = itemIndex;
            item.iSubItem = 0;
            item.pszText = const_cast<LPWSTR>(uuid.c_str());
            ListView_InsertItem(app.textureList, &item);

            SetItemText(app.textureList, itemIndex, 1, std::to_wstring(entry.imageSize));
            SetItemText(app.textureList, itemIndex, 2, std::to_wstring(entry.bodySize));
            SetItemText(app.textureList, itemIndex, 3, FormatTime(entry.timestamp));

            if (!selectedUuid.empty() && uuid == selectedUuid)
            {
                selectedIndex = itemIndex;
            }
        }

        if (selectedIndex < 0 && !app.visibleEntries.empty())
        {
            selectedIndex = 0;
        }

        if (selectedIndex >= 0)
        {
            ListView_SetItemState(
                app.textureList,
                selectedIndex,
                LVIS_SELECTED | LVIS_FOCUSED,
                LVIS_SELECTED | LVIS_FOCUSED);
        }

        listRedraw.Resume();
        app.listUpdateInProgress = false;
        UpdateDetails(app);
        UpdateGalleryScroll(app);
        InvalidateRect(app.galleryControl, nullptr, TRUE);
    }

    bool OpenCache(AppState& app)
    {
        ++app.previewRequestId;
        ClearPreview(app);

        const fs::path cachePath =
            ResolveTextureCacheDirectory(GetText(app.pathEdit));
        TextureCacheDatabase database;
        const CacheError result = database.Open(cachePath);

        if (result != CacheError::None)
        {
            app.detailsBase = L"No cache loaded.";
            SetStatus(app, L"Could not open texture.entries in the selected folder.");
            SetDetails(app, app.detailsBase);
            return false;
        }

        app.database = std::move(database);
        LoadPreviewIndex(app);
        app.previewableOnly = false;
        SendMessageW(app.previewableCheck, BM_SETCHECK, BST_UNCHECKED, 0);
        UpdatePreviewableControl(app, true);
        SetWindowTextW(app.pathEdit, app.database.CacheDirectory().wstring().c_str());
        PopulateList(app);

        SetEntryStatus(app);
        return true;
    }

    std::vector<const CacheEntry*> SelectedEntries(AppState& app)
    {
        std::vector<const CacheEntry*> selectedEntries;
        int item = -1;

        while (true)
        {
            item = ListView_GetNextItem(app.textureList, item, LVNI_SELECTED);

            if (item < 0)
            {
                break;
            }

            const std::size_t index = static_cast<std::size_t>(item);

            if (index < app.visibleEntries.size())
            {
                selectedEntries.push_back(app.visibleEntries[index]);
            }
        }

        return selectedEntries;
    }

    void ExportEntries(
        AppState& app,
        const std::vector<const CacheEntry*>& entries,
        const wchar_t* emptyMessage)
    {
        if (entries.empty())
        {
            SetStatus(app, emptyMessage);
            return;
        }

        if (app.exportInProgress || app.scanInProgress)
        {
            SetStatus(app, L"Another background operation is already running.");
            return;
        }

        const std::optional<fs::path> outputDirectory =
            PickFolder(app.window, L"Choose export directory");

        if (!outputDirectory)
        {
            return;
        }

        TextureExportOptions options;
        options.overwriteExisting =
            SendMessageW(app.overwriteCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        options.skipKnownIncomplete =
            SendMessageW(app.retryIncompleteCheck, BM_GETCHECK, 0, 0) != BST_CHECKED;
        options.maximumStoredMessages = 10;

        std::vector<CacheEntry> entryCopies;
        entryCopies.reserve(entries.size());

        for (const CacheEntry* entry : entries)
        {
            if (entry != nullptr)
            {
                entryCopies.push_back(*entry);
            }
        }

        if (entryCopies.empty())
        {
            SetStatus(app, emptyMessage);
            return;
        }

        app.exportInProgress = true;
        ShowWindow(app.progressBar, SW_SHOW);
        SetExportControlsEnabled(app, false);
        SetExportProgress(app, TextureExportProgress{0, entryCopies.size()});

        TextureCacheDatabase database = app.database;
        HWND window = app.window;
        const fs::path stateFile =
            *outputDirectory / L".cacheexplorer-export-state.tsv";

        std::thread(
            [database = std::move(database),
             entryCopies = std::move(entryCopies),
             outputDirectory = *outputDirectory,
             stateFile,
             options,
             window]() mutable
            {
                auto complete = std::make_unique<ExportCompleteMessageData>();
                complete->overwriteExisting = options.overwriteExisting;
                complete->retryIncomplete = !options.skipKnownIncomplete;

                TextureExportState exportState;
                std::string stateError;

                if (!exportState.Load(stateFile, stateError))
                {
                    complete->failureMessage = L"Could not load export state file.";
                    auto* rawComplete = complete.release();
                    if (!PostMessageW(
                            window,
                            ExportCompleteMessage,
                            0,
                            reinterpret_cast<LPARAM>(rawComplete)))
                    {
                        delete rawComplete;
                    }
                    return;
                }

                TextureExportOptions workerOptions = options;
                workerOptions.exportState = &exportState;

                std::vector<const CacheEntry*> exportEntries;
                exportEntries.reserve(entryCopies.size());

                for (const CacheEntry& entry : entryCopies)
                {
                    exportEntries.push_back(&entry);
                }

                TextureExporter exporter;
                complete->results = exporter.ExportPngEntries(
                    database,
                    exportEntries,
                    outputDirectory,
                    workerOptions,
                    [window](const TextureExportProgress& progress)
                    {
                        auto progressMessage = std::make_unique<ExportProgressMessageData>();
                        progressMessage->progress = progress;
                        auto* rawProgress = progressMessage.release();

                        if (!PostMessageW(
                                window,
                                ExportProgressMessage,
                                0,
                                reinterpret_cast<LPARAM>(rawProgress)))
                        {
                            delete rawProgress;
                        }
                    });

                if (!exportState.Save(stateFile, stateError))
                {
                    complete->failureMessage = L"Export finished, but state could not be saved.";
                }

                auto* rawComplete = complete.release();
                if (!PostMessageW(
                        window,
                        ExportCompleteMessage,
                        0,
                        reinterpret_cast<LPARAM>(rawComplete)))
                {
                    delete rawComplete;
                }
            }).detach();
    }

    void ExportSelected(AppState& app)
    {
        ExportEntries(
            app,
            SelectedEntries(app),
            L"Select one or more textures to export.");
    }

    void ExportVisible(AppState& app)
    {
        ExportEntries(
            app,
            app.visibleEntries,
            L"No visible textures to export.");
    }

    void ApplyExportComplete(
        AppState& app,
        std::unique_ptr<ExportCompleteMessageData> complete)
    {
        app.exportInProgress = false;
        SetExportControlsEnabled(app, true);
        ShowWindow(app.progressBar, SW_HIDE);

        if (!complete)
        {
            SetStatus(app, L"Export finished, but the result was unavailable.");
            return;
        }

        if (!complete->failureMessage.empty())
        {
            SetStatus(app, complete->failureMessage);
            return;
        }

        const BulkExportResults& results = complete->results;
        std::wostringstream status;
        status
            << L"Exported " << results.exported
            << L", skipped existing " << results.skippedExisting
            << L", skipped incomplete " << results.skippedKnownIncomplete
            << L", incomplete " << results.incompleteTextures
            << L", errors " << results.ErrorCount()
            << L". Overwrite: "
            << (complete->overwriteExisting ? L"on" : L"off")
            << L", retry incomplete: "
            << (complete->retryIncomplete ? L"on" : L"off")
            << L".";
        SetStatus(app, status.str());
    }

    void StartScanVisible(AppState& app)
    {
        if (app.visibleEntries.empty())
        {
            SetStatus(app, L"No visible textures to scan.");
            return;
        }

        if (app.exportInProgress || app.scanInProgress)
        {
            SetStatus(app, L"Another background operation is already running.");
            return;
        }

        const fs::path previewDirectory = PreviewDirectory(app);

        if (previewDirectory.empty())
        {
            SetStatus(app, L"Temporary preview directory is unavailable.");
            return;
        }

        std::vector<CacheEntry> entryCopies;
        std::size_t skippedKnownCount = 0;
        entryCopies.reserve(app.visibleEntries.size());

        for (const CacheEntry* entry : app.visibleEntries)
        {
            if (entry == nullptr)
            {
                continue;
            }

            const std::string uuid = entry->uuid.ToString();
            const auto knownStatus = app.previewableByUuid.find(uuid);

            if (knownStatus != app.previewableByUuid.end())
            {
                if (!knownStatus->second || HasPreviewPng(app, *entry))
                {
                    ++skippedKnownCount;
                    continue;
                }
            }

            entryCopies.push_back(*entry);
        }

        if (entryCopies.empty())
        {
            SetStatus(app, L"No visible textures to scan.");
            return;
        }

        constexpr std::size_t LargeScanThreshold = 500;

        if (entryCopies.size() > LargeScanThreshold)
        {
            std::wostringstream message;
            message
                << L"Find previews for "
                << entryCopies.size()
                << L" visible textures?\n\n"
                << L"This attempts JPEG2000 decode work for each visible texture and can take a while. "
                << L"Use search or Has data first if you want a smaller scan.";

            if (MessageBoxW(
                    app.window,
                    message.str().c_str(),
                    L"Find Previews",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
            {
                SetStatus(app, L"Find Previews canceled. Narrow filters first for a shorter scan.");
                return;
            }
        }

        app.scanInProgress = true;
        ShowWindow(app.progressBar, SW_SHOW);
        SetExportControlsEnabled(app, false);
        SetStatus(app, L"Finding previewable textures. This can take a while for large visible sets...");
        UpdateWindow(app.statusText);
        SetScanProgress(app, 0, entryCopies.size(), 0, 0, skippedKnownCount);

        TextureCacheDatabase database = app.database;
        HWND window = app.window;

        std::thread(
            [database = std::move(database),
             entryCopies = std::move(entryCopies),
             previewDirectory,
             skippedKnownCount,
             window]() mutable
            {
                constexpr std::size_t PreviewWorkerCount = 3;
                std::atomic<std::size_t> nextEntry{0};
                std::size_t processed = 0;
                std::size_t previewableCount = 0;
                std::size_t unavailableCount = 0;
                std::vector<std::pair<std::string, bool>> pendingStatuses;
                std::mutex progressMutex;

                auto postProgressLocked = [&]()
                {
                    auto progress = std::make_unique<ScanProgressMessageData>();
                    progress->statuses = std::move(pendingStatuses);
                    progress->processed = processed;
                    progress->total = entryCopies.size();
                    progress->previewableCount = previewableCount;
                    progress->unavailableCount = unavailableCount;
                    progress->skippedKnownCount = skippedKnownCount;
                    pendingStatuses.clear();
                    auto* rawProgress = progress.release();

                    if (!PostMessageW(
                            window,
                            ScanProgressMessage,
                            0,
                            reinterpret_cast<LPARAM>(rawProgress)))
                    {
                        delete rawProgress;
                    }
                };

                auto worker = [&]()
                {
                    TextureExporter exporter;

                    while (true)
                    {
                        const std::size_t entryIndex = nextEntry.fetch_add(1);

                        if (entryIndex >= entryCopies.size())
                        {
                            return;
                        }

                        const CacheEntry& entry = entryCopies[entryIndex];
                        const std::string uuid = entry.uuid.ToString();
                        const fs::path previewFile =
                            previewDirectory / (ToWide(uuid) + L".png");

                        std::error_code existsError;
                        bool previewable = fs::exists(previewFile, existsError) && !existsError;

                        if (!previewable)
                        {
                            TextureExportOptions options;
                            options.overwriteExisting = false;
                            options.verboseDecoderErrors = false;
                            options.maximumStoredMessages = 0;

                            const TexturePngExportResult result = exporter.ExportPngEntry(
                                database,
                                entry,
                                previewFile,
                                options);
                            previewable = result.Succeeded();
                        }

                        std::lock_guard<std::mutex> lock(progressMutex);
                        ++processed;

                        if (previewable)
                        {
                            ++previewableCount;
                        }
                        else
                        {
                            ++unavailableCount;
                        }

                        pendingStatuses.emplace_back(uuid, previewable);

                        if (processed == entryCopies.size() || pendingStatuses.size() >= 50)
                        {
                            postProgressLocked();
                        }
                    }
                };

                std::vector<std::thread> workers;
                const std::size_t workerCount = std::min(PreviewWorkerCount, entryCopies.size());
                workers.reserve(workerCount);

                for (std::size_t i = 0; i < workerCount; ++i)
                {
                    workers.emplace_back(worker);
                }

                for (std::thread& thread : workers)
                {
                    thread.join();
                }

                {
                    std::lock_guard<std::mutex> lock(progressMutex);

                    if (!pendingStatuses.empty())
                    {
                        postProgressLocked();
                    }
                }

                auto complete = std::make_unique<ScanCompleteMessageData>();
                complete->total = entryCopies.size();
                complete->previewableCount = previewableCount;
                complete->unavailableCount = unavailableCount;
                complete->skippedKnownCount = skippedKnownCount;
                auto* rawComplete = complete.release();

                if (!PostMessageW(
                        window,
                        ScanCompleteMessage,
                        0,
                        reinterpret_cast<LPARAM>(rawComplete)))
                {
                    delete rawComplete;
                }
            }).detach();
    }

    void ApplyScanProgress(AppState& app, std::unique_ptr<ScanProgressMessageData> progress)
    {
        if (!progress || !app.scanInProgress)
        {
            return;
        }

        bool foundPreviewable = false;

        for (const auto& status : progress->statuses)
        {
            RecordPreviewStatus(app, status.first, status.second);
            foundPreviewable = foundPreviewable || status.second;
        }

        if (!progress->statuses.empty())
        {
            SavePreviewIndex(app);
        }

        SetScanProgress(
            app,
            progress->processed,
            progress->total,
            progress->previewableCount,
            progress->unavailableCount,
            progress->skippedKnownCount);

        if (app.galleryMode && foundPreviewable)
        {
            UpdateGalleryScroll(app);
            InvalidateRect(app.galleryControl, nullptr, TRUE);
        }
    }

    void ApplyScanComplete(AppState& app, std::unique_ptr<ScanCompleteMessageData> complete)
    {
        app.scanInProgress = false;
        SetExportControlsEnabled(app, true);
        ShowWindow(app.progressBar, SW_HIDE);

        if (app.previewableOnly)
        {
            PopulateList(app);
        }

        UpdateGalleryScroll(app);
        InvalidateRect(app.galleryControl, nullptr, TRUE);

        if (!complete)
        {
            SetStatus(app, L"Scan finished, but the result was unavailable.");
            return;
        }

        std::wostringstream status;
        status
            << L"Checked "
            << (complete->total + complete->skippedKnownCount)
            << L" textures. Scanned "
            << complete->total
            << L" new, skipped "
            << complete->skippedKnownCount
            << L" cached. Found "
            << complete->previewableCount
            << L", no preview "
            << complete->unavailableCount
            << L".";
        SetStatus(app, status.str());
    }

    void ResizeControls(AppState& app, int width, int height)
    {
        constexpr int margin = 12;
        constexpr int rowHeight = 28;
        constexpr int buttonWidth = 90;
        constexpr int exportButtonWidth = 112;
        constexpr int scanButtonWidth = 112;
        constexpr int checkWidth = 88;
        constexpr int previewableWidth = 98;
        constexpr int galleryWidth = 98;
        constexpr int smallCheckWidth = 88;
        constexpr int retryCheckWidth = 98;
        constexpr int uuidFilterWidth = 210;
        constexpr int clearFilterWidth = 52;
        constexpr int statusHeight = 24;
        constexpr int progressWidth = 220;
        constexpr int gap = 8;
        constexpr int detailsWidth = 320;
        constexpr int detailsHeight = 270;

        const int contentWidth = width - (margin * 2);
        const int pathWidth = contentWidth - (buttonWidth * 2) - (gap * 2);
        const int listTop = margin + (rowHeight * 2) + (gap * 2);
        const int listHeight = height - listTop - statusHeight - (margin * 2);
        const int listWidth = contentWidth - detailsWidth - gap;
        const int detailsLeft = margin + listWidth + gap;

        MoveWindow(app.pathEdit, margin, margin, pathWidth, rowHeight, TRUE);
        MoveWindow(
            app.browseButton,
            margin + pathWidth + gap,
            margin,
            buttonWidth,
            rowHeight,
            TRUE);
        MoveWindow(
            app.openButton,
            margin + pathWidth + gap + buttonWidth + gap,
            margin,
            buttonWidth,
            rowHeight,
            TRUE);
        int toolbarLeft = margin;
        const int toolbarTop = margin + rowHeight + gap;

        MoveWindow(app.exportButton, toolbarLeft, toolbarTop, exportButtonWidth, rowHeight, TRUE);
        toolbarLeft += exportButtonWidth + gap;
        MoveWindow(app.exportVisibleButton, toolbarLeft, toolbarTop, exportButtonWidth, rowHeight, TRUE);
        toolbarLeft += exportButtonWidth + gap;
        MoveWindow(app.scanVisibleButton, toolbarLeft, toolbarTop, scanButtonWidth, rowHeight, TRUE);
        toolbarLeft += scanButtonWidth + gap;
        MoveWindow(app.likelyCompleteCheck, toolbarLeft, toolbarTop, checkWidth, rowHeight, TRUE);
        toolbarLeft += checkWidth + gap;
        MoveWindow(app.previewableCheck, toolbarLeft, toolbarTop, previewableWidth, rowHeight, TRUE);
        toolbarLeft += previewableWidth + gap;
        MoveWindow(app.galleryCheck, toolbarLeft, toolbarTop, galleryWidth, rowHeight, TRUE);
        toolbarLeft += galleryWidth + gap;
        MoveWindow(app.overwriteCheck, toolbarLeft, toolbarTop, smallCheckWidth, rowHeight, TRUE);
        toolbarLeft += smallCheckWidth + gap;
        MoveWindow(app.retryIncompleteCheck, toolbarLeft, toolbarTop, retryCheckWidth, rowHeight, TRUE);
        toolbarLeft += retryCheckWidth + gap;
        MoveWindow(app.uuidFilterEdit, toolbarLeft, toolbarTop, uuidFilterWidth, rowHeight, TRUE);
        toolbarLeft += uuidFilterWidth + gap;
        MoveWindow(app.clearUuidFilterButton, toolbarLeft, toolbarTop, clearFilterWidth, rowHeight, TRUE);
        MoveWindow(app.textureList, margin, listTop, listWidth, listHeight, TRUE);
        MoveWindow(app.galleryControl, margin, listTop, listWidth, listHeight, TRUE);
        ShowWindow(app.textureList, app.galleryMode ? SW_HIDE : SW_SHOW);
        ShowWindow(app.galleryControl, app.galleryMode ? SW_SHOW : SW_HIDE);
        UpdateGalleryScroll(app);
        MoveWindow(app.detailsText, detailsLeft, listTop, detailsWidth, detailsHeight, TRUE);
        MoveWindow(
            app.previewControl,
            detailsLeft,
            listTop + detailsHeight + gap,
            detailsWidth,
            listHeight - detailsHeight - gap,
            TRUE);
        MoveWindow(app.statusText, margin, height - margin - statusHeight, contentWidth - progressWidth - gap, statusHeight, TRUE);
        MoveWindow(
            app.progressBar,
            margin + contentWidth - progressWidth,
            height - margin - statusHeight + 2,
            progressWidth,
            statusHeight - 4,
            TRUE);
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        AppState* app = reinterpret_cast<AppState*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));

        switch (message)
        {
            case WM_CREATE:
            {
                CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
                app = reinterpret_cast<AppState*>(create->lpCreateParams);
                app->window = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));

                app->pathEdit = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"EDIT",
                    DefaultCachePath().c_str(),
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(PathEditId)),
                    app->instance,
                    nullptr);
                app->browseButton = CreateWindowW(
                    L"BUTTON",
                    L"Browse",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(BrowseButtonId)),
                    app->instance,
                    nullptr);
                app->openButton = CreateWindowW(
                    L"BUTTON",
                    L"Open",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(OpenButtonId)),
                    app->instance,
                    nullptr);
                app->exportButton = CreateWindowW(
                    L"BUTTON",
                    L"Export Selected",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ExportButtonId)),
                    app->instance,
                    nullptr);
                app->exportVisibleButton = CreateWindowW(
                    L"BUTTON",
                    L"Export Results",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ExportVisibleButtonId)),
                    app->instance,
                    nullptr);
                app->scanVisibleButton = CreateWindowW(
                    L"BUTTON",
                    L"Find Previews",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ScanVisibleButtonId)),
                    app->instance,
                    nullptr);
                app->likelyCompleteCheck = CreateWindowW(
                    L"BUTTON",
                    L"Has data",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(LikelyCompleteCheckId)),
                    app->instance,
                    nullptr);
                app->previewableCheck = CreateWindowW(
                    L"BUTTON",
                    L"Show found",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(PreviewableCheckId)),
                    app->instance,
                    nullptr);
                app->galleryCheck = CreateWindowW(
                    L"BUTTON",
                    L"Gallery view",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(GalleryCheckId)),
                    app->instance,
                    nullptr);
                app->overwriteCheck = CreateWindowW(
                    L"BUTTON",
                    L"Overwrite",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(OverwriteCheckId)),
                    app->instance,
                    nullptr);
                app->retryIncompleteCheck = CreateWindowW(
                    L"BUTTON",
                    L"Retry failed",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(RetryIncompleteCheckId)),
                    app->instance,
                    nullptr);
                app->uuidFilterEdit = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"EDIT",
                    L"",
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(UuidFilterEditId)),
                    app->instance,
                    nullptr);
                SendMessageW(
                    app->uuidFilterEdit,
                    EM_SETCUEBANNER,
                    TRUE,
                    reinterpret_cast<LPARAM>(L"UUID contains"));
                app->clearUuidFilterButton = CreateWindowW(
                    L"BUTTON",
                    L"Clear",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ClearUuidFilterButtonId)),
                    app->instance,
                    nullptr);
                app->textureList = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    WC_LISTVIEWW,
                    L"",
                    WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(TextureListId)),
                    app->instance,
                    nullptr);
                app->galleryControl = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"CacheExplorerGallery",
                    L"",
                    WS_CHILD | WS_VSCROLL,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(GalleryControlId)),
                    app->instance,
                    nullptr);
                app->detailsText = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"EDIT",
                    L"No cache loaded.",
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(DetailsTextId)),
                    app->instance,
                    nullptr);
                app->previewControl = CreateWindowW(
                    L"STATIC",
                    L"",
                    WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(PreviewControlId)),
                    app->instance,
                    nullptr);
                app->statusText = CreateWindowW(
                    L"STATIC",
                    L"Choose a Firestorm texture cache folder.",
                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(StatusTextId)),
                    app->instance,
                    nullptr);
                app->progressBar = CreateWindowExW(
                    0,
                    PROGRESS_CLASSW,
                    L"",
                    WS_CHILD | PBS_SMOOTH,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ProgressBarId)),
                    app->instance,
                    nullptr);

                ConfigureListView(app->textureList);
                PostMessageW(window, AutoOpenMessage, 0, 0);
                return 0;
            }

            case AutoOpenMessage:
                if (app != nullptr)
                {
                    OpenCache(*app);
                }
                return 0;

            case PreviewCompleteMessage:
            {
                std::unique_ptr<PreviewResult> result(
                    reinterpret_cast<PreviewResult*>(lParam));

                if (app != nullptr)
                {
                    ApplyPreviewResult(*app, std::move(result));
                }

                return 0;
            }

            case ExportProgressMessage:
            {
                std::unique_ptr<ExportProgressMessageData> progress(
                    reinterpret_cast<ExportProgressMessageData*>(lParam));

                if (app != nullptr && app->exportInProgress && progress)
                {
                    SetExportProgress(*app, progress->progress);
                }

                return 0;
            }

            case ExportCompleteMessage:
            {
                std::unique_ptr<ExportCompleteMessageData> complete(
                    reinterpret_cast<ExportCompleteMessageData*>(lParam));

                if (app != nullptr)
                {
                    ApplyExportComplete(*app, std::move(complete));
                }

                return 0;
            }

            case ScanProgressMessage:
            {
                std::unique_ptr<ScanProgressMessageData> progress(
                    reinterpret_cast<ScanProgressMessageData*>(lParam));

                if (app != nullptr)
                {
                    ApplyScanProgress(*app, std::move(progress));
                }

                return 0;
            }

            case ScanCompleteMessage:
            {
                std::unique_ptr<ScanCompleteMessageData> complete(
                    reinterpret_cast<ScanCompleteMessageData*>(lParam));

                if (app != nullptr)
                {
                    ApplyScanComplete(*app, std::move(complete));
                }

                return 0;
            }

            case WM_SIZE:
                if (app != nullptr)
                {
                    ResizeControls(*app, LOWORD(lParam), HIWORD(lParam));
                }
                return 0;

            case WM_NOTIFY:
                if (app != nullptr)
                {
                    const NMHDR* header = reinterpret_cast<const NMHDR*>(lParam);

                    if (header->idFrom == TextureListId &&
                        header->code == LVN_ITEMCHANGED)
                    {
                        if (!app->listUpdateInProgress)
                        {
                            UpdateDetails(*app);
                        }
                    }
                    else if (header->idFrom == TextureListId &&
                        header->code == LVN_COLUMNCLICK)
                    {
                        const NMLISTVIEW* listView =
                            reinterpret_cast<const NMLISTVIEW*>(lParam);

                        if (app->sortColumn == listView->iSubItem)
                        {
                            app->sortAscending = !app->sortAscending;
                        }
                        else
                        {
                            app->sortColumn = listView->iSubItem;
                            app->sortAscending = listView->iSubItem != 3;
                        }

                        SetStatus(*app, L"Sorting...");
                        UpdateWindow(app->statusText);

                        ScopedWaitCursor waitCursor;
                        PopulateList(*app);
                        SetEntryStatus(*app);
                    }
                }
                return 0;

            case WM_DRAWITEM:
                if (app != nullptr && wParam == PreviewControlId)
                {
                    DrawPreview(
                        *app,
                        *reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
                    return TRUE;
                }
                break;

            case WM_COMMAND:
                if (app == nullptr)
                {
                    break;
                }

                switch (LOWORD(wParam))
                {
                    case BrowseButtonId:
                    {
                        const std::optional<fs::path> cacheDirectory =
                            PickFolder(window, L"Choose Firestorm texturecache folder");

                        if (cacheDirectory)
                        {
                            SetWindowTextW(app->pathEdit, cacheDirectory->wstring().c_str());
                        }

                        return 0;
                    }

                    case OpenButtonId:
                        OpenCache(*app);
                        return 0;

                    case LikelyCompleteCheckId:
                    {
                        app->likelyCompleteOnly =
                            SendMessageW(app->likelyCompleteCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        if (app->database.IsOpen())
                        {
                            PopulateList(*app);

                            SetEntryStatus(*app);
                        }

                        return 0;
                    }

                    case PreviewableCheckId:
                    {
                        app->previewableOnly =
                            SendMessageW(app->previewableCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;

                        if (app->database.IsOpen())
                        {
                            PopulateList(*app);
                            SetEntryStatus(*app);
                        }

                        return 0;
                    }

                    case GalleryCheckId:
                    {
                        app->galleryMode =
                            SendMessageW(app->galleryCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        RECT client{};
                        GetClientRect(window, &client);
                        ResizeControls(
                            *app,
                            client.right - client.left,
                            client.bottom - client.top);
                        InvalidateRect(app->galleryControl, nullptr, TRUE);
                        return 0;
                    }

                    case ExportButtonId:
                        ExportSelected(*app);
                        return 0;

                    case ExportVisibleButtonId:
                        ExportVisible(*app);
                        return 0;

                    case ScanVisibleButtonId:
                        StartScanVisible(*app);
                        return 0;

                    case UuidFilterEditId:
                        if (HIWORD(wParam) == EN_CHANGE)
                        {
                            app->uuidFilter = GetText(app->uuidFilterEdit);

                            if (app->database.IsOpen())
                            {
                                PopulateList(*app);
                                SetEntryStatus(*app);
                            }
                        }

                        return 0;

                    case ClearUuidFilterButtonId:
                        SetWindowTextW(app->uuidFilterEdit, L"");
                        app->uuidFilter.clear();

                        if (app->database.IsOpen())
                        {
                            PopulateList(*app);
                            SetEntryStatus(*app);
                        }

                        return 0;
                }
                break;

            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&controls);

    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    if (FAILED(comResult))
    {
        return 1;
    }

    ULONG_PTR gdiplusToken = 0;
    gdip::GdiplusStartupInput gdiplusInput;

    if (gdip::GdiplusStartup(
            &gdiplusToken,
            &gdiplusInput,
            nullptr) != gdip::Ok)
    {
        CoUninitialize();
        return 1;
    }

    AppState app;
    app.instance = instance;

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"CacheExplorerWindow";
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&windowClass);

    WNDCLASSW galleryClass{};
    galleryClass.lpfnWndProc = GalleryProc;
    galleryClass.hInstance = instance;
    galleryClass.lpszClassName = L"CacheExplorerGallery";
    galleryClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    galleryClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&galleryClass);

    HWND window = CreateWindowExW(
        0,
        windowClass.lpszClassName,
        L"Cache Explorer",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1100,
        680,
        nullptr,
        nullptr,
        instance,
        &app);

    if (window == nullptr)
    {
        gdip::GdiplusShutdown(gdiplusToken);
        CoUninitialize();
        return 1;
    }

    ShowWindow(window, showCommand == SW_HIDE ? SW_SHOWNORMAL : showCommand);
    UpdateWindow(window);

    MSG message{};

    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    gdip::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return static_cast<int>(message.wParam);
}

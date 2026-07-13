#include "TextureCacheDatabase.h"
#include "TextureExporter.h"
#include "TextureExportState.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#define NOMINMAX
#define UUID WindowsSdkUUID
#include <windows.h>
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
    constexpr int LikelyCompleteCheckId = 1009;
    constexpr int OverwriteCheckId = 1010;
    constexpr int RetryIncompleteCheckId = 1011;
    constexpr int ProgressBarId = 1012;
    constexpr int TextureListId = 1005;
    constexpr int StatusTextId = 1006;
    constexpr int DetailsTextId = 1007;
    constexpr int PreviewControlId = 1008;

    constexpr UINT AutoOpenMessage = WM_APP + 1;

    struct AppState
    {
        HINSTANCE instance = nullptr;
        HWND window = nullptr;
        HWND pathEdit = nullptr;
        HWND browseButton = nullptr;
        HWND openButton = nullptr;
        HWND exportButton = nullptr;
        HWND exportVisibleButton = nullptr;
        HWND likelyCompleteCheck = nullptr;
        HWND overwriteCheck = nullptr;
        HWND retryIncompleteCheck = nullptr;
        HWND textureList = nullptr;
        HWND detailsText = nullptr;
        HWND previewControl = nullptr;
        HWND statusText = nullptr;
        HWND progressBar = nullptr;

        TextureCacheDatabase database;
        std::vector<const CacheEntry*> visibleEntries;
        bool likelyCompleteOnly = false;
        int sortColumn = 3;
        bool sortAscending = false;
        std::unique_ptr<gdip::Image> previewImage;
        fs::path previewDirectory;
    };

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
        std::wostringstream status;
        status
            << L"Showing "
            << app.visibleEntries.size()
            << L" of "
            << app.database.Entries().size()
            << L" texture entries.";
        SetStatus(app, status.str());
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
        EnableWindow(app.likelyCompleteCheck, enabled);
        EnableWindow(app.overwriteCheck, enabled);
        EnableWindow(app.retryIncompleteCheck, enabled);
        EnableWindow(app.textureList, enabled);
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

    void ClearPreview(AppState& app)
    {
        app.previewImage.reset();
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

    void UpdatePreview(
        AppState& app,
        const CacheEntry& entry,
        std::wostringstream& details)
    {
        ClearPreview(app);

        const fs::path previewDirectory = PreviewDirectory(app);

        if (previewDirectory.empty())
        {
            details << L"\r\n\r\nPreview\r\nTemporary preview directory is unavailable.";
            return;
        }

        const fs::path previewFile =
            previewDirectory / (ToWide(entry.uuid.ToString()) + L".png");

        TextureExportOptions options;
        options.overwriteExisting = true;
        options.verboseDecoderErrors = false;
        options.maximumStoredMessages = 0;

        TextureExporter exporter;
        const TexturePngExportResult result = exporter.ExportPngEntry(
            app.database,
            entry,
            previewFile,
            options);

        if (result.status != TexturePngExportStatus::Exported)
        {
            details
                << L"\r\n\r\nPreview\r\n"
                << ToWide(result.message);
            return;
        }

        auto image = std::make_unique<gdip::Image>(previewFile.wstring().c_str());

        if (image->GetLastStatus() != gdip::Ok)
        {
            details << L"\r\n\r\nPreview\r\nPNG was exported but could not be loaded.";
            return;
        }

        details
            << L"\r\n\r\nPreview\r\n"
            << image->GetWidth()
            << L" x "
            << image->GetHeight();

        app.previewImage = std::move(image);
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
            SetDetails(app, L"No texture selected.");
            ClearPreview(app);
            return;
        }

        const std::size_t index = static_cast<std::size_t>(item);

        if (index >= app.visibleEntries.size())
        {
            SetDetails(app, L"No texture selected.");
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

        UpdatePreview(app, entry, details);
        SetDetails(app, details.str());
    }

    bool IsLikelyComplete(const CacheEntry& entry)
    {
        return entry.imageSize > 0 &&
            entry.bodySize > 0 &&
            entry.imageSize > entry.bodySize;
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
        UpdateDetails(app);
    }

    bool OpenCache(AppState& app)
    {
        const fs::path cachePath =
            ResolveTextureCacheDirectory(GetText(app.pathEdit));
        TextureCacheDatabase database;
        const CacheError result = database.Open(cachePath);

        if (result != CacheError::None)
        {
            SetStatus(app, L"Could not open texture.entries in the selected folder.");
            SetDetails(app, L"No cache loaded.");
            return false;
        }

        app.database = std::move(database);
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

        const std::optional<fs::path> outputDirectory =
            PickFolder(app.window, L"Choose export directory");

        if (!outputDirectory)
        {
            return;
        }

        TextureExportState exportState;
        TextureExportOptions options;
        options.overwriteExisting =
            SendMessageW(app.overwriteCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        options.skipKnownIncomplete =
            SendMessageW(app.retryIncompleteCheck, BM_GETCHECK, 0, 0) != BST_CHECKED;
        options.maximumStoredMessages = 10;
        options.exportState = &exportState;

        const fs::path stateFile =
            *outputDirectory / L".cacheexplorer-export-state.tsv";
        std::string stateError;

        if (!exportState.Load(stateFile, stateError))
        {
            SetStatus(app, L"Could not load export state file.");
            return;
        }

        ShowWindow(app.progressBar, SW_SHOW);
        SetExportControlsEnabled(app, false);
        ScopedWaitCursor waitCursor;
        SetExportProgress(app, TextureExportProgress{0, entries.size()});

        TextureExporter exporter;
        const BulkExportResults results = exporter.ExportPngEntries(
            app.database,
            entries,
            *outputDirectory,
            options,
            [&app](const TextureExportProgress& progress)
            {
                SetExportProgress(app, progress);
            });

        SetExportControlsEnabled(app, true);
        ShowWindow(app.progressBar, SW_HIDE);

        if (!exportState.Save(stateFile, stateError))
        {
            SetStatus(app, L"Export finished, but state could not be saved.");
            return;
        }

        std::wostringstream status;
        status
            << L"Exported " << results.exported
            << L", skipped existing " << results.skippedExisting
            << L", skipped incomplete " << results.skippedKnownIncomplete
            << L", incomplete " << results.incompleteTextures
            << L", errors " << results.ErrorCount()
            << L". Overwrite: "
            << (options.overwriteExisting ? L"on" : L"off")
            << L", retry incomplete: "
            << (!options.skipKnownIncomplete ? L"on" : L"off")
            << L".";
        SetStatus(app, status.str());
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

    void ResizeControls(AppState& app, int width, int height)
    {
        constexpr int margin = 12;
        constexpr int rowHeight = 28;
        constexpr int buttonWidth = 90;
        constexpr int exportButtonWidth = 120;
        constexpr int checkWidth = 130;
        constexpr int smallCheckWidth = 95;
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
        MoveWindow(app.exportButton, margin, margin + rowHeight + gap, exportButtonWidth, rowHeight, TRUE);
        MoveWindow(
            app.exportVisibleButton,
            margin + exportButtonWidth + gap,
            margin + rowHeight + gap,
            exportButtonWidth,
            rowHeight,
            TRUE);
        MoveWindow(
            app.likelyCompleteCheck,
            margin + (exportButtonWidth * 2) + (gap * 2),
            margin + rowHeight + gap,
            checkWidth,
            rowHeight,
            TRUE);
        MoveWindow(
            app.overwriteCheck,
            margin + (exportButtonWidth * 2) + (gap * 2) + checkWidth + gap,
            margin + rowHeight + gap,
            smallCheckWidth,
            rowHeight,
            TRUE);
        MoveWindow(
            app.retryIncompleteCheck,
            margin + (exportButtonWidth * 2) + (gap * 2) + checkWidth + gap + smallCheckWidth + gap,
            margin + rowHeight + gap,
            checkWidth,
            rowHeight,
            TRUE);
        MoveWindow(app.textureList, margin, listTop, listWidth, listHeight, TRUE);
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
                    L"Export Visible",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ExportVisibleButtonId)),
                    app->instance,
                    nullptr);
                app->likelyCompleteCheck = CreateWindowW(
                    L"BUTTON",
                    L"Likely complete",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(LikelyCompleteCheckId)),
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
                    L"Retry incomplete",
                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                    0,
                    0,
                    0,
                    0,
                    window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(RetryIncompleteCheckId)),
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
                        UpdateDetails(*app);
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

                    case ExportButtonId:
                        ExportSelected(*app);
                        return 0;

                    case ExportVisibleButtonId:
                        ExportVisible(*app);
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

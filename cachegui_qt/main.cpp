#include "J2CDecoder.h"
#include "TextureCacheDatabase.h"
#include "TextureExporter.h"
#include "TextureRebuilder.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdint>
#include <filesystem>
#include <future>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <QApplication>
#include <QAbstractTableModel>
#include <QDateTime>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPixmap>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace fs = std::filesystem;

namespace
{
    const char* CacheErrorMessage(CacheError error)
    {
        switch (error)
        {
            case CacheError::None:
                return "No error.";

            case CacheError::FileNotFound:
                return "texture.entries could not be found.";

            case CacheError::ReadError:
                return "The cache entry table could not be read.";

            case CacheError::InvalidHeader:
                return "The texture.entries header is invalid.";

            case CacheError::UnsupportedVersion:
                return "The cache version is unsupported.";
        }

        return "Unknown cache error.";
    }

    QString ToQString(const std::string& value)
    {
        return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
    }

    QString PathToQString(const fs::path& path)
    {
#ifdef _WIN32
        return QString::fromStdWString(path.wstring());
#else
        return QString::fromUtf8(path.u8string().c_str());
#endif
    }

    fs::path PathFromQString(const QString& value)
    {
        const QByteArray utf8 = value.toUtf8();
        return fs::u8path(utf8.constData());
    }

    fs::path ResolveTextureCacheDirectory(const fs::path& suppliedPath)
    {
        if (fs::exists(suppliedPath / "texture.entries"))
        {
            return suppliedPath;
        }

        const fs::path childTextureCache = suppliedPath / "texturecache";

        if (fs::exists(childTextureCache / "texture.entries"))
        {
            return childTextureCache;
        }

        return suppliedPath;
    }

    QString DefaultCachePath()
    {
#ifdef _WIN32
        const QString localAppData =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

        if (!localAppData.isEmpty())
        {
            return localAppData + "/FirestormOS_x64/texturecache";
        }
#endif

        return {};
    }

    QString FormatTime(std::time_t timestamp)
    {
        if (timestamp <= 0)
        {
            return {};
        }

        return QDateTime::fromSecsSinceEpoch(
                static_cast<qint64>(timestamp))
            .toLocalTime()
            .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }

    enum class PreviewState
    {
        Unknown,
        Checking,
        Previewable,
        Unavailable,
        LoadFailed
    };

    struct PreviewRecord
    {
        PreviewState state = PreviewState::Unknown;
        QString message;
        QPixmap pixmap;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    class PreviewCache
    {
    public:
        void Clear()
        {
            records_.clear();
        }

        void SetChecking(const CacheEntry& entry)
        {
            PreviewRecord record;
            record.state = PreviewState::Checking;
            records_[entry.cacheIndex] = record;
        }

        void SetUnavailable(const CacheEntry& entry, const QString& message)
        {
            PreviewRecord record;
            record.state = PreviewState::Unavailable;
            record.message = message;
            records_[entry.cacheIndex] = record;
        }

        void SetLoadFailed(const CacheEntry& entry, const QString& message)
        {
            PreviewRecord record;
            record.state = PreviewState::LoadFailed;
            record.message = message;
            records_[entry.cacheIndex] = record;
        }

        void SetPreviewable(
            const CacheEntry& entry,
            const QPixmap& pixmap,
            std::uint32_t width,
            std::uint32_t height)
        {
            PreviewRecord record;
            record.state = PreviewState::Previewable;
            record.pixmap = pixmap;
            record.width = width;
            record.height = height;
            records_[entry.cacheIndex] = record;
        }

        const PreviewRecord* Find(const CacheEntry& entry) const
        {
            const auto record = records_.find(entry.cacheIndex);

            if (record == records_.end())
            {
                return nullptr;
            }

            return &record->second;
        }

        QString StatusText(const CacheEntry& entry) const
        {
            const PreviewRecord* record = Find(entry);

            if (record == nullptr)
            {
                return {};
            }

            switch (record->state)
            {
                case PreviewState::Unknown:
                    return {};

                case PreviewState::Checking:
                    return QStringLiteral("Checking");

                case PreviewState::Previewable:
                    return QStringLiteral("Preview");

                case PreviewState::Unavailable:
                    return QStringLiteral("No preview");

                case PreviewState::LoadFailed:
                    return QStringLiteral("Load failed");
            }

            return {};
        }

        int StatusRank(const CacheEntry& entry) const
        {
            const PreviewRecord* record = Find(entry);

            if (record == nullptr)
            {
                return 0;
            }

            switch (record->state)
            {
                case PreviewState::Unknown:
                    return 0;

                case PreviewState::Checking:
                    return 1;

                case PreviewState::Unavailable:
                case PreviewState::LoadFailed:
                    return 2;

                case PreviewState::Previewable:
                    return 3;
            }

            return 0;
        }

    private:
        std::unordered_map<std::uint32_t, PreviewRecord> records_;
    };

    class CacheEntryTableModel final : public QAbstractTableModel
    {
    public:
        explicit CacheEntryTableModel(QObject* parent = nullptr)
            : QAbstractTableModel(parent)
        {
        }

        void SetDatabase(const TextureCacheDatabase* database)
        {
            beginResetModel();
            database_ = database;
            endResetModel();
        }

        void SetPreviewCache(const PreviewCache* previewCache)
        {
            beginResetModel();
            previewCache_ = previewCache;
            endResetModel();
        }

        void NotifyAllPreviewStatusesChanged()
        {
            if (rowCount() > 0)
            {
                emit dataChanged(
                    index(0, 5),
                    index(rowCount() - 1, 5),
                    {Qt::DisplayRole, Qt::UserRole});
            }
        }

        void NotifyPreviewStatusChanged(std::uint32_t cacheIndex)
        {
            const int row = RowForCacheIndex(cacheIndex);

            if (row >= 0)
            {
                const QModelIndex statusIndex = index(row, 5);
                emit dataChanged(
                    statusIndex,
                    statusIndex,
                    {Qt::DisplayRole, Qt::UserRole});
            }
        }

        int rowCount(const QModelIndex& parent = {}) const override
        {
            if (parent.isValid() || database_ == nullptr)
            {
                return 0;
            }

            return static_cast<int>(database_->Entries().size());
        }

        int columnCount(const QModelIndex& parent = {}) const override
        {
            return parent.isValid() ? 0 : 6;
        }

        QVariant data(const QModelIndex& index, int role) const override
        {
            if (!index.isValid() || database_ == nullptr)
            {
                return {};
            }

            const CacheEntry* entry = EntryAt(index.row());

            if (entry == nullptr)
            {
                return {};
            }

            if (role == Qt::DisplayRole)
            {
                switch (index.column())
                {
                    case 0:
                        return ToQString(entry->uuid.ToString());

                    case 1:
                        return entry->imageSize;

                    case 2:
                        return entry->bodySize;

                    case 3:
                        return static_cast<qulonglong>(entry->cacheIndex);

                    case 4:
                        return FormatTime(entry->timestamp);

                    case 5:
                        return PreviewStatusText(*entry);

                    default:
                        return {};
                }
            }

            if (role == Qt::UserRole)
            {
                switch (index.column())
                {
                    case 0:
                        return ToQString(entry->uuid.ToString());

                    case 1:
                        return entry->imageSize;

                    case 2:
                        return entry->bodySize;

                    case 3:
                        return static_cast<qulonglong>(entry->cacheIndex);

                    case 4:
                        return static_cast<qlonglong>(entry->timestamp);

                    case 5:
                        return PreviewStatusRank(*entry);

                    default:
                        return {};
                }
            }

            return {};
        }

        QVariant headerData(
            int section,
            Qt::Orientation orientation,
            int role) const override
        {
            if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            {
                return {};
            }

            switch (section)
            {
                case 0:
                    return QStringLiteral("UUID");

                case 1:
                    return QStringLiteral("Image");

                case 2:
                    return QStringLiteral("Body");

                case 3:
                    return QStringLiteral("Cache index");

                case 4:
                    return QStringLiteral("Timestamp");

                case 5:
                    return QStringLiteral("Preview");

                default:
                    return {};
            }
        }

        const CacheEntry* EntryAt(int row) const
        {
            if (database_ == nullptr || row < 0)
            {
                return nullptr;
            }

            const auto& entries = database_->Entries();
            const auto index = static_cast<std::size_t>(row);

            if (index >= entries.size())
            {
                return nullptr;
            }

            return &entries[index];
        }

        int RowForEntry(const CacheEntry& entry) const
        {
            return RowForCacheIndex(entry.cacheIndex);
        }

    private:
        QString PreviewStatusText(const CacheEntry& entry) const
        {
            if (previewCache_ == nullptr)
            {
                return {};
            }

            return previewCache_->StatusText(entry);
        }

        int PreviewStatusRank(const CacheEntry& entry) const
        {
            if (previewCache_ == nullptr)
            {
                return 0;
            }

            return previewCache_->StatusRank(entry);
        }

        int RowForCacheIndex(std::uint32_t cacheIndex) const
        {
            if (database_ == nullptr)
            {
                return -1;
            }

            const auto& entries = database_->Entries();

            for (std::size_t row = 0; row < entries.size(); ++row)
            {
                if (entries[row].cacheIndex == cacheIndex)
                {
                    return static_cast<int>(row);
                }
            }

            return -1;
        }

        const TextureCacheDatabase* database_ = nullptr;
        const PreviewCache* previewCache_ = nullptr;
    };

    struct PreviewDecodeResult
    {
        std::uint64_t requestId = 0;
        CacheEntry entry;
        bool succeeded = false;
        std::string message;
        DecodedImage image;
    };

    PreviewDecodeResult DecodePreview(
        std::uint64_t requestId,
        const fs::path& cacheDirectory,
        const CacheEntry& entry)
    {
        PreviewDecodeResult result;
        result.requestId = requestId;
        result.entry = entry;

        TextureRebuilder rebuilder;
        std::vector<std::uint8_t> encodedData;

        const RebuildError rebuildResult =
            rebuilder.Rebuild(cacheDirectory, entry, encodedData);

        if (rebuildResult != RebuildError::None)
        {
            result.message = TextureRebuilder::ErrorMessage(rebuildResult);
            return result;
        }

        J2CDecoder decoder;
        const DecodeError decodeResult =
            decoder.Decode(encodedData, result.image, false);

        if (decodeResult != DecodeError::None)
        {
            result.message = J2CDecoder::ErrorMessage(decodeResult);
            return result;
        }

        result.succeeded = true;
        return result;
    }

    class MainWindow final : public QMainWindow
    {
    public:
        MainWindow()
        {
            setWindowTitle(QStringLiteral("Cache Explorer Qt Spike"));
            resize(1050, 680);

            auto* root = new QWidget(this);
            auto* layout = new QVBoxLayout(root);

            auto* pathLayout = new QGridLayout();
            pathLabel_ = new QLabel(QStringLiteral("Texture cache folder"), root);
            pathEdit_ = new QLineEdit(DefaultCachePath(), root);
            browseButton_ = new QPushButton(QStringLiteral("Browse"), root);
            openButton_ = new QPushButton(QStringLiteral("Open"), root);

            pathLayout->addWidget(pathLabel_, 0, 0);
            pathLayout->addWidget(pathEdit_, 0, 1);
            pathLayout->addWidget(browseButton_, 0, 2);
            pathLayout->addWidget(openButton_, 0, 3);
            pathLayout->setColumnStretch(1, 1);

            previewButton_ = new QPushButton(QStringLiteral("Preview"), root);
            tryNextButton_ = new QPushButton(QStringLiteral("Try Next Preview"), root);
            exportButton_ = new QPushButton(QStringLiteral("Export PNG"), root);
            previewButton_->setEnabled(false);
            tryNextButton_->setEnabled(false);
            exportButton_->setEnabled(false);

            auto* actionLayout = new QHBoxLayout();
            actionLayout->addWidget(previewButton_);
            actionLayout->addWidget(tryNextButton_);
            actionLayout->addWidget(exportButton_);
            actionLayout->addStretch(1);

            proxyModel_ = new QSortFilterProxyModel(root);
            tableModel_.SetPreviewCache(&previewCache_);
            proxyModel_->setSourceModel(&tableModel_);
            proxyModel_->setSortRole(Qt::UserRole);
            proxyModel_->setDynamicSortFilter(false);

            table_ = new QTableView(root);
            table_->setModel(proxyModel_);
            table_->setSelectionBehavior(QAbstractItemView::SelectRows);
            table_->setSelectionMode(QAbstractItemView::SingleSelection);
            table_->setSortingEnabled(true);
            table_->verticalHeader()->hide();
            table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
            table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
            table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
            table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
            table_->setColumnWidth(1, 90);
            table_->setColumnWidth(2, 90);
            table_->setColumnWidth(3, 95);
            table_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
            table_->setColumnWidth(4, 150);
            table_->setColumnWidth(5, 95);

            previewPollTimer_ = new QTimer(root);
            previewPollTimer_->setInterval(50);

            previewLabel_ = new QLabel(QStringLiteral("No preview selected."), root);
            previewLabel_->setAlignment(Qt::AlignCenter);
            previewLabel_->setMinimumSize(320, 320);
            previewLabel_->setStyleSheet(QStringLiteral("QLabel { background: #202020; color: #d0d0d0; }"));

            auto* contentLayout = new QHBoxLayout();
            contentLayout->addWidget(table_, 3);
            contentLayout->addWidget(previewLabel_, 1);

            statusLabel_ = new QLabel(
                QStringLiteral("Choose a Firestorm texture cache folder."),
                root);

            layout->addLayout(pathLayout);
            layout->addLayout(actionLayout);
            layout->addLayout(contentLayout, 1);
            layout->addWidget(statusLabel_);
            setCentralWidget(root);

            connect(
                browseButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    BrowseForCache();
                });

            connect(
                openButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    OpenCache();
                });

            connect(
                pathEdit_,
                &QLineEdit::returnPressed,
                this,
                [this]()
                {
                    OpenCache();
                });

            connect(
                table_->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                [this]()
                {
                    UpdateActionState();
                    ShowCachedPreviewForSelection();
                });

            connect(
                previewButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    PreviewSelected();
                });

            connect(
                tryNextButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    StartTryNextPreview();
                });

            connect(
                exportButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    ExportSelected();
                });

            connect(
                previewPollTimer_,
                &QTimer::timeout,
                this,
                [this]()
                {
                    PollPreviewWorker();
                });
        }

    private:
        void BrowseForCache()
        {
            const QString selectedDirectory =
                QFileDialog::getExistingDirectory(
                    this,
                    QStringLiteral("Choose Firestorm texture cache folder"),
                    pathEdit_->text());

            if (!selectedDirectory.isEmpty())
            {
                pathEdit_->setText(selectedDirectory);
            }
        }

        void OpenCache()
        {
            const fs::path cacheDirectory =
                ResolveTextureCacheDirectory(PathFromQString(pathEdit_->text()));

            SetBusy(true, QStringLiteral("Opening cache..."));

            const CacheError result = database_.Open(cacheDirectory);

            if (result != CacheError::None)
            {
                tableModel_.SetDatabase(nullptr);
                previewCache_.Clear();
                tableModel_.NotifyAllPreviewStatusesChanged();
                ClearPreview();
                UpdateActionState();
                SetBusy(false);
                statusLabel_->setText(
                    QStringLiteral("Could not open cache: ")
                    + QString::fromUtf8(CacheErrorMessage(result)));
                return;
            }

            pathEdit_->setText(PathToQString(database_.CacheDirectory()));
            PopulateTable();
            previewCache_.Clear();
            tableModel_.NotifyAllPreviewStatusesChanged();
            ClearPreview();
            UpdateActionState();
            SetBusy(false);
        }

        void PopulateTable()
        {
            const auto& entries = database_.Entries();

            tableModel_.SetDatabase(&database_);
            table_->sortByColumn(4, Qt::DescendingOrder);

            const CacheHeader& header = database_.Header();
            statusLabel_->setText(
                QStringLiteral("Loaded %1 valid texture entries from %2 slots. Cache version %3.")
                    .arg(entries.size())
                    .arg(header.entryCount)
                    .arg(header.version));
        }

        const CacheEntry* SelectedEntry() const
        {
            const QModelIndexList selectedRows =
                table_->selectionModel()->selectedRows();

            if (selectedRows.empty())
            {
                return nullptr;
            }

            const QModelIndex sourceIndex =
                proxyModel_->mapToSource(selectedRows.front());

            if (!sourceIndex.isValid())
            {
                return nullptr;
            }

            return tableModel_.EntryAt(sourceIndex.row());
        }

        void UpdateActionState()
        {
            const bool hasSelection = SelectedEntry() != nullptr;
            const bool previewIdle = !previewWorkerActive_;
            previewButton_->setEnabled(
                !busy_ && !tryNextActive_ && previewIdle && hasSelection);
            tryNextButton_->setEnabled(
                !busy_ && !tryNextActive_ && previewIdle && database_.IsOpen());
            exportButton_->setEnabled(
                !busy_ && !tryNextActive_ && previewIdle && hasSelection);
        }

        void SetBusy(bool busy, const QString& message = {})
        {
            busy_ = busy;
            openButton_->setEnabled(!busy);
            browseButton_->setEnabled(!busy);
            table_->setEnabled(!busy);
            UpdateActionState();

            if (!message.isEmpty())
            {
                statusLabel_->setText(message);
            }

            if (busy)
            {
                QApplication::setOverrideCursor(Qt::WaitCursor);
                QApplication::processEvents();
            }
            else
            {
                QApplication::restoreOverrideCursor();
            }
        }

        void SelectEntry(const CacheEntry& entry)
        {
            const int sourceRow = tableModel_.RowForEntry(entry);

            if (sourceRow < 0)
            {
                return;
            }

            const QModelIndex sourceIndex = tableModel_.index(sourceRow, 0);
            const QModelIndex proxyIndex = proxyModel_->mapFromSource(sourceIndex);

            if (!proxyIndex.isValid())
            {
                return;
            }

            table_->selectRow(proxyIndex.row());
            table_->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
        }

        TexturePngExportResult ExportEntryToFile(
            const CacheEntry& entry,
            const fs::path& outputFile,
            bool overwriteExisting)
        {
            TextureExportOptions options;
            options.overwriteExisting = overwriteExisting;
            options.verboseDecoderErrors = false;

            TextureExporter exporter;
            return exporter.ExportPngEntry(
                database_,
                entry,
                outputFile,
                options);
        }

        void StartPreviewRequest(
            const CacheEntry& entry,
            bool showFailure,
            bool continueTryNext)
        {
            if (previewWorkerActive_ || !database_.IsOpen())
            {
                return;
            }

            const std::uint64_t requestId = nextPreviewRequestId_++;
            const fs::path cacheDirectory = database_.CacheDirectory();

            previewWorkerActive_ = true;
            activePreviewRequestId_ = requestId;
            activePreviewShowFailure_ = showFailure;
            activePreviewContinueTryNext_ = continueTryNext;
            previewCache_.SetChecking(entry);
            tableModel_.NotifyPreviewStatusChanged(entry.cacheIndex);
            UpdateActionState();

            previewFuture_ = std::async(
                std::launch::async,
                [requestId, cacheDirectory, entry]()
                {
                    return DecodePreview(requestId, cacheDirectory, entry);
                });

            if (!previewPollTimer_->isActive())
            {
                previewPollTimer_->start();
            }

            statusLabel_->setText(
                QStringLiteral("Checking preview %1...")
                    .arg(ToQString(entry.uuid.ToString())));
        }

        void PollPreviewWorker()
        {
            if (!previewWorkerActive_)
            {
                previewPollTimer_->stop();
                return;
            }

            if (previewFuture_.wait_for(std::chrono::seconds(0)) !=
                std::future_status::ready)
            {
                return;
            }

            PreviewDecodeResult result = previewFuture_.get();
            previewWorkerActive_ = false;
            previewPollTimer_->stop();

            if (result.requestId != activePreviewRequestId_)
            {
                UpdateActionState();
                return;
            }

            ApplyPreviewResult(result);
        }

        void ApplyPreviewResult(const PreviewDecodeResult& result)
        {
            const bool showFailure = activePreviewShowFailure_;
            const bool continueTryNext = activePreviewContinueTryNext_;
            activePreviewContinueTryNext_ = false;
            activePreviewShowFailure_ = false;

            if (!result.succeeded)
            {
                previewCache_.SetUnavailable(
                    result.entry,
                    ToQString(result.message));
                tableModel_.NotifyPreviewStatusChanged(result.entry.cacheIndex);

                if (showFailure)
                {
                    ClearPreview();
                    statusLabel_->setText(
                        QStringLiteral("Preview unavailable: ")
                        + ToQString(result.message));
                }

                if (continueTryNext)
                {
                    QTimer::singleShot(
                        0,
                        this,
                        [this]()
                        {
                            ContinueTryNextPreview();
                        });
                }
                else
                {
                    UpdateActionState();
                }

                return;
            }

            const DecodedImage& decodedImage = result.image;
            const QImage image(
                decodedImage.rgba.data(),
                static_cast<int>(decodedImage.width),
                static_cast<int>(decodedImage.height),
                static_cast<int>(decodedImage.width * 4),
                QImage::Format_RGBA8888);

            if (image.isNull())
            {
                previewCache_.SetLoadFailed(
                    result.entry,
                    QStringLiteral("Preview image could not be created."));
                tableModel_.NotifyPreviewStatusChanged(result.entry.cacheIndex);

                if (showFailure)
                {
                    ClearPreview();
                    statusLabel_->setText(QStringLiteral("Preview image could not be created."));
                }

                if (continueTryNext)
                {
                    QTimer::singleShot(
                        0,
                        this,
                        [this]()
                        {
                            ContinueTryNextPreview();
                        });
                }
                else
                {
                    UpdateActionState();
                }

                return;
            }

            const QPixmap pixmap = QPixmap::fromImage(image.copy());
            previewCache_.SetPreviewable(
                result.entry,
                pixmap,
                decodedImage.width,
                decodedImage.height);
            tableModel_.NotifyPreviewStatusChanged(result.entry.cacheIndex);
            previewPixmap_ = pixmap;
            ShowPreviewPixmap();
            SelectEntry(result.entry);

            tryNextActive_ = false;
            statusLabel_->setText(
                QStringLiteral("Preview ready: %1 (%2 x %3)")
                    .arg(ToQString(result.entry.uuid.ToString()))
                    .arg(decodedImage.width)
                    .arg(decodedImage.height));
            UpdateActionState();
        }

        void ShowCachedPreviewForSelection()
        {
            const CacheEntry* entry = SelectedEntry();

            if (entry == nullptr)
            {
                return;
            }

            const PreviewRecord* record = previewCache_.Find(*entry);

            if (record == nullptr ||
                record->state != PreviewState::Previewable ||
                record->pixmap.isNull())
            {
                return;
            }

            previewPixmap_ = record->pixmap;
            ShowPreviewPixmap();
            statusLabel_->setText(
                QStringLiteral("Preview ready: %1 (%2 x %3)")
                    .arg(ToQString(entry->uuid.ToString()))
                    .arg(record->width)
                    .arg(record->height));
        }

        void PreviewSelected()
        {
            const CacheEntry* entry = SelectedEntry();

            if (entry == nullptr)
            {
                return;
            }

            StartPreviewRequest(*entry, true, false);
        }

        void StartTryNextPreview()
        {
            if (!database_.IsOpen() || tryNextActive_)
            {
                return;
            }

            const QModelIndexList selectedRows =
                table_->selectionModel()->selectedRows();
            tryNextProxyRow_ = 0;

            if (!selectedRows.empty())
            {
                tryNextProxyRow_ = selectedRows.front().row() + 1;
            }

            tryNextAttempts_ = 0;
            tryNextActive_ = true;
            UpdateActionState();
            statusLabel_->setText(QStringLiteral("Looking for the next previewable texture..."));
            QTimer::singleShot(0, this, [this]() { ContinueTryNextPreview(); });
        }

        void ContinueTryNextPreview()
        {
            if (!tryNextActive_)
            {
                return;
            }

            constexpr int MaximumAttemptsPerClick = 200;

            if (tryNextProxyRow_ >= proxyModel_->rowCount() ||
                tryNextAttempts_ >= MaximumAttemptsPerClick)
            {
                tryNextActive_ = false;
                UpdateActionState();
                statusLabel_->setText(
                    QStringLiteral("No previewable texture found in the next %1 entries.")
                        .arg(tryNextAttempts_));
                return;
            }

            const QModelIndex proxyIndex =
                proxyModel_->index(tryNextProxyRow_, 0);
            const QModelIndex sourceIndex =
                proxyModel_->mapToSource(proxyIndex);
            ++tryNextProxyRow_;
            ++tryNextAttempts_;

            const CacheEntry* entry =
                tableModel_.EntryAt(sourceIndex.row());

            if (entry == nullptr)
            {
                QTimer::singleShot(0, this, [this]() { ContinueTryNextPreview(); });
                return;
            }

            statusLabel_->setText(
                QStringLiteral("Trying preview %1...")
                    .arg(ToQString(entry->uuid.ToString())));

            StartPreviewRequest(*entry, false, true);
        }

        void ExportSelected()
        {
            const CacheEntry* entry = SelectedEntry();

            if (entry == nullptr)
            {
                return;
            }

            const QString defaultName =
                ToQString(entry->uuid.ToString()) + QStringLiteral(".png");
            const QString outputFile =
                QFileDialog::getSaveFileName(
                    this,
                    QStringLiteral("Export PNG"),
                    defaultName,
                    QStringLiteral("PNG images (*.png)"));

            if (outputFile.isEmpty())
            {
                return;
            }

            const TexturePngExportResult result =
                ExportEntryToFile(*entry, PathFromQString(outputFile), true);

            if (!result.Succeeded())
            {
                statusLabel_->setText(
                    QStringLiteral("Export failed: ")
                    + ToQString(result.message));
                return;
            }

            statusLabel_->setText(
                QStringLiteral("Exported PNG: %1").arg(outputFile));
        }

        void ClearPreview()
        {
            previewPixmap_ = {};
            previewLabel_->setText(QStringLiteral("No preview selected."));
            previewLabel_->setPixmap({});
        }

        void ShowPreviewPixmap()
        {
            previewLabel_->setPixmap(
                previewPixmap_.scaled(
                    previewLabel_->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
        }

        QLabel* pathLabel_ = nullptr;
        QLineEdit* pathEdit_ = nullptr;
        QPushButton* browseButton_ = nullptr;
        QPushButton* openButton_ = nullptr;
        QPushButton* previewButton_ = nullptr;
        QPushButton* tryNextButton_ = nullptr;
        QPushButton* exportButton_ = nullptr;
        QTableView* table_ = nullptr;
        QLabel* previewLabel_ = nullptr;
        QLabel* statusLabel_ = nullptr;
        PreviewCache previewCache_;
        CacheEntryTableModel tableModel_;
        QSortFilterProxyModel* proxyModel_ = nullptr;
        QTimer* previewPollTimer_ = nullptr;
        QPixmap previewPixmap_;
        std::future<PreviewDecodeResult> previewFuture_;
        TextureCacheDatabase database_;
        bool busy_ = false;
        bool previewWorkerActive_ = false;
        bool activePreviewShowFailure_ = false;
        bool activePreviewContinueTryNext_ = false;
        bool tryNextActive_ = false;
        int tryNextProxyRow_ = 0;
        int tryNextAttempts_ = 0;
        std::uint64_t nextPreviewRequestId_ = 1;
        std::uint64_t activePreviewRequestId_ = 0;
    };
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

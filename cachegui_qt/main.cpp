#include "CacheEntryTableModel.h"
#include "GalleryListView.h"
#include "GalleryPreviewQueue.h"
#include "PreviewCache.h"
#include "PreviewDecodeWorker.h"
#include "PreviewImage.h"
#include "QtHelpers.h"
#include "TextureCacheDatabase.h"
#include "TextureExporter.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <future>
#include <sstream>
#include <string>
#include <utility>

#include <QApplication>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QResizeEvent>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
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

    enum class PreviewRequestKind
    {
        Manual,
        TryNext
    };

    class MainWindow final : public QMainWindow
    {
    public:
        MainWindow()
        {
            setWindowTitle(QStringLiteral("Cache Explorer"));
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
            viewToggleButton_ = new QPushButton(QStringLiteral("Gallery"), root);
            galleryActivityLabel_ = new QLabel(root);
            galleryActivityLabel_->setStyleSheet(QStringLiteral("QLabel { color: #666; }"));
            galleryActivityLabel_->hide();
            previewButton_->setEnabled(false);
            tryNextButton_->setEnabled(false);
            exportButton_->setEnabled(false);
            viewToggleButton_->setEnabled(false);

            auto* actionLayout = new QHBoxLayout();
            actionLayout->addWidget(previewButton_);
            actionLayout->addWidget(tryNextButton_);
            actionLayout->addWidget(exportButton_);
            actionLayout->addStretch(1);
            actionLayout->addWidget(galleryActivityLabel_);
            actionLayout->addWidget(viewToggleButton_);

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
            table_->setIconSize(QSize(16, 16));
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

            galleryView_ = new GalleryListView(root);
            galleryView_->setModel(proxyModel_);
            galleryView_->setModelColumn(0);
            galleryView_->setSelectionMode(QAbstractItemView::SingleSelection);
            galleryView_->setViewMode(QListView::IconMode);
            galleryView_->setResizeMode(QListView::Adjust);
            galleryView_->setMovement(QListView::Static);
            galleryView_->setIconSize(QSize(128, 128));
            galleryView_->setGridSize(QSize(180, 170));
            galleryView_->setUniformItemSizes(true);
            galleryView_->setWordWrap(false);
            galleryView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

            viewStack_ = new QStackedWidget(root);
            viewStack_->addWidget(table_);
            viewStack_->addWidget(galleryView_);

            previewPollTimer_ = new QTimer(root);
            previewPollTimer_->setInterval(50);
            galleryPreviewPollTimer_ = new QTimer(root);
            galleryPreviewPollTimer_->setInterval(50);

            previewLabel_ = new QLabel(QStringLiteral("No preview selected."), root);
            previewLabel_->setAlignment(Qt::AlignCenter);
            previewLabel_->setMinimumSize(320, 320);
            previewLabel_->setStyleSheet(QStringLiteral("QLabel { background: #202020; color: #d0d0d0; }"));

            auto* contentLayout = new QHBoxLayout();
            contentLayout->addWidget(viewStack_, 3);
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
                galleryView_->selectionModel(),
                &QItemSelectionModel::selectionChanged,
                this,
                [this]()
                {
                    UpdateActionState();
                    ShowCachedPreviewForSelection();
                });

            connect(
                galleryView_->selectionModel(),
                &QItemSelectionModel::currentChanged,
                this,
                [this](const QModelIndex&, const QModelIndex&)
                {
                    UpdateActionState();
                    ShowCachedPreviewForSelection();
                });

            connect(
                galleryView_->verticalScrollBar(),
                &QScrollBar::valueChanged,
                this,
                [this]()
                {
                    ScheduleGalleryPreviewSearch();
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
                viewToggleButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    ToggleViewMode();
                });

            connect(
                previewPollTimer_,
                &QTimer::timeout,
                this,
                [this]()
                {
                    PollPreviewWorker();
                });

            connect(
                galleryPreviewPollTimer_,
                &QTimer::timeout,
                this,
                [this]()
                {
                    PollGalleryPreviewWorker();
                });
        }

    protected:
        void resizeEvent(QResizeEvent* event) override
        {
            QMainWindow::resizeEvent(event);

            if (!previewPixmap_.isNull())
            {
                ShowPreviewPixmap();
            }
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
                ClearGalleryPreviewQueue();
                tableModel_.NotifyAllPreviewStatusesChanged();
                ClearPreview();
                UpdateActionState();
                SetBusy(false);
                UpdateGalleryActivity();
                statusLabel_->setText(
                    QStringLiteral("Could not open cache: ")
                    + QString::fromUtf8(CacheErrorMessage(result)));
                return;
            }

            pathEdit_->setText(PathToQString(database_.CacheDirectory()));
            PopulateTable();
            previewCache_.Clear();
            ClearGalleryPreviewQueue();
            tableModel_.NotifyAllPreviewStatusesChanged();
            ClearPreview();
            UpdateActionState();
            SetBusy(false);
            UpdateGalleryActivity();
            ScheduleGalleryPreviewSearch();
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
            const QModelIndex selectedIndex = SelectedProxyIndex();

            const QModelIndex sourceIndex =
                proxyModel_->mapToSource(selectedIndex);

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
            viewToggleButton_->setEnabled(!busy_ && database_.IsOpen());
        }

        void SetBusy(bool busy, const QString& message = {})
        {
            busy_ = busy;
            openButton_->setEnabled(!busy);
            browseButton_->setEnabled(!busy);
            table_->setEnabled(!busy);
            galleryView_->setEnabled(!busy);
            UpdateActionState();
            UpdateGalleryActivity();

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
            galleryView_->setCurrentIndex(proxyIndex);
            galleryView_->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
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
            PreviewRequestKind requestKind)
        {
            if (previewWorkerActive_ || !database_.IsOpen())
            {
                return;
            }

            const std::uint64_t requestId = nextPreviewRequestId_++;
            const fs::path cacheDirectory = database_.CacheDirectory();

            previewWorkerActive_ = true;
            activePreviewRequestId_ = requestId;
            activePreviewRequestKind_ = requestKind;
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
            const PreviewRequestKind requestKind = activePreviewRequestKind_;
            activePreviewRequestKind_ = PreviewRequestKind::Manual;

            if (!result.succeeded)
            {
                previewCache_.SetUnavailable(
                    result.entry,
                    ToQString(result.message));
                tableModel_.NotifyPreviewStatusChanged(result.entry.cacheIndex);

                if (requestKind == PreviewRequestKind::Manual)
                {
                    ClearPreview();
                    statusLabel_->setText(
                        QStringLiteral("Preview unavailable: ")
                        + ToQString(result.message));
                }

                if (requestKind == PreviewRequestKind::TryNext)
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

            QPixmap pixmap;
            QString imageError;

            if (!StoreDecodedPreview(result.entry, result.image, pixmap, imageError))
            {
                if (requestKind == PreviewRequestKind::Manual)
                {
                    ClearPreview();
                    statusLabel_->setText(imageError);
                }

                if (requestKind == PreviewRequestKind::TryNext)
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

            previewPixmap_ = pixmap;
            ShowPreviewPixmap();
            SelectEntry(result.entry);
            tryNextActive_ = false;
            statusLabel_->setText(
                QStringLiteral("Preview ready: %1 (%2 x %3)")
                    .arg(ToQString(result.entry.uuid.ToString()))
                    .arg(result.image.width)
                    .arg(result.image.height));
            UpdateActionState();
        }

        void StartGalleryPreviewRequest(const CacheEntry& entry)
        {
            if (galleryPreviewWorkerActive_ || !database_.IsOpen())
            {
                return;
            }

            const std::uint64_t requestId = nextPreviewRequestId_++;
            const fs::path cacheDirectory = database_.CacheDirectory();

            galleryPreviewWorkerActive_ = true;
            activeGalleryPreviewRequestId_ = requestId;
            previewCache_.SetChecking(entry);
            tableModel_.NotifyPreviewStatusChanged(entry.cacheIndex);
            UpdateGalleryActivity();

            galleryPreviewFuture_ = std::async(
                std::launch::async,
                [requestId, cacheDirectory, entry]()
                {
                    return DecodePreview(requestId, cacheDirectory, entry);
                });

            if (!galleryPreviewPollTimer_->isActive())
            {
                galleryPreviewPollTimer_->start();
            }
        }

        void PollGalleryPreviewWorker()
        {
            if (!galleryPreviewWorkerActive_)
            {
                galleryPreviewPollTimer_->stop();
                return;
            }

            if (galleryPreviewFuture_.wait_for(std::chrono::seconds(0)) !=
                std::future_status::ready)
            {
                return;
            }

            PreviewDecodeResult result = galleryPreviewFuture_.get();
            galleryPreviewWorkerActive_ = false;
            galleryPreviewPollTimer_->stop();
            UpdateGalleryActivity();

            if (result.requestId != activeGalleryPreviewRequestId_)
            {
                ScheduleGalleryPreviewSearch();
                return;
            }

            galleryPreviewQueue_.MarkCompleted();
            ApplyGalleryPreviewResult(result);
        }

        void ApplyGalleryPreviewResult(const PreviewDecodeResult& result)
        {
            if (!result.succeeded)
            {
                previewCache_.SetUnavailable(
                    result.entry,
                    ToQString(result.message));
                tableModel_.NotifyPreviewStatusChanged(result.entry.cacheIndex);
                StartNextQueuedGalleryPreview();
                return;
            }

            QPixmap pixmap;
            QString imageError;

            if (!StoreDecodedPreview(result.entry, result.image, pixmap, imageError))
            {
                StartNextQueuedGalleryPreview();
                return;
            }

            const CacheEntry* selectedEntry = SelectedEntry();

            if (selectedEntry != nullptr &&
                selectedEntry->cacheIndex == result.entry.cacheIndex)
            {
                previewPixmap_ = pixmap;
                ShowPreviewPixmap();
            }

            StartNextQueuedGalleryPreview();
        }

        bool StoreDecodedPreview(
            const CacheEntry& entry,
            const DecodedImage& decodedImage,
            QPixmap& pixmap,
            QString& errorMessage)
        {
            if (!CreatePreviewPixmap(decodedImage, pixmap, errorMessage))
            {
                previewCache_.SetLoadFailed(entry, errorMessage);
                tableModel_.NotifyPreviewStatusChanged(entry.cacheIndex);
                return false;
            }

            previewCache_.SetPreviewable(
                entry,
                pixmap,
                decodedImage.width,
                decodedImage.height);
            tableModel_.NotifyPreviewStatusChanged(entry.cacheIndex);
            return true;
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

            StartPreviewRequest(*entry, PreviewRequestKind::Manual);
        }

        void StartTryNextPreview()
        {
            if (!database_.IsOpen() || tryNextActive_)
            {
                return;
            }

            const QModelIndex selectedIndex = SelectedProxyIndex();
            tryNextProxyRow_ = 0;

            if (selectedIndex.isValid())
            {
                tryNextProxyRow_ = selectedIndex.row() + 1;
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

            StartPreviewRequest(*entry, PreviewRequestKind::TryNext);
        }

        QModelIndex SelectedProxyIndex() const
        {
            if (galleryMode_)
            {
                const QModelIndex galleryIndex = galleryView_->currentIndex();

                if (galleryIndex.isValid())
                {
                    return galleryIndex.sibling(galleryIndex.row(), 0);
                }
            }

            const QModelIndexList selectedRows =
                table_->selectionModel()->selectedRows();

            if (!selectedRows.empty())
            {
                return selectedRows.front().sibling(selectedRows.front().row(), 0);
            }

            const QModelIndex galleryIndex = galleryView_->currentIndex();

            if (galleryIndex.isValid())
            {
                return galleryIndex.sibling(galleryIndex.row(), 0);
            }

            return {};
        }

        void ToggleViewMode()
        {
            const QModelIndex selectedIndex = SelectedProxyIndex();
            galleryMode_ = !galleryMode_;
            if (!galleryMode_)
            {
                ClearGalleryPreviewQueue();
            }
            viewStack_->setCurrentWidget(
                galleryMode_
                    ? static_cast<QWidget*>(galleryView_)
                    : static_cast<QWidget*>(table_));
            viewToggleButton_->setText(
                galleryMode_ ? QStringLiteral("Table") : QStringLiteral("Gallery"));
            SyncActiveViewSelection(selectedIndex);
            UpdateActionState();
            ShowCachedPreviewForSelection();
            UpdateGalleryActivity();
            ScheduleGalleryPreviewSearch();
        }

        void SyncActiveViewSelection(const QModelIndex& selectedIndex)
        {
            if (!selectedIndex.isValid())
            {
                return;
            }

            if (galleryMode_)
            {
                galleryView_->setCurrentIndex(selectedIndex);
                galleryView_->scrollTo(selectedIndex, QAbstractItemView::PositionAtCenter);
                return;
            }

            table_->selectRow(selectedIndex.row());
            table_->scrollTo(selectedIndex, QAbstractItemView::PositionAtCenter);
        }

        void ScheduleGalleryPreviewSearch()
        {
            if (!galleryMode_ || !database_.IsOpen() || galleryPreviewSearchPending_)
            {
                return;
            }

            galleryPreviewSearchPending_ = true;
            UpdateGalleryActivity();
            QTimer::singleShot(
                75,
                this,
                [this]()
                {
                    if (!galleryMode_ || !database_.IsOpen())
                    {
                        galleryPreviewSearchPending_ = false;
                        ClearGalleryPreviewQueue();
                        UpdateGalleryActivity();
                        return;
                    }

                    galleryPreviewSearchPending_ = false;

                    if (galleryPreviewWorkerActive_)
                    {
                        galleryPreviewQueue_.RequestRefresh();
                        UpdateGalleryActivity();
                        return;
                    }

                    UpdateGalleryActivity();
                    RebuildGalleryPreviewQueue();
                    StartNextQueuedGalleryPreview();
                });
        }

        void StartNextQueuedGalleryPreview()
        {
            if (!galleryMode_ ||
                !database_.IsOpen() ||
                busy_ ||
                galleryPreviewWorkerActive_ ||
                tryNextActive_)
            {
                return;
            }

            if (galleryPreviewQueue_.ConsumeRefreshRequest())
            {
                ScheduleGalleryPreviewSearch();
                return;
            }

            while (galleryPreviewQueue_.HasEntries())
            {
                const CacheEntry entry = galleryPreviewQueue_.TakeNext();

                if (!previewCache_.ShouldAttemptPreview(entry))
                {
                    galleryPreviewQueue_.MarkCompleted();
                    continue;
                }

                StartGalleryPreviewRequest(entry);
                return;
            }

            galleryPreviewQueue_.Replace({});
            UpdateGalleryActivity();
        }

        void RebuildGalleryPreviewQueue()
        {
            std::deque<CacheEntry> entries;

            const int rowCount = proxyModel_->rowCount();

            if (rowCount <= 0)
            {
                galleryPreviewQueue_.Replace(std::move(entries));
                return;
            }

            const int firstRow = FirstVisibleGalleryProxyRow(rowCount);

            constexpr int MaximumRowsToScan = 240;
            constexpr std::size_t MaximumQueueSize = 48;
            const int finalRow = std::min(rowCount, firstRow + MaximumRowsToScan);
            const QRect visibleArea = galleryView_->viewport()->rect().adjusted(0, -170, 0, 170);

            for (int proxyRow = firstRow; proxyRow < finalRow; ++proxyRow)
            {
                const QModelIndex proxyIndex = proxyModel_->index(proxyRow, 0);

                if (!proxyIndex.isValid() ||
                    !galleryView_->visualRect(proxyIndex).intersects(visibleArea))
                {
                    continue;
                }

                const QModelIndex sourceIndex = proxyModel_->mapToSource(proxyIndex);
                const CacheEntry* entry = tableModel_.EntryAt(sourceIndex.row());

                if (entry != nullptr && previewCache_.ShouldAttemptPreview(*entry))
                {
                    entries.push_back(*entry);

                    if (entries.size() >= MaximumQueueSize)
                    {
                        break;
                    }
                }
            }

            galleryPreviewQueue_.Replace(std::move(entries));
            UpdateGalleryActivity();
        }

        void ClearGalleryPreviewQueue()
        {
            galleryPreviewQueue_.Clear();
        }

        void UpdateGalleryActivity()
        {
            if (galleryActivityLabel_ == nullptr)
            {
                return;
            }

            const bool showLabel =
                galleryMode_ &&
                database_.IsOpen() &&
                (galleryPreviewWorkerActive_ ||
                    galleryPreviewSearchPending_ ||
                    galleryPreviewQueue_.RefreshPending() ||
                    galleryPreviewQueue_.HasEntries());

            galleryActivityLabel_->setVisible(showLabel);

            if (!showLabel)
            {
                galleryActivityLabel_->clear();
                return;
            }

            if (galleryPreviewSearchPending_)
            {
                galleryActivityLabel_->setText(QStringLiteral("Scanning visible items..."));
                return;
            }

            if (galleryPreviewQueue_.RefreshPending())
            {
                galleryActivityLabel_->setText(QStringLiteral("Refreshing thumbnails..."));
                return;
            }

            if (galleryPreviewQueue_.Total() > 0)
            {
                const std::size_t current =
                    std::min(
                        galleryPreviewQueue_.Total(),
                        galleryPreviewQueue_.Completed() +
                            (galleryPreviewWorkerActive_ ? 1u : 0u));
                galleryActivityLabel_->setText(
                    QStringLiteral("Checking thumbnails %1 / %2")
                        .arg(static_cast<qulonglong>(current))
                        .arg(static_cast<qulonglong>(galleryPreviewQueue_.Total())));
                return;
            }

            galleryActivityLabel_->setText(QStringLiteral("Checking thumbnails..."));
        }

        int FirstVisibleGalleryProxyRow(int rowCount) const
        {
            const int width = galleryView_->viewport()->width();
            const int height = galleryView_->viewport()->height();

            if (width > 0 && height > 0)
            {
                const int xPoints[] = {0, width / 4, width / 2, (width * 3) / 4, width - 1};
                const int yPoints[] = {0, height / 4, height / 2, (height * 3) / 4, height - 1};
                int bestRow = rowCount;

                for (const int y : yPoints)
                {
                    for (const int x : xPoints)
                    {
                        const QModelIndex index = galleryView_->indexAt(QPoint(x, y));

                        if (index.isValid())
                        {
                            bestRow = std::min(bestRow, index.row());
                        }
                    }
                }

                if (bestRow != rowCount)
                {
                    return bestRow;
                }
            }

            if (galleryView_->currentIndex().isValid())
            {
                return galleryView_->currentIndex().row();
            }

            return 0;
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
        QPushButton* viewToggleButton_ = nullptr;
        QTableView* table_ = nullptr;
        QListView* galleryView_ = nullptr;
        QStackedWidget* viewStack_ = nullptr;
        QLabel* galleryActivityLabel_ = nullptr;
        QLabel* previewLabel_ = nullptr;
        QLabel* statusLabel_ = nullptr;
        PreviewCache previewCache_;
        CacheEntryTableModel tableModel_;
        QSortFilterProxyModel* proxyModel_ = nullptr;
        QTimer* previewPollTimer_ = nullptr;
        QTimer* galleryPreviewPollTimer_ = nullptr;
        QPixmap previewPixmap_;
        std::future<PreviewDecodeResult> previewFuture_;
        std::future<PreviewDecodeResult> galleryPreviewFuture_;
        GalleryPreviewQueue galleryPreviewQueue_;
        TextureCacheDatabase database_;
        bool busy_ = false;
        bool previewWorkerActive_ = false;
        bool galleryPreviewWorkerActive_ = false;
        PreviewRequestKind activePreviewRequestKind_ = PreviewRequestKind::Manual;
        bool tryNextActive_ = false;
        bool galleryMode_ = false;
        bool galleryPreviewSearchPending_ = false;
        int tryNextProxyRow_ = 0;
        int tryNextAttempts_ = 0;
        std::uint64_t nextPreviewRequestId_ = 1;
        std::uint64_t activePreviewRequestId_ = 0;
        std::uint64_t activeGalleryPreviewRequestId_ = 0;
    };
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

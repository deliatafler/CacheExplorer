#include "CacheEntryTableModel.h"
#include "GalleryActivityIndicator.h"
#include "GalleryListView.h"
#include "GalleryPreviewQueue.h"
#include "GalleryPreviewScanner.h"
#include "PreviewCache.h"
#include "PreviewDecodeWorker.h"
#include "PreviewImage.h"
#include "PreviewPanel.h"
#include "PreviewStatus.h"
#include "QtActionState.h"
#include "QtFileDialogs.h"
#include "QtHelpers.h"
#include "QtSelection.h"
#include "QtTextureExport.h"
#include "QtViewMode.h"
#include "TextureCacheDatabase.h"
#include "TryNextPreviewState.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <sstream>
#include <string>

#include <QApplication>
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
            galleryActivityIndicator_.SetLabel(galleryActivityLabel_);
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
            previewPanel_.SetLabel(previewLabel_);

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

            if (previewPanel_.HasPixmap())
            {
                previewPanel_.Refresh();
            }
        }

    private:
        void BrowseForCache()
        {
            const QString selectedDirectory =
                ChooseCacheDirectory(*this, pathEdit_->text());

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
                ClearPreviewUiState();
                SetBusy(false);
                statusLabel_->setText(
                    QStringLiteral("Could not open cache: ")
                    + QString::fromUtf8(CacheErrorMessage(result)));
                return;
            }

            pathEdit_->setText(PathToQString(database_.CacheDirectory()));
            PopulateTable();
            ClearPreviewUiState();
            SetBusy(false);
            ScheduleGalleryPreviewSearch();
        }

        void ClearPreviewUiState()
        {
            ClearGalleryPreviewQueue();
            ClearPreviewStatuses(previewCache_, tableModel_);
            previewPanel_.Clear();
        }

        void PopulateTable()
        {
            const auto& entries = database_.Entries();

            tableModel_.SetDatabase(&database_);
            table_->sortByColumn(4, Qt::DescendingOrder);

            const CacheHeader& header = database_.Header();
            statusLabel_->setText(
                LoadedCacheStatus(
                    entries.size(),
                    header.entryCount,
                    header.version));
        }

        const CacheEntry* SelectedEntry() const
        {
            return ::SelectedEntry(
                galleryMode_,
                *table_,
                *galleryView_,
                *proxyModel_,
                tableModel_);
        }

        void UpdateActionState()
        {
            ApplyMainActionState(
                MainActionState{
                    busy_,
                    SelectedEntry() != nullptr,
                    previewWorkerActive_,
                    tryNextPreview_.IsActive(),
                    database_.IsOpen()},
                *previewButton_,
                *tryNextButton_,
                *exportButton_,
                *viewToggleButton_);
        }

        void SetBusy(bool busy, const QString& message = {})
        {
            busy_ = busy;
            ApplyBusyState(
                busy_,
                *openButton_,
                *browseButton_,
                *pathEdit_,
                *table_,
                *galleryView_);
            UpdateActionState();
            UpdateGalleryActivity();

            if (!message.isEmpty())
            {
                statusLabel_->setText(message);
            }

        }

        void SelectEntry(const CacheEntry& entry)
        {
            ::SelectEntry(
                entry,
                tableModel_,
                *proxyModel_,
                *table_,
                *galleryView_);
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
            SetPreviewChecking(previewCache_, tableModel_, entry);
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
                SetPreviewUnavailable(
                    previewCache_,
                    tableModel_,
                    result.entry,
                    ToQString(result.message));

                if (requestKind == PreviewRequestKind::Manual)
                {
                    previewPanel_.Clear();
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
                    previewPanel_.Clear();
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

            previewPanel_.SetPixmap(pixmap);
            SelectEntry(result.entry);
            tryNextPreview_.Stop();
            statusLabel_->setText(
                PreviewReadyStatus(
                    result.entry.uuid.ToString(),
                    result.image.width,
                    result.image.height));
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
            SetPreviewChecking(previewCache_, tableModel_, entry);
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
                SetPreviewUnavailable(
                    previewCache_,
                    tableModel_,
                    result.entry,
                    ToQString(result.message));
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
                previewPanel_.SetPixmap(pixmap);
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
                SetPreviewLoadFailed(
                    previewCache_,
                    tableModel_,
                    entry,
                    errorMessage);
                return false;
            }

            SetPreviewable(
                previewCache_,
                tableModel_,
                entry,
                pixmap,
                decodedImage.width,
                decodedImage.height);
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

            previewPanel_.SetPixmap(record->pixmap);
            statusLabel_->setText(
                PreviewReadyStatus(
                    entry->uuid.ToString(),
                    record->width,
                    record->height));
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
            if (!database_.IsOpen() || tryNextPreview_.IsActive())
            {
                return;
            }

            const QModelIndex selectedIndex = SelectedProxyIndex();
            int firstProxyRow = 0;

            if (selectedIndex.isValid())
            {
                firstProxyRow = selectedIndex.row() + 1;
            }

            tryNextPreview_.Start(firstProxyRow);
            UpdateActionState();
            statusLabel_->setText(QStringLiteral("Looking for the next previewable texture..."));
            QTimer::singleShot(0, this, [this]() { ContinueTryNextPreview(); });
        }

        void ContinueTryNextPreview()
        {
            if (!tryNextPreview_.IsActive())
            {
                return;
            }

            if (tryNextPreview_.IsExhausted(proxyModel_->rowCount()))
            {
                tryNextPreview_.Stop();
                UpdateActionState();
                statusLabel_->setText(
                    QStringLiteral("No previewable texture found in the next %1 entries.")
                        .arg(tryNextPreview_.Attempts()));
                return;
            }

            const QModelIndex proxyIndex =
                proxyModel_->index(tryNextPreview_.TakeNextProxyRow(), 0);
            const QModelIndex sourceIndex =
                proxyModel_->mapToSource(proxyIndex);

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
            return ::SelectedProxyIndex(
                galleryMode_,
                *table_,
                *galleryView_);
        }

        void ToggleViewMode()
        {
            const QModelIndex selectedIndex = SelectedProxyIndex();
            galleryMode_ = ToggleGalleryMode(galleryMode_);
            if (!galleryMode_)
            {
                ClearGalleryPreviewQueue();
            }
            ApplyViewMode(
                galleryMode_,
                *viewStack_,
                *table_,
                *galleryView_,
                *viewToggleButton_);
            SyncActiveViewSelection(selectedIndex);
            UpdateActionState();
            ShowCachedPreviewForSelection();
            UpdateGalleryActivity();
            ScheduleGalleryPreviewSearch();
        }

        void SyncActiveViewSelection(const QModelIndex& selectedIndex)
        {
            ::SyncActiveViewSelection(
                galleryMode_,
                selectedIndex,
                *table_,
                *galleryView_);
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
                tryNextPreview_.IsActive())
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
            galleryPreviewQueue_.Replace(
                BuildVisibleGalleryPreviewQueue(
                    *galleryView_,
                    *proxyModel_,
                    tableModel_,
                    previewCache_));
            UpdateGalleryActivity();
        }

        void ClearGalleryPreviewQueue()
        {
            galleryPreviewQueue_.Clear();
        }

        void UpdateGalleryActivity()
        {
            galleryActivityIndicator_.Update(
                GalleryActivityState{
                    galleryMode_,
                    database_.IsOpen(),
                    galleryPreviewWorkerActive_,
                    galleryPreviewSearchPending_,
                    galleryPreviewQueue_.RefreshPending(),
                    galleryPreviewQueue_.HasEntries(),
                    galleryPreviewQueue_.Total(),
                    galleryPreviewQueue_.Completed()});
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
                ChoosePngOutputFile(*this, defaultName);

            if (outputFile.isEmpty())
            {
                return;
            }

            const TexturePngExportResult result =
                ExportTexturePng(
                    database_,
                    *entry,
                    PathFromQString(outputFile),
                    true);

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
        GalleryActivityIndicator galleryActivityIndicator_;
        PreviewPanel previewPanel_;
        PreviewCache previewCache_;
        CacheEntryTableModel tableModel_;
        QSortFilterProxyModel* proxyModel_ = nullptr;
        QTimer* previewPollTimer_ = nullptr;
        QTimer* galleryPreviewPollTimer_ = nullptr;
        std::future<PreviewDecodeResult> previewFuture_;
        std::future<PreviewDecodeResult> galleryPreviewFuture_;
        GalleryPreviewQueue galleryPreviewQueue_;
        TextureCacheDatabase database_;
        bool busy_ = false;
        bool previewWorkerActive_ = false;
        bool galleryPreviewWorkerActive_ = false;
        PreviewRequestKind activePreviewRequestKind_ = PreviewRequestKind::Manual;
        TryNextPreviewState tryNextPreview_;
        bool galleryMode_ = false;
        bool galleryPreviewSearchPending_ = false;
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

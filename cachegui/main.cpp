#include "CacheEntryTableModel.h"
#include "GalleryFilterProxyModel.h"
#include "GalleryActivityIndicator.h"
#include "GalleryListView.h"
#include "GalleryPreviewController.h"
#include "GalleryPreviewScanner.h"
#include "PreviewCache.h"
#include "PreviewDecodeWorker.h"
#include "PreviewPanel.h"
#include "PreviewStatus.h"
#include "PreviewWorkerState.h"
#include "QtAboutDialog.h"
#include "QtActionState.h"
#include "QtFileDialogs.h"
#include "QtGallerySort.h"
#include "QtGalleryStatus.h"
#include "QtHelpers.h"
#include "QtPreviewStateStore.h"
#include "QtSelection.h"
#include "QtTextureExport.h"
#include "QtTryNextPreview.h"
#include "QtViewMode.h"
#include "TextureCacheDatabase.h"
#include "TryNextPreviewState.h"
#include "UUID.h"

#include <cstdint>
#include <chrono>
#include <exception>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

namespace fs = std::filesystem;

namespace
{
    enum class PreviewRequestKind
    {
        TableSelection,
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
            CreateControls(root);
            ConfigureModels();
            ConfigureTable();
            ConfigureGallery();
            ConfigureSelectionModels();
            CreateLayout(root);
            ConnectSignals();

            ApplyViewMode(
                galleryMode_,
                *viewStack_,
                *table_,
                *galleryView_,
                *viewToggleButton_);
            tryNextButton_->setVisible(false);
            galleryFilterLabel_->setVisible(true);
            galleryFilterCombo_->setVisible(true);
            gallerySortLabel_->setVisible(true);
            gallerySortCombo_->setVisible(true);
            galleryCountLabel_->setVisible(true);

            setCentralWidget(root);
        }

        bool SmokeOpenCache(const QString& cachePath)
        {
            pathEdit_->setText(cachePath);
            OpenCache();

            return database_.IsOpen()
                && static_cast<std::size_t>(tableModel_.rowCount())
                    == database_.Entries().size();
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
        void CreateControls(QWidget* root)
        {
            pathLabel_ = new QLabel(QStringLiteral("Texture cache"), root);
            pathEdit_ = new QLineEdit(PreferredCachePath(), root);
            defaultCacheButton_ = new QToolButton(root);
            defaultCacheButton_->setIcon(
                style()->standardIcon(QStyle::SP_DirHomeIcon));
            defaultCacheButton_->setToolTip(
                QStringLiteral("Open the preferred viewer texture cache"));
            defaultCacheButton_->setVisible(DefaultCachePathExists());
            recentCacheButton_ = new QToolButton(root);
            recentCacheButton_->setText(QStringLiteral("Recent"));
            recentCacheButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
            recentCacheButton_->setPopupMode(QToolButton::InstantPopup);
            recentCacheMenu_ = new QMenu(recentCacheButton_);
            recentCacheButton_->setMenu(recentCacheMenu_);
            recentCacheButton_->setVisible(false);
            browseButton_ = new QPushButton(QStringLiteral("Choose Folder..."), root);
            openButton_ = new QPushButton(QStringLiteral("Open"), root);
            aboutButton_ = new QPushButton(QStringLiteral("About"), root);

            uuidLookupLabel_ = new QLabel(QStringLiteral("Find UUID"), root);
            uuidLookupEdit_ = new QLineEdit(root);
            uuidLookupEdit_->setPlaceholderText(
                QStringLiteral("xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"));
            uuidLookupEdit_->setClearButtonEnabled(true);
            uuidLookupEdit_->setMinimumWidth(290);
            findUuidButton_ = new QPushButton(QStringLiteral("Find"), root);

            tryNextButton_ = new QPushButton(QStringLiteral("Try Next Preview"), root);
            exportButton_ = new QPushButton(QStringLiteral("Export PNG"), root);
            viewToggleButton_ = new QPushButton(QStringLiteral("Gallery"), root);
            galleryFilterLabel_ = new QLabel(QStringLiteral("Show"), root);
            galleryFilterCombo_ = new QComboBox(root);
            ConfigureGalleryPreviewFilterControl(*galleryFilterCombo_);
            galleryFilterLabel_->hide();
            galleryFilterCombo_->hide();
            gallerySortLabel_ = new QLabel(QStringLiteral("Sort"), root);
            gallerySortCombo_ = new QComboBox(root);
            ConfigureGallerySortControl(*gallerySortCombo_);
            gallerySortLabel_->hide();
            gallerySortCombo_->hide();
            galleryCountLabel_ = new QLabel(root);
            galleryCountLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            galleryCountLabel_->setMinimumWidth(
                galleryCountLabel_->fontMetrics().horizontalAdvance(
                    QStringLiteral("999999 of 999999 entries")));
            galleryCountLabel_->setStyleSheet(QStringLiteral("QLabel { color: #666; }"));
            galleryCountLabel_->hide();
            galleryActivityLabel_ = new QLabel(root);
            galleryActivityLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            galleryActivityLabel_->setMinimumWidth(
                galleryActivityLabel_->fontMetrics().horizontalAdvance(
                    QStringLiteral("Refreshing visible thumbnails...")));
            galleryActivityLabel_->setStyleSheet(QStringLiteral("QLabel { color: #666; }"));
            galleryActivityLabel_->hide();
            galleryActivityIndicator_.SetLabel(galleryActivityLabel_);
            tryNextButton_->setEnabled(false);
            exportButton_->setEnabled(false);
            viewToggleButton_->setEnabled(false);
            uuidLookupEdit_->setEnabled(false);
            findUuidButton_->setEnabled(false);

            RefreshRecentCacheMenu();

            table_ = new QTableView(root);
            galleryView_ = new GalleryListView(root);
            viewStack_ = new QStackedWidget(root);

            previewPollTimer_ = new QTimer(root);
            previewPollTimer_->setInterval(50);
            tableSelectionPreviewTimer_ = new QTimer(root);
            tableSelectionPreviewTimer_->setSingleShot(true);
            tableSelectionPreviewTimer_->setInterval(125);
            galleryPreviewPollTimer_ = new QTimer(root);
            galleryPreviewPollTimer_->setInterval(50);
            batchExportPollTimer_ = new QTimer(root);
            batchExportPollTimer_->setInterval(100);

            previewLabel_ = new QLabel(QStringLiteral("No preview selected."), root);
            previewLabel_->setAlignment(Qt::AlignCenter);
            previewLabel_->setMinimumSize(320, 320);
            previewPanel_.SetLabel(previewLabel_);
            previewPanel_.Clear();

            statusLabel_ = new QLabel(
                QStringLiteral("Choose a Second Life viewer texture cache folder."),
                root);
        }

        void ConfigureModels()
        {
            proxyModel_ = new GalleryFilterProxyModel(this);
            tableModel_.SetPreviewCache(&previewCache_);
            proxyModel_->setSourceModel(&tableModel_);
            proxyModel_->setSortRole(Qt::UserRole);
            proxyModel_->setDynamicSortFilter(false);
        }

        void ConfigureTable()
        {
            table_->setModel(proxyModel_);
            table_->setSelectionBehavior(QAbstractItemView::SelectRows);
            table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
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
        }

        void ConfigureGallery()
        {
            galleryView_->setModel(proxyModel_);
            galleryView_->setModelColumn(0);
            galleryView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
            galleryView_->setViewMode(QListView::IconMode);
            galleryView_->setResizeMode(QListView::Adjust);
            galleryView_->setMovement(QListView::Static);
            galleryView_->setIconSize(QSize(128, 128));
            galleryView_->setGridSize(QSize(196, 174));
            galleryView_->setUniformItemSizes(true);
            galleryView_->setWordWrap(false);
            galleryView_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        }

        void ConfigureSelectionModels()
        {
            selectionModel_ = new QItemSelectionModel(proxyModel_, this);
            table_->setSelectionModel(selectionModel_);
            galleryView_->setSelectionModel(selectionModel_);
        }

        void CreateLayout(QWidget* root)
        {
            auto* layout = new QVBoxLayout(root);

            auto* pathLayout = new QGridLayout();
            pathLayout->addWidget(pathLabel_, 0, 0);
            pathLayout->addWidget(pathEdit_, 0, 1);
            pathLayout->addWidget(defaultCacheButton_, 0, 2);
            pathLayout->addWidget(recentCacheButton_, 0, 3);
            pathLayout->addWidget(browseButton_, 0, 4);
            pathLayout->addWidget(openButton_, 0, 5);
            pathLayout->addWidget(aboutButton_, 0, 6);
            pathLayout->setColumnStretch(1, 1);

            auto* lookupLayout = new QHBoxLayout();
            lookupLayout->addWidget(uuidLookupLabel_);
            lookupLayout->addWidget(uuidLookupEdit_);
            lookupLayout->addWidget(findUuidButton_);
            lookupLayout->addStretch(1);

            auto* actionLayout = new QHBoxLayout();
            actionLayout->addWidget(tryNextButton_);
            actionLayout->addWidget(exportButton_);
            actionLayout->addStretch(1);
            actionLayout->addWidget(galleryFilterLabel_);
            actionLayout->addWidget(galleryFilterCombo_);
            actionLayout->addWidget(gallerySortLabel_);
            actionLayout->addWidget(gallerySortCombo_);
            actionLayout->addWidget(galleryCountLabel_);
            actionLayout->addWidget(galleryActivityLabel_);
            actionLayout->addWidget(viewToggleButton_);

            viewStack_->addWidget(table_);
            viewStack_->addWidget(galleryView_);

            auto* contentLayout = new QHBoxLayout();
            contentLayout->addWidget(viewStack_, 3);
            contentLayout->addWidget(previewLabel_, 1);

            layout->addLayout(pathLayout);
            layout->addLayout(lookupLayout);
            layout->addLayout(actionLayout);
            layout->addLayout(contentLayout, 1);
            layout->addWidget(statusLabel_);
        }

        void ConnectSignals()
        {
            connect(
                browseButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    BrowseForCache();
                });

            connect(
                defaultCacheButton_,
                &QToolButton::clicked,
                this,
                [this]()
                {
                    OpenDefaultCache();
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
                pathEdit_,
                &QLineEdit::textChanged,
                this,
                [this](const QString&)
                {
                    UpdateOpenButtonText();
                });

            connect(
                aboutButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    ShowAboutDialog(*this, database_);
                });

            connect(
                uuidLookupEdit_,
                &QLineEdit::returnPressed,
                this,
                [this]()
                {
                    FindUuid();
                });

            connect(
                uuidLookupEdit_,
                &QLineEdit::textChanged,
                this,
                [this](const QString&)
                {
                    UpdateActionState();
                });

            connect(
                findUuidButton_,
                &QPushButton::clicked,
                this,
                [this]()
                {
                    FindUuid();
                });

            connect(
                selectionModel_,
                &QItemSelectionModel::selectionChanged,
                this,
                [this]()
                {
                    UpdateActionState();
                    ShowCachedPreviewForSelection();
                    ScheduleTableSelectionPreview();
                });

            connect(
                selectionModel_,
                &QItemSelectionModel::currentChanged,
                this,
                [this](const QModelIndex&, const QModelIndex&)
                {
                    UpdateActionState();
                    ShowCachedPreviewForSelection();
                    ScheduleTableSelectionPreview();
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
                gallerySortCombo_,
                &QComboBox::currentIndexChanged,
                this,
                [this]()
                {
                    const GallerySortMode sortMode =
                        CurrentGallerySortMode(*gallerySortCombo_);
                    statusLabel_->setText(GallerySortInProgressStatus(sortMode));
                    QApplication::processEvents();

                    ApplyGallerySort(
                        *proxyModel_,
                        sortMode);
                    UpdateGalleryEntryCount();
                    statusLabel_->setText(GallerySortCompleteStatus(sortMode));
                    ClearGalleryPreviewQueue();
                    UpdateGalleryActivity();
                    ScheduleGalleryPreviewSearch();
                });

            connect(
                galleryFilterCombo_,
                &QComboBox::currentIndexChanged,
                this,
                [this]()
                {
                    proxyModel_->SetPreviewFilter(
                        CurrentGalleryPreviewFilter(*galleryFilterCombo_));
                    UpdateGalleryEntryCount();
                    ClearGalleryPreviewQueue();
                    UpdateGalleryActivity();
                    statusLabel_->setText(
                        GalleryFilterUpdatedStatus(
                            proxyModel_->rowCount(),
                            tableModel_.rowCount()));
                    ScheduleGalleryPreviewSearch();
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
                tableSelectionPreviewTimer_,
                &QTimer::timeout,
                this,
                [this]()
                {
                    StartSelectedTablePreview();
                });

            connect(
                galleryPreviewPollTimer_,
                &QTimer::timeout,
                this,
                [this]()
                {
                    PollGalleryPreviewWorker();
                });

            connect(
                batchExportPollTimer_,
                &QTimer::timeout,
                this,
                [this]()
                {
                    PollBatchPngExport();
                });
        }
        void BrowseForCache()
        {
            const QString selectedDirectory =
                ChooseCacheDirectory(*this, pathEdit_->text());

            if (!selectedDirectory.isEmpty())
            {
                pathEdit_->setText(selectedDirectory);
                OpenCache();
            }
        }

        void OpenDefaultCache()
        {
            const QString defaultPath = DefaultCachePath();

            if (defaultPath.isEmpty())
            {
                return;
            }

            pathEdit_->setText(defaultPath);
            OpenCache();
        }

        void OpenCache()
        {
            const fs::path cacheDirectory =
                ResolveTextureCacheDirectory(PathFromQString(pathEdit_->text()));

            SetBusy(true, QStringLiteral("Opening cache..."));

            const CacheError result = database_.Open(cacheDirectory);

            if (result != CacheError::None)
            {
                HandleOpenCacheFailure(result);
                return;
            }

            HandleOpenCacheSuccess();
        }

        void HandleOpenCacheFailure(CacheError result)
        {
            tableModel_.SetDatabase(nullptr);
            previewStateFile_.clear();
            ClearPreviewUiState();
            UpdateGalleryEntryCount();
            SetBusy(false);
            UpdateOpenButtonText();
            statusLabel_->setText(
                QStringLiteral("Could not open cache: ")
                + QString::fromUtf8(CacheErrorMessage(result)));
        }

        void HandleOpenCacheSuccess()
        {
            pathEdit_->setText(PathToQString(database_.CacheDirectory()));
            RememberOpenedCachePath(pathEdit_->text());
            RefreshRecentCacheMenu();
            PopulateTable();
            ClearPreviewUiState();
            LoadPersistentPreviewState();
            UpdateGalleryEntryCount();
            SetBusy(false);
            UpdateOpenButtonText();
            ScheduleGalleryPreviewSearch();
        }

        void UpdateOpenButtonText()
        {
            const fs::path suppliedPath =
                ResolveTextureCacheDirectory(PathFromQString(pathEdit_->text()));
            const bool reopeningCurrentCache =
                database_.IsOpen() &&
                IsSameCachePath(suppliedPath, database_.CacheDirectory());

            openButton_->setText(
                reopeningCurrentCache
                    ? QStringLiteral("Refresh")
                    : QStringLiteral("Open"));
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

        std::vector<const CacheEntry*> SelectedEntries() const
        {
            return ::SelectedEntries(*table_, *proxyModel_, tableModel_);
        }

        void UpdateActionState()
        {
            const std::vector<const CacheEntry*> selectedEntries = SelectedEntries();

            ApplyMainActionState(
                MainActionState{
                    busy_,
                    !selectedEntries.empty(),
                    previewWorker_.IsActive(),
                    tryNextPreview_.IsActive(),
                    database_.IsOpen(),
                    galleryMode_},
                *tryNextButton_,
                *exportButton_,
                *viewToggleButton_);

            exportButton_->setText(
                selectedEntries.size() > 1
                    ? QStringLiteral("Export Selected PNGs...")
                    : QStringLiteral("Export PNG"));

            const bool lookupAvailable = !busy_ && database_.IsOpen();
            uuidLookupEdit_->setEnabled(lookupAvailable);
            findUuidButton_->setEnabled(
                lookupAvailable && !uuidLookupEdit_->text().trimmed().isEmpty());
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
            defaultCacheButton_->setEnabled(!busy_);
            recentCacheButton_->setEnabled(!busy_);
            galleryFilterCombo_->setEnabled(!busy_);
            gallerySortCombo_->setEnabled(!busy_);
            UpdateActionState();
            UpdateGalleryActivity();

            if (!message.isEmpty())
            {
                statusLabel_->setText(message);
            }

        }

        bool SelectEntry(const CacheEntry& entry)
        {
            return ::SelectEntry(
                entry,
                tableModel_,
                *proxyModel_,
                *table_,
                *galleryView_);
        }

        void RefreshRecentCacheMenu()
        {
            const QStringList recentPaths = RecentCachePaths();
            recentCacheMenu_->clear();

            for (const QString& recentPath : recentPaths)
            {
                QAction* action = recentCacheMenu_->addAction(recentPath);

                connect(
                    action,
                    &QAction::triggered,
                    this,
                    [this, recentPath]()
                    {
                        pathEdit_->setText(recentPath);
                        OpenCache();
                    });
            }

            if (!recentPaths.isEmpty())
            {
                recentCacheMenu_->addSeparator();
                QAction* clearAction = recentCacheMenu_->addAction(
                    QStringLiteral("Clear recent caches"));

                connect(
                    clearAction,
                    &QAction::triggered,
                    this,
                    [this]()
                    {
                        ClearRecentCachePaths();
                        RefreshRecentCacheMenu();
                        statusLabel_->setText(
                            QStringLiteral("Cleared recent cache locations."));
                    });
            }

            recentCacheButton_->setVisible(!recentPaths.isEmpty());
            recentCacheButton_->setEnabled(!busy_ && !recentPaths.isEmpty());
        }

        void FindUuid()
        {
            if (!database_.IsOpen() || busy_)
            {
                return;
            }

            const QByteArray uuidText =
                uuidLookupEdit_->text().trimmed().toUtf8();
            const std::optional<UUID> uuid = UUID::FromString(
                std::string(
                    uuidText.constData(),
                    static_cast<std::size_t>(uuidText.size())));

            if (!uuid.has_value())
            {
                statusLabel_->setText(
                    QStringLiteral("Enter a valid texture UUID."));
                uuidLookupEdit_->setFocus();
                return;
            }

            const CacheEntry* entry = database_.Find(*uuid);

            if (entry == nullptr)
            {
                statusLabel_->setText(
                    QStringLiteral("UUID was not found in this cache: %1")
                        .arg(ToQString(uuid->ToString())));
                return;
            }

            bool filterCleared = false;

            if (!SelectEntry(*entry) && galleryMode_)
            {
                const int allFilterIndex = galleryFilterCombo_->findData(
                    static_cast<int>(GalleryPreviewFilter::All));

                if (allFilterIndex >= 0)
                {
                    galleryFilterCombo_->setCurrentIndex(allFilterIndex);
                    filterCleared = true;
                }

                SelectEntry(*entry);
            }

            statusLabel_->setText(
                filterCleared
                    ? QStringLiteral("Found texture and cleared the Gallery filter: %1")
                        .arg(ToQString(entry->uuid.ToString()))
                    : QStringLiteral("Found texture: %1")
                        .arg(ToQString(entry->uuid.ToString())));
            ScheduleGalleryPreviewSearch();
        }

        void StartPreviewRequest(
            const CacheEntry& entry,
            PreviewRequestKind requestKind)
        {
            if (previewWorker_.IsActive() || !database_.IsOpen())
            {
                return;
            }

            const std::uint64_t requestId = nextPreviewRequestId_++;
            const fs::path cacheDirectory = database_.CacheDirectory();

            activePreviewRequestKind_ = requestKind;
            SetPreviewChecking(previewCache_, tableModel_, entry);
            UpdateActionState();

            previewWorker_.StartDecode(
                requestId,
                cacheDirectory,
                entry);

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
            if (!previewWorker_.IsActive())
            {
                previewPollTimer_->stop();
                return;
            }

            if (!previewWorker_.IsReady())
            {
                return;
            }

            const std::uint64_t activeRequestId =
                previewWorker_.ActiveRequestId();
            PreviewDecodeResult result = previewWorker_.TakeResult();
            previewPollTimer_->stop();

            if (result.requestId != activeRequestId)
            {
                UpdateActionState();
                return;
            }

            ApplyPreviewResult(result);
        }

        void ApplyPreviewResult(const PreviewDecodeResult& result)
        {
            const PreviewRequestKind requestKind = activePreviewRequestKind_;
            activePreviewRequestKind_ = PreviewRequestKind::TableSelection;
            const CacheEntry* selectedEntry = SelectedEntry();
            const bool resultIsSelected =
                selectedEntry != nullptr &&
                selectedEntry->cacheIndex == result.entry.cacheIndex;

            if (!result.succeeded)
            {
                SetPreviewUnavailable(
                    previewCache_,
                    tableModel_,
                    result.entry,
                    ToQString(result.message));
                SavePersistentPreviewState();

                if (requestKind == PreviewRequestKind::TableSelection &&
                    resultIsSelected)
                {
                    previewPanel_.SetMessage(
                        PreviewUnavailablePanelMessage(result.status));
                    statusLabel_->setText(
                        PreviewUnavailableStatus(
                            result.status,
                            result.message));
                }

                FinishFailedPreviewAttempt(requestKind);
                return;
            }

            QPixmap pixmap;
            QString imageError;

            if (!StoreDecodedPreview(
                    previewCache_,
                    tableModel_,
                    result.entry,
                    result.image,
                    pixmap,
                    imageError))
            {
                SavePersistentPreviewState();
                if (requestKind == PreviewRequestKind::TableSelection &&
                    resultIsSelected)
                {
                    previewPanel_.SetMessage(imageError);
                    statusLabel_->setText(imageError);
                }

                FinishFailedPreviewAttempt(requestKind);
                return;
            }

            SavePersistentPreviewState();

            if (requestKind == PreviewRequestKind::TryNext)
            {
                previewPanel_.SetPixmap(pixmap);
                SelectEntry(result.entry);
                tryNextPreview_.Stop();
                statusLabel_->setText(
                    PreviewReadyStatus(
                        result.entry.uuid.ToString(),
                        result.image.width,
                        result.image.height));
            }
            else if (resultIsSelected)
            {
                previewPanel_.SetPixmap(pixmap);
                statusLabel_->setText(
                    PreviewReadyStatus(
                        result.entry.uuid.ToString(),
                        result.image.width,
                        result.image.height));
            }

            UpdateActionState();
            ScheduleTableSelectionPreview();
        }

        void FinishFailedPreviewAttempt(PreviewRequestKind requestKind)
        {
            if (requestKind == PreviewRequestKind::TryNext)
            {
                ScheduleTryNextPreviewContinuation();
                return;
            }

            UpdateActionState();
            ScheduleTableSelectionPreview();
        }

        void ScheduleTryNextPreviewContinuation()
        {
            QTimer::singleShot(
                0,
                this,
                [this]()
                {
                    ContinueTryNextPreview();
                });
        }

        void StartGalleryPreviewRequest(const CacheEntry& entry)
        {
            if (galleryPreviewWorker_.IsActive() || !database_.IsOpen())
            {
                return;
            }

            const std::uint64_t requestId = nextPreviewRequestId_++;
            const fs::path cacheDirectory = database_.CacheDirectory();

            SetPreviewChecking(previewCache_, tableModel_, entry);
            UpdateGalleryActivity();

            galleryPreviewWorker_.StartDecode(
                requestId,
                cacheDirectory,
                entry);

            if (!galleryPreviewPollTimer_->isActive())
            {
                galleryPreviewPollTimer_->start();
            }
        }

        void PollGalleryPreviewWorker()
        {
            if (!galleryPreviewWorker_.IsActive())
            {
                galleryPreviewPollTimer_->stop();
                return;
            }

            if (!galleryPreviewWorker_.IsReady())
            {
                return;
            }

            const std::uint64_t activeRequestId =
                galleryPreviewWorker_.ActiveRequestId();
            PreviewDecodeResult result = galleryPreviewWorker_.TakeResult();
            galleryPreviewPollTimer_->stop();
            UpdateGalleryActivity();

            if (result.requestId != activeRequestId)
            {
                ScheduleGalleryPreviewSearch();
                return;
            }

            galleryPreviewController_.MarkCompleted();
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
                SavePersistentPreviewState();
                StartNextQueuedGalleryPreview();
                return;
            }

            QPixmap pixmap;
            QString imageError;

            if (!StoreDecodedPreview(
                    previewCache_,
                    tableModel_,
                    result.entry,
                    result.image,
                    pixmap,
                    imageError))
            {
                SavePersistentPreviewState();
                StartNextQueuedGalleryPreview();
                return;
            }

            SavePersistentPreviewState();
            const CacheEntry* selectedEntry = SelectedEntry();

            if (selectedEntry != nullptr &&
                selectedEntry->cacheIndex == result.entry.cacheIndex)
            {
                previewPanel_.SetPixmap(pixmap);
            }

            StartNextQueuedGalleryPreview();
        }

        void ShowCachedPreviewForSelection()
        {
            const CacheEntry* entry = SelectedEntry();

            if (entry == nullptr)
            {
                return;
            }

            const CachedSelectionPreview preview =
                CachedPreviewForSelection(*entry, previewCache_);

            if (!preview.available)
            {
                if (!preview.panelMessage.isEmpty())
                {
                    if (preview.panelMessageIsError)
                    {
                        previewPanel_.SetMessage(preview.panelMessage);
                    }
                    else
                    {
                        previewPanel_.SetNotice(preview.panelMessage);
                    }
                }

                if (!preview.statusText.isEmpty())
                {
                    statusLabel_->setText(preview.statusText);
                }
                return;
            }

            previewPanel_.SetPixmap(preview.pixmap);
            statusLabel_->setText(preview.statusText);
        }

        void ScheduleTableSelectionPreview()
        {
            if (galleryMode_ ||
                !database_.IsOpen() ||
                tryNextPreview_.IsActive())
            {
                return;
            }

            const CacheEntry* entry = SelectedEntry();

            if (entry != nullptr && previewCache_.ShouldAttemptPreview(*entry))
            {
                tableSelectionPreviewTimer_->start();
            }
        }

        void StartSelectedTablePreview()
        {
            if (galleryMode_ || !database_.IsOpen() || tryNextPreview_.IsActive())
            {
                return;
            }

            if (previewWorker_.IsActive())
            {
                tableSelectionPreviewTimer_->start();
                return;
            }

            const CacheEntry* entry = SelectedEntry();

            if (entry != nullptr && previewCache_.ShouldAttemptPreview(*entry))
            {
                StartPreviewRequest(*entry, PreviewRequestKind::TableSelection);
            }
        }

        void StartTryNextPreview()
        {
            if (!database_.IsOpen() || tryNextPreview_.IsActive())
            {
                return;
            }

            tryNextPreview_.Start(
                FirstTryNextProxyRow(SelectedProxyIndex()));
            UpdateActionState();
            statusLabel_->setText(TryNextSearchStartedStatus());
            ScheduleTryNextPreviewContinuation();
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
                    TryNextSearchExhaustedStatus(tryNextPreview_.Attempts()));
                return;
            }

            const CacheEntry* entry =
                TakeNextTryNextEntry(
                    tryNextPreview_,
                    *proxyModel_,
                    tableModel_);

            if (entry == nullptr)
            {
                ScheduleTryNextPreviewContinuation();
                return;
            }

            statusLabel_->setText(TryingPreviewStatus(*entry));

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
            tableSelectionPreviewTimer_->stop();
            tryNextButton_->setVisible(!galleryMode_);
            galleryFilterLabel_->setVisible(galleryMode_);
            galleryFilterCombo_->setVisible(galleryMode_);
            gallerySortLabel_->setVisible(galleryMode_);
            gallerySortCombo_->setVisible(galleryMode_);
            galleryCountLabel_->setVisible(galleryMode_);
            proxyModel_->SetGalleryMode(galleryMode_);
            ApplyViewMode(
                galleryMode_,
                *viewStack_,
                *table_,
                *galleryView_,
                *viewToggleButton_);
            SyncActiveViewSelection(selectedIndex);
            if (galleryMode_)
            {
                ApplyGallerySort(
                    *proxyModel_,
                    CurrentGallerySortMode(*gallerySortCombo_));
            }
            UpdateActionState();
            ShowCachedPreviewForSelection();
            UpdateGalleryEntryCount();
            UpdateGalleryActivity();
            ScheduleGalleryPreviewSearch();
            ScheduleTableSelectionPreview();
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
            if (!galleryPreviewController_.CanScheduleSearch(
                    galleryMode_,
                    database_.IsOpen()))
            {
                return;
            }

            galleryPreviewController_.BeginScheduledSearch();
            UpdateGalleryActivity();
            QTimer::singleShot(
                75,
                this,
                [this]()
                {
                    const GallerySearchAction action =
                        galleryPreviewController_.FinishScheduledSearch(
                            galleryMode_,
                            database_.IsOpen(),
                            galleryPreviewWorker_.IsActive());

                    if (action == GallerySearchAction::Clear)
                    {
                        UpdateGalleryActivity();
                        return;
                    }

                    if (action == GallerySearchAction::Refresh)
                    {
                        UpdateGalleryActivity();
                        return;
                    }

                    if (action == GallerySearchAction::Rebuild)
                    {
                        UpdateGalleryActivity();
                        RebuildGalleryPreviewQueue();
                        StartNextQueuedGalleryPreview();
                    }
                });
        }

        void StartNextQueuedGalleryPreview()
        {
            if (!galleryPreviewController_.CanStartQueuedPreview(
                    galleryMode_,
                    database_.IsOpen(),
                    busy_,
                    galleryPreviewWorker_.IsActive(),
                    tryNextPreview_.IsActive()))
            {
                return;
            }

            if (galleryPreviewController_.ConsumeRefreshRequest())
            {
                ScheduleGalleryPreviewSearch();
                return;
            }

            const std::optional<CacheEntry> entry =
                galleryPreviewController_.TakeNextAttemptableEntry(previewCache_);

            if (entry.has_value())
            {
                StartGalleryPreviewRequest(*entry);
                return;
            }

            UpdateGalleryActivity();
        }

        void RebuildGalleryPreviewQueue()
        {
            galleryPreviewController_.ReplaceQueue(
                BuildVisibleGalleryPreviewQueue(
                    *galleryView_,
                    *proxyModel_,
                    tableModel_,
                    previewCache_));
            UpdateGalleryActivity();
        }

        void ClearGalleryPreviewQueue()
        {
            galleryPreviewController_.Clear();
        }

        void LoadPersistentPreviewState()
        {
            if (!database_.IsOpen())
            {
                previewStateFile_.clear();
                return;
            }

            previewStateFile_ =
                PreviewStateFilePath(database_.CacheDirectory());

            QString errorMessage;
            const bool loaded =
                LoadPreviewState(
                    previewStateFile_,
                    database_,
                    previewCache_,
                    errorMessage);

            tableModel_.NotifyAllPreviewStatusesChanged();

            if (!loaded)
            {
                statusLabel_->setText(errorMessage);
            }
        }

        void SavePersistentPreviewState()
        {
            if (!database_.IsOpen() || previewStateFile_.empty())
            {
                return;
            }

            QString errorMessage;
            SavePreviewState(
                previewStateFile_,
                database_,
                previewCache_,
                errorMessage);
        }

        void UpdateGalleryActivity()
        {
            galleryActivityIndicator_.Update(
                galleryPreviewController_.ActivityState(
                    galleryMode_,
                    database_.IsOpen(),
                    galleryPreviewWorker_.IsActive()));
        }

        void UpdateGalleryEntryCount()
        {
            galleryCountLabel_->setText(
                GalleryEntryCountText(
                    proxyModel_->rowCount(),
                    tableModel_.rowCount()));
        }

        void ExportSelected()
        {
            const std::vector<const CacheEntry*> entries = SelectedEntries();

            if (entries.empty())
            {
                return;
            }

            if (entries.size() > 1)
            {
                StartBatchPngExport(entries);
                return;
            }

            const CacheEntry& entry = *entries.front();

            const QString outputFile =
                ChoosePngOutputFile(
                    *this,
                    DefaultPngExportFileName(entry));

            if (outputFile.isEmpty())
            {
                return;
            }

            const TexturePngExportResult result =
                ExportTexturePng(
                    database_,
                    entry,
                    PathFromQString(outputFile),
                    true);

            statusLabel_->setText(PngExportStatusText(result, outputFile));
        }

        void StartBatchPngExport(const std::vector<const CacheEntry*>& entries)
        {
            const QString outputDirectory = ChoosePngOutputDirectory(*this);

            if (outputDirectory.isEmpty())
            {
                return;
            }

            const TextureCacheDatabase* database = &database_;
            const std::filesystem::path outputPath =
                PathFromQString(outputDirectory);

            try
            {
                batchExportFuture_ = std::async(
                    std::launch::async,
                    [database, entries, outputPath]()
                    {
                        return ExportTexturePngs(
                            *database,
                            entries,
                            outputPath);
                    });
            }
            catch (const std::exception& error)
            {
                statusLabel_->setText(
                    QStringLiteral("Could not start PNG export: %1")
                        .arg(QString::fromUtf8(error.what())));
                return;
            }

            batchExportActive_ = true;
            batchExportOutputDirectory_ = outputDirectory;
            SetBusy(
                true,
                QStringLiteral("Exporting %1 selected PNGs...")
                    .arg(entries.size()));
            batchExportPollTimer_->start();
        }

        void PollBatchPngExport()
        {
            if (!batchExportActive_)
            {
                batchExportPollTimer_->stop();
                return;
            }

            if (batchExportFuture_.wait_for(std::chrono::seconds(0)) !=
                std::future_status::ready)
            {
                return;
            }

            batchExportPollTimer_->stop();
            batchExportActive_ = false;
            const QString outputDirectory = batchExportOutputDirectory_;
            batchExportOutputDirectory_.clear();

            try
            {
                const BulkExportResults results = batchExportFuture_.get();
                SetBusy(false);
                statusLabel_->setText(
                    BulkPngExportStatusText(
                        results,
                        outputDirectory));
                ScheduleGalleryPreviewSearch();
                ScheduleTableSelectionPreview();
            }
            catch (const std::exception& error)
            {
                SetBusy(false);
                statusLabel_->setText(
                    QStringLiteral("PNG export failed: %1")
                        .arg(QString::fromUtf8(error.what())));
                ScheduleGalleryPreviewSearch();
                ScheduleTableSelectionPreview();
            }
        }

        QLabel* pathLabel_ = nullptr;
        QLineEdit* pathEdit_ = nullptr;
        QPushButton* browseButton_ = nullptr;
        QToolButton* defaultCacheButton_ = nullptr;
        QToolButton* recentCacheButton_ = nullptr;
        QMenu* recentCacheMenu_ = nullptr;
        QPushButton* openButton_ = nullptr;
        QPushButton* aboutButton_ = nullptr;
        QLabel* uuidLookupLabel_ = nullptr;
        QLineEdit* uuidLookupEdit_ = nullptr;
        QPushButton* findUuidButton_ = nullptr;
        QPushButton* tryNextButton_ = nullptr;
        QPushButton* exportButton_ = nullptr;
        QPushButton* viewToggleButton_ = nullptr;
        QLabel* galleryFilterLabel_ = nullptr;
        QComboBox* galleryFilterCombo_ = nullptr;
        QLabel* gallerySortLabel_ = nullptr;
        QComboBox* gallerySortCombo_ = nullptr;
        QLabel* galleryCountLabel_ = nullptr;
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
        GalleryFilterProxyModel* proxyModel_ = nullptr;
        QItemSelectionModel* selectionModel_ = nullptr;
        QTimer* previewPollTimer_ = nullptr;
        QTimer* tableSelectionPreviewTimer_ = nullptr;
        QTimer* galleryPreviewPollTimer_ = nullptr;
        QTimer* batchExportPollTimer_ = nullptr;
        GalleryPreviewController galleryPreviewController_;
        TextureCacheDatabase database_;
        std::filesystem::path previewStateFile_;
        bool busy_ = false;
        PreviewWorkerState previewWorker_;
        PreviewWorkerState galleryPreviewWorker_;
        std::future<BulkExportResults> batchExportFuture_;
        bool batchExportActive_ = false;
        QString batchExportOutputDirectory_;
        PreviewRequestKind activePreviewRequestKind_ = PreviewRequestKind::TableSelection;
        TryNextPreviewState tryNextPreview_;
        bool galleryMode_ = true;
        std::uint64_t nextPreviewRequestId_ = 1;
    };
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("CacheExplorer"));
    QApplication::setApplicationDisplayName(QStringLiteral("Cache Explorer"));
    QApplication::setApplicationVersion(QStringLiteral(CACHEEXPLORER_VERSION));
    QApplication::setOrganizationName(QStringLiteral("CacheExplorer"));

    QCommandLineParser commandLine;
    const QCommandLineOption smokeOpenOption(
        QStringLiteral("smoke-open"),
        QStringLiteral("Open a cache, populate the entry model, and exit."),
        QStringLiteral("cache-folder"));
    commandLine.addOption(smokeOpenOption);
    commandLine.process(app);

    MainWindow window;

    if (commandLine.isSet(smokeOpenOption))
    {
        return window.SmokeOpenCache(commandLine.value(smokeOpenOption)) ? 0 : 1;
    }

    window.show();
    return app.exec();
}

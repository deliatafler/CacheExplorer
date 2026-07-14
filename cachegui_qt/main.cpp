#include "TextureCacheDatabase.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <string>

#include <QApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
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

    QTableWidgetItem* MakeReadOnlyItem(const QString& text)
    {
        auto* item = new QTableWidgetItem(text);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        return item;
    }

    QTableWidgetItem* MakeNumericItem(qint64 value)
    {
        auto* item = MakeReadOnlyItem(QString::number(value));
        item->setData(Qt::UserRole, value);
        return item;
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

            table_ = new QTableWidget(root);
            table_->setColumnCount(5);
            table_->setHorizontalHeaderLabels({
                QStringLiteral("UUID"),
                QStringLiteral("Image"),
                QStringLiteral("Body"),
                QStringLiteral("Cache index"),
                QStringLiteral("Timestamp")});
            table_->setSelectionBehavior(QAbstractItemView::SelectRows);
            table_->setSelectionMode(QAbstractItemView::SingleSelection);
            table_->setSortingEnabled(true);
            table_->verticalHeader()->hide();
            table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
            table_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
            table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);

            statusLabel_ = new QLabel(
                QStringLiteral("Choose a Firestorm texture cache folder."),
                root);

            layout->addLayout(pathLayout);
            layout->addWidget(table_, 1);
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
                ResolveTextureCacheDirectory(pathEdit_->text().toStdWString());

            const CacheError result = database_.Open(cacheDirectory);

            if (result != CacheError::None)
            {
                table_->setRowCount(0);
                statusLabel_->setText(
                    QStringLiteral("Could not open cache: ")
                    + QString::fromUtf8(CacheErrorMessage(result)));
                return;
            }

            pathEdit_->setText(
                QString::fromStdWString(database_.CacheDirectory().wstring()));
            PopulateTable();
        }

        void PopulateTable()
        {
            const auto& entries = database_.Entries();

            table_->setSortingEnabled(false);
            table_->setRowCount(static_cast<int>(entries.size()));

            for (std::size_t index = 0; index < entries.size(); ++index)
            {
                const CacheEntry& entry = entries[index];
                const int row = static_cast<int>(index);

                table_->setItem(
                    row,
                    0,
                    MakeReadOnlyItem(ToQString(entry.uuid.ToString())));
                table_->setItem(
                    row,
                    1,
                    MakeNumericItem(entry.imageSize));
                table_->setItem(
                    row,
                    2,
                    MakeNumericItem(entry.bodySize));
                table_->setItem(
                    row,
                    3,
                    MakeNumericItem(entry.cacheIndex));
                table_->setItem(
                    row,
                    4,
                    MakeReadOnlyItem(FormatTime(entry.timestamp)));
            }

            table_->setSortingEnabled(true);
            table_->sortItems(4, Qt::DescendingOrder);

            const CacheHeader& header = database_.Header();
            statusLabel_->setText(
                QStringLiteral("Loaded %1 valid texture entries from %2 slots. Cache version %3.")
                    .arg(entries.size())
                    .arg(header.entryCount)
                    .arg(header.version));
        }

        QLabel* pathLabel_ = nullptr;
        QLineEdit* pathEdit_ = nullptr;
        QPushButton* browseButton_ = nullptr;
        QPushButton* openButton_ = nullptr;
        QTableWidget* table_ = nullptr;
        QLabel* statusLabel_ = nullptr;
        TextureCacheDatabase database_;
    };
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

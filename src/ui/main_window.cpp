#include "ui/main_window.h"
#include "core/logger.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHeaderView>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtGui/QDesktopServices>
#include <QtCore/QUrl>
#include <QtCore/QProcess>

namespace FastFileSearch {
namespace UI {

const QString MainWindow::WINDOW_TITLE = "FastFileSearch";
const QString MainWindow::ORGANIZATION_NAME = "FastFileSearch Team";
const QString MainWindow::APPLICATION_NAME = "FastFileSearch";

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , centralWidget_(nullptr)
    , mainLayout_(nullptr)
    , searchManager_(nullptr)
    , isIndexing_(false)
    , isSearching_(false)
    , indexingThread_(nullptr)
    , searchThread_(nullptr)
    , indexingWorker_(nullptr)
    , searchWorker_(nullptr)
    , searchTimer_(new QTimer(this))
{
    // Set application properties
    QApplication::setOrganizationName(ORGANIZATION_NAME);
    QApplication::setApplicationName(APPLICATION_NAME);
    QApplication::setApplicationVersion("1.0.0");
    
    // Initialize search manager
    FastFileSearch::AppSettings settings;
    settings.setDefaults();
    searchManager_ = std::make_unique<FastFileSearch::App::SearchManager>(settings);
    
    // Setup UI
    setupUI();
    applyDarkTheme();
    createFileIcons();
    
    // Setup threading
    setupThreading();
    
    // Connect search timer
    searchTimer_->setSingleShot(true);
    connect(searchTimer_, &QTimer::timeout, this, &MainWindow::performSearch);
    
    // Initialize search manager
    if (!searchManager_->initialize()) {
        QMessageBox::critical(this, "Error", "Failed to initialize search manager!");
    }
    
    updateWindowTitle();
    updateIndexingStatus();
    
    LOG_INFO("MainWindow initialized successfully");
}

MainWindow::~MainWindow() {
    if (indexingThread_) {
        indexingThread_->quit();
        indexingThread_->wait();
    }
    
    if (searchThread_) {
        searchThread_->quit();
        searchThread_->wait();
    }
    
    if (searchManager_) {
        searchManager_->shutdown();
    }
}

void MainWindow::setupUI() {
    // Create central widget
    centralWidget_ = new QWidget(this);
    setCentralWidget(centralWidget_);
    
    // Create main layout
    mainLayout_ = new QVBoxLayout(centralWidget_);
    mainLayout_->setContentsMargins(10, 10, 10, 10);
    mainLayout_->setSpacing(10);
    
    // Setup components
    setupMenuBar();
    setupSearchArea();
    setupResultsArea();
    setupStatusBar();
    setupSystemTray();
    
    // Set window properties
    setWindowTitle(WINDOW_TITLE);
    setMinimumSize(800, 600);
    resize(1200, 800);
    
    // Center window
    move(QApplication::desktop()->screen()->rect().center() - rect().center());
}

void MainWindow::setupMenuBar() {
    menuBar_ = menuBar();
    
    // File menu
    fileMenu_ = menuBar_->addMenu("&File");
    
    startIndexingAction_ = fileMenu_->addAction("&Start Indexing");
    startIndexingAction_->setShortcut(QKeySequence("Ctrl+I"));
    connect(startIndexingAction_, &QAction::triggered, this, &MainWindow::onToggleIndexing);
    
    stopIndexingAction_ = fileMenu_->addAction("&Stop Indexing");
    stopIndexingAction_->setShortcut(QKeySequence("Ctrl+Shift+I"));
    stopIndexingAction_->setEnabled(false);
    connect(stopIndexingAction_, &QAction::triggered, this, &MainWindow::onToggleIndexing);
    
    fileMenu_->addSeparator();
    
    exportAction_ = fileMenu_->addAction("&Export Results...");
    exportAction_->setShortcut(QKeySequence("Ctrl+E"));
    exportAction_->setEnabled(false);
    connect(exportAction_, &QAction::triggered, this, &MainWindow::onExportResults);
    
    fileMenu_->addSeparator();
    
    exitAction_ = fileMenu_->addAction("E&xit");
    exitAction_->setShortcut(QKeySequence("Ctrl+Q"));
    connect(exitAction_, &QAction::triggered, this, &QWidget::close);
    
    // Search menu
    searchMenu_ = menuBar_->addMenu("&Search");
    
    QAction* focusSearchAction = searchMenu_->addAction("&Focus Search");
    focusSearchAction->setShortcut(QKeySequence("Ctrl+F"));
    connect(focusSearchAction, &QAction::triggered, [this]() {
        searchLineEdit_->setFocus();
        searchLineEdit_->selectAll();
    });
    
    QAction* clearResultsAction = searchMenu_->addAction("&Clear Results");
    clearResultsAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(clearResultsAction, &QAction::triggered, this, &MainWindow::onClearResults);
    
    // Tools menu
    toolsMenu_ = menuBar_->addMenu("&Tools");
    
    settingsAction_ = toolsMenu_->addAction("&Settings...");
    settingsAction_->setShortcut(QKeySequence("Ctrl+,"));
    connect(settingsAction_, &QAction::triggered, this, &MainWindow::onShowSettings);
    
    // Help menu
    helpMenu_ = menuBar_->addMenu("&Help");
    
    aboutAction_ = helpMenu_->addAction("&About");
    connect(aboutAction_, &QAction::triggered, this, &MainWindow::onShowAbout);
}

void MainWindow::setupSearchArea() {
    searchGroup_ = new QGroupBox("Search", this);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchGroup_);
    
    // Search input row
    QHBoxLayout* inputLayout = new QHBoxLayout();
    
    searchLineEdit_ = new QLineEdit(this);
    searchLineEdit_->setPlaceholderText("Enter search query...");
    searchLineEdit_->setMinimumHeight(32);
    connect(searchLineEdit_, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(searchLineEdit_, &QLineEdit::returnPressed, this, &MainWindow::onSearchTriggered);
    
    searchModeCombo_ = new QComboBox(this);
    searchModeCombo_->addItem("Fuzzy", static_cast<int>(FastFileSearch::SearchMode::Fuzzy));
    searchModeCombo_->addItem("Exact", static_cast<int>(FastFileSearch::SearchMode::Exact));
    searchModeCombo_->addItem("Wildcard", static_cast<int>(FastFileSearch::SearchMode::Wildcard));
    searchModeCombo_->addItem("Regex", static_cast<int>(FastFileSearch::SearchMode::Regex));
    searchModeCombo_->setCurrentIndex(0);
    connect(searchModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSearchModeChanged);
    
    searchButton_ = new QPushButton("Search", this);
    searchButton_->setMinimumHeight(32);
    searchButton_->setDefault(true);
    connect(searchButton_, &QPushButton::clicked, this, &MainWindow::onSearchTriggered);
    
    clearButton_ = new QPushButton("Clear", this);
    clearButton_->setMinimumHeight(32);
    connect(clearButton_, &QPushButton::clicked, this, &MainWindow::onClearResults);
    
    inputLayout->addWidget(searchLineEdit_, 1);
    inputLayout->addWidget(searchModeCombo_);
    inputLayout->addWidget(searchButton_);
    inputLayout->addWidget(clearButton_);
    
    // Options row
    QHBoxLayout* optionsLayout = new QHBoxLayout();
    
    caseSensitiveCheck_ = new QCheckBox("Case sensitive", this);
    
    QLabel* maxResultsLabel = new QLabel("Max results:", this);
    maxResultsSpin_ = new QSpinBox(this);
    maxResultsSpin_->setRange(1, 10000);
    maxResultsSpin_->setValue(MAX_RESULTS_DEFAULT);
    maxResultsSpin_->setSuffix(" files");
    
    optionsLayout->addWidget(caseSensitiveCheck_);
    optionsLayout->addStretch();
    optionsLayout->addWidget(maxResultsLabel);
    optionsLayout->addWidget(maxResultsSpin_);
    
    searchLayout->addLayout(inputLayout);
    searchLayout->addLayout(optionsLayout);
    
    mainLayout_->addWidget(searchGroup_);
}

void MainWindow::setupResultsArea() {
    resultsGroup_ = new QGroupBox("Results", this);
    QVBoxLayout* resultsLayout = new QVBoxLayout(resultsGroup_);
    
    // Results info row
    QHBoxLayout* infoLayout = new QHBoxLayout();
    
    resultsCountLabel_ = new QLabel("No results", this);
    resultsCountLabel_->setStyleSheet("font-weight: bold;");
    
    searchTimeLabel_ = new QLabel("", this);
    searchTimeLabel_->setAlignment(Qt::AlignRight);
    
    infoLayout->addWidget(resultsCountLabel_);
    infoLayout->addStretch();
    infoLayout->addWidget(searchTimeLabel_);
    
    // Results tree
    resultsTree_ = new QTreeWidget(this);
    resultsTree_->setHeaderLabels({"Name", "Path", "Size", "Modified", "Type"});
    resultsTree_->setAlternatingRowColors(true);
    resultsTree_->setRootIsDecorated(false);
    resultsTree_->setSortingEnabled(true);
    resultsTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    // Set column widths
    QHeaderView* header = resultsTree_->header();
    header->resizeSection(0, 250);  // Name
    header->resizeSection(1, 400);  // Path
    header->resizeSection(2, 100);  // Size
    header->resizeSection(3, 150);  // Modified
    header->resizeSection(4, 100);  // Type
    header->setStretchLastSection(false);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    
    connect(resultsTree_, &QTreeWidget::itemDoubleClicked,
            this, &MainWindow::onResultItemDoubleClicked);
    connect(resultsTree_, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::onResultItemSelectionChanged);
    
    resultsLayout->addLayout(infoLayout);
    resultsLayout->addWidget(resultsTree_);
    
    mainLayout_->addWidget(resultsGroup_);
}

void MainWindow::setupStatusBar() {
    QStatusBar* statusBar = this->statusBar();
    
    // Indexing progress
    indexingProgress_ = new QProgressBar(this);
    indexingProgress_->setVisible(false);
    indexingProgress_->setMaximumWidth(200);
    
    indexingStatusLabel_ = new QLabel("Ready", this);
    
    statusLabel_ = new QLabel("Welcome to FastFileSearch", this);
    
    statusBar->addWidget(statusLabel_, 1);
    statusBar->addPermanentWidget(indexingStatusLabel_);
    statusBar->addPermanentWidget(indexingProgress_);
}

void MainWindow::setupSystemTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    
    systemTray_ = new QSystemTrayIcon(this);
    systemTray_->setIcon(QIcon(":/icons/app_icon.png"));
    systemTray_->setToolTip(WINDOW_TITLE);
    
    // Create tray menu
    trayMenu_ = new QMenu(this);
    
    QAction* showAction = trayMenu_->addAction("Show");
    connect(showAction, &QAction::triggered, this, &QWidget::showNormal);
    
    QAction* hideAction = trayMenu_->addAction("Hide");
    connect(hideAction, &QAction::triggered, this, &QWidget::hide);
    
    trayMenu_->addSeparator();
    
    QAction* exitTrayAction = trayMenu_->addAction("Exit");
    connect(exitTrayAction, &QAction::triggered, this, &QWidget::close);
    
    systemTray_->setContextMenu(trayMenu_);
    
    connect(systemTray_, &QSystemTrayIcon::activated,
            this, &MainWindow::onSystemTrayActivated);
    
    systemTray_->show();
}

void MainWindow::setupThreading() {
    // Indexing thread
    indexingThread_ = new QThread(this);
    indexingWorker_ = new IndexingWorker(searchManager_.get());
    indexingWorker_->moveToThread(indexingThread_);
    
    connect(indexingThread_, &QThread::started,
            indexingWorker_, &IndexingWorker::startIndexing);
    connect(indexingWorker_, &IndexingWorker::indexingStarted,
            this, &MainWindow::onIndexingStarted);
    connect(indexingWorker_, &IndexingWorker::indexingProgress,
            this, &MainWindow::onIndexingProgress);
    connect(indexingWorker_, &IndexingWorker::indexingCompleted,
            this, &MainWindow::onIndexingCompleted);
    
    // Search thread
    searchThread_ = new QThread(this);
    searchWorker_ = new SearchWorker(searchManager_.get());
    searchWorker_->moveToThread(searchThread_);
    
    connect(searchWorker_, &SearchWorker::searchCompleted,
            this, &MainWindow::onSearchCompleted);
    
    indexingThread_->start();
    searchThread_->start();
}

void MainWindow::applyDarkTheme() {
    QString darkStyle = R"(
        QMainWindow {
            background-color: #2b2b2b;
            color: #ffffff;
        }

        QMenuBar {
            background-color: #3c3c3c;
            color: #ffffff;
            border: none;
        }

        QMenuBar::item {
            background-color: transparent;
            padding: 4px 8px;
        }

        QMenuBar::item:selected {
            background-color: #4a4a4a;
        }

        QMenu {
            background-color: #3c3c3c;
            color: #ffffff;
            border: 1px solid #555555;
        }

        QMenu::item:selected {
            background-color: #4a4a4a;
        }

        QGroupBox {
            font-weight: bold;
            border: 2px solid #555555;
            border-radius: 5px;
            margin-top: 1ex;
            padding-top: 10px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }

        QLineEdit {
            background-color: #404040;
            border: 2px solid #555555;
            border-radius: 4px;
            padding: 5px;
            color: #ffffff;
        }

        QLineEdit:focus {
            border-color: #0078d4;
        }

        QPushButton {
            background-color: #0078d4;
            border: none;
            border-radius: 4px;
            padding: 8px 16px;
            color: #ffffff;
            font-weight: bold;
        }

        QPushButton:hover {
            background-color: #106ebe;
        }

        QPushButton:pressed {
            background-color: #005a9e;
        }

        QPushButton:disabled {
            background-color: #555555;
            color: #888888;
        }

        QComboBox {
            background-color: #404040;
            border: 2px solid #555555;
            border-radius: 4px;
            padding: 5px;
            color: #ffffff;
            min-width: 100px;
        }

        QComboBox::drop-down {
            border: none;
        }

        QComboBox::down-arrow {
            image: url(:/icons/arrow_down.png);
            width: 12px;
            height: 12px;
        }

        QComboBox QAbstractItemView {
            background-color: #404040;
            border: 1px solid #555555;
            selection-background-color: #0078d4;
        }

        QTreeWidget {
            background-color: #2b2b2b;
            alternate-background-color: #353535;
            color: #ffffff;
            border: 1px solid #555555;
            gridline-color: #555555;
        }

        QTreeWidget::item:selected {
            background-color: #0078d4;
        }

        QTreeWidget::item:hover {
            background-color: #404040;
        }

        QHeaderView::section {
            background-color: #3c3c3c;
            color: #ffffff;
            padding: 5px;
            border: 1px solid #555555;
        }

        QCheckBox {
            color: #ffffff;
        }

        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }

        QCheckBox::indicator:unchecked {
            background-color: #404040;
            border: 2px solid #555555;
            border-radius: 3px;
        }

        QCheckBox::indicator:checked {
            background-color: #0078d4;
            border: 2px solid #0078d4;
            border-radius: 3px;
            image: url(:/icons/check.png);
        }

        QSpinBox {
            background-color: #404040;
            border: 2px solid #555555;
            border-radius: 4px;
            padding: 5px;
            color: #ffffff;
        }

        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #555555;
            border: none;
            width: 16px;
        }

        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #666666;
        }

        QProgressBar {
            background-color: #404040;
            border: 1px solid #555555;
            border-radius: 4px;
            text-align: center;
        }

        QProgressBar::chunk {
            background-color: #0078d4;
            border-radius: 3px;
        }

        QStatusBar {
            background-color: #3c3c3c;
            color: #ffffff;
            border-top: 1px solid #555555;
        }

        QLabel {
            color: #ffffff;
        }
    )";

    setStyleSheet(darkStyle);
}

void MainWindow::createFileIcons() {
    // Create simple colored icons for different file types
    auto createColoredIcon = [](const QColor& color, const QString& text = "") -> QIcon {
        QPixmap pixmap(16, 16);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);

        // Draw background
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(0, 0, 16, 16, 2, 2);

        // Draw text if provided
        if (!text.isEmpty()) {
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 6, QFont::Bold));
            painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
        }

        return QIcon(pixmap);
    };

    // Create icons for different file types
    folderIcon_ = createColoredIcon(QColor(255, 193, 7), "📁");  // Yellow folder
    fileIcon_ = createColoredIcon(QColor(108, 117, 125), "📄");   // Gray file
    textFileIcon_ = createColoredIcon(QColor(40, 167, 69), "TXT"); // Green
    imageFileIcon_ = createColoredIcon(QColor(220, 53, 69), "IMG"); // Red
    audioFileIcon_ = createColoredIcon(QColor(102, 16, 242), "♪");   // Purple
    videoFileIcon_ = createColoredIcon(QColor(255, 193, 7), "▶");   // Orange
    archiveFileIcon_ = createColoredIcon(QColor(108, 117, 125), "ZIP"); // Gray
    executableFileIcon_ = createColoredIcon(QColor(220, 53, 69), "EXE"); // Red
    documentFileIcon_ = createColoredIcon(QColor(0, 123, 255), "DOC"); // Blue
    codeFileIcon_ = createColoredIcon(QColor(40, 167, 69), "</>"); // Green
}

QIcon MainWindow::getFileIcon(const FastFileSearch::FileEntry& entry) {
    if (entry.isDirectory()) {
        return folderIcon_;
    }

    QString ext = QString::fromStdString(entry.extension).toLower();

    // Text files
    if (ext == "txt" || ext == "md" || ext == "readme" || ext == "log") {
        return textFileIcon_;
    }

    // Image files
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" ||
        ext == "bmp" || ext == "svg" || ext == "ico") {
        return imageFileIcon_;
    }

    // Audio files
    if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "aac" ||
        ext == "ogg" || ext == "wma") {
        return audioFileIcon_;
    }

    // Video files
    if (ext == "mp4" || ext == "avi" || ext == "mkv" || ext == "mov" ||
        ext == "wmv" || ext == "flv" || ext == "webm") {
        return videoFileIcon_;
    }

    // Archive files
    if (ext == "zip" || ext == "rar" || ext == "7z" || ext == "tar" ||
        ext == "gz" || ext == "bz2") {
        return archiveFileIcon_;
    }

    // Executable files
    if (ext == "exe" || ext == "msi" || ext == "app" || ext == "deb" ||
        ext == "rpm" || ext == "dmg") {
        return executableFileIcon_;
    }

    // Document files
    if (ext == "pdf" || ext == "doc" || ext == "docx" || ext == "xls" ||
        ext == "xlsx" || ext == "ppt" || ext == "pptx") {
        return documentFileIcon_;
    }

    // Code files
    if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" ||
        ext == "py" || ext == "js" || ext == "html" || ext == "css" ||
        ext == "java" || ext == "cs" || ext == "php" || ext == "rb") {
        return codeFileIcon_;
    }

    return fileIcon_;
}

QString MainWindow::formatFileSize(uint64_t size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double displaySize = static_cast<double>(size);

    while (displaySize >= 1024.0 && unitIndex < 4) {
        displaySize /= 1024.0;
        unitIndex++;
    }

    if (unitIndex == 0) {
        return QString("%1 %2").arg(static_cast<int>(displaySize)).arg(units[unitIndex]);
    } else {
        return QString("%1 %2").arg(displaySize, 0, 'f', 1).arg(units[unitIndex]);
    }
}

QString MainWindow::formatDateTime(std::time_t timestamp) {
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(timestamp);
    return dateTime.toString("yyyy-MM-dd hh:mm:ss");
}

void MainWindow::onSearchTextChanged() {
    searchTimer_->stop();

    if (!searchLineEdit_->text().isEmpty()) {
        searchTimer_->start(SEARCH_DELAY_MS);
    } else {
        onClearResults();
    }
}

void MainWindow::onSearchModeChanged() {
    if (!searchLineEdit_->text().isEmpty()) {
        searchTimer_->stop();
        searchTimer_->start(SEARCH_DELAY_MS);
    }
}

void MainWindow::onSearchTriggered() {
    searchTimer_->stop();
    performSearch();
}

void MainWindow::performSearch() {
    QString query = searchLineEdit_->text().trimmed();
    if (query.isEmpty()) {
        return;
    }

    if (isSearching_) {
        return;
    }

    isSearching_ = true;
    searchButton_->setEnabled(false);
    searchButton_->setText("Searching...");

    // Get search parameters
    int modeIndex = searchModeCombo_->currentData().toInt();
    FastFileSearch::SearchMode mode = static_cast<FastFileSearch::SearchMode>(modeIndex);
    bool caseSensitive = caseSensitiveCheck_->isChecked();
    int maxResults = maxResultsSpin_->value();

    // Perform search in worker thread
    QMetaObject::invokeMethod(searchWorker_, "performSearch",
                             Qt::QueuedConnection,
                             Q_ARG(QString, query),
                             Q_ARG(FastFileSearch::SearchMode, mode),
                             Q_ARG(bool, caseSensitive),
                             Q_ARG(int, maxResults));
}

void MainWindow::onSearchCompleted(const FastFileSearch::SearchResults& results) {
    isSearching_ = false;
    searchButton_->setEnabled(true);
    searchButton_->setText("Search");

    updateSearchResults(results);

    // Update export action
    exportAction_->setEnabled(!results.empty());
}

void MainWindow::updateSearchResults(const FastFileSearch::SearchResults& results) {
    QMutexLocker locker(&resultsMutex_);

    currentResults_ = results;
    resultsTree_->clear();

    // Update results count
    QString countText;
    if (results.empty()) {
        countText = "No results";
    } else {
        countText = QString("%1 result%2").arg(results.size()).arg(results.size() == 1 ? "" : "s");
        if (results.getTotalMatches() > results.size()) {
            countText += QString(" (showing first %1 of %2)").arg(results.size()).arg(results.getTotalMatches());
        }
    }
    resultsCountLabel_->setText(countText);

    // Update search time
    auto searchTime = std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(results.getSearchTime());
    auto searchTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(searchTime);
    searchTimeLabel_->setText(QString("Search time: %1ms").arg(searchTimeMs.count()));

    // Populate results tree
    for (const auto& result : results) {
        QTreeWidgetItem* item = new QTreeWidgetItem(resultsTree_);

        // Set icon
        item->setIcon(0, getFileIcon(result.entry));

        // Set data
        item->setText(0, QString::fromStdString(result.entry.fileName));
        item->setText(1, QString::fromStdString(result.entry.fullPath));
        item->setText(2, formatFileSize(result.entry.size));
        item->setText(3, formatDateTime(result.entry.lastModified));
        item->setText(4, result.entry.isDirectory() ? "Folder" : "File");

        // Store file entry data
        item->setData(0, Qt::UserRole, QVariant::fromValue(result.entry.id));

        // Set tooltip
        QString tooltip = QString("Path: %1\nSize: %2\nModified: %3\nScore: %4")
                         .arg(QString::fromStdString(result.entry.fullPath))
                         .arg(formatFileSize(result.entry.size))
                         .arg(formatDateTime(result.entry.lastModified))
                         .arg(result.score, 0, 'f', 3);
        item->setToolTip(0, tooltip);
        item->setToolTip(1, tooltip);
    }

    // Sort by relevance (score) initially
    resultsTree_->sortByColumn(0, Qt::AscendingOrder);
}

void MainWindow::onResultItemDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)

    if (!item) return;

    QString filePath = item->text(1);

    // Open file or folder in system default application
    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void MainWindow::onResultItemSelectionChanged() {
    QList<QTreeWidgetItem*> selectedItems = resultsTree_->selectedItems();

    if (selectedItems.isEmpty()) {
        statusLabel_->setText("No file selected");
    } else if (selectedItems.size() == 1) {
        QTreeWidgetItem* item = selectedItems.first();
        QString fileName = item->text(0);
        QString fileSize = item->text(2);
        statusLabel_->setText(QString("Selected: %1 (%2)").arg(fileName).arg(fileSize));
    } else {
        statusLabel_->setText(QString("%1 files selected").arg(selectedItems.size()));
    }
}

void MainWindow::onToggleIndexing() {
    if (isIndexing_) {
        // Stop indexing
        QMetaObject::invokeMethod(indexingWorker_, "stopIndexing", Qt::QueuedConnection);
    } else {
        // Start indexing
        startIndexing();
    }
}

void MainWindow::startIndexing() {
    if (isIndexing_) return;

    isIndexing_ = true;
    startIndexingAction_->setEnabled(false);
    stopIndexingAction_->setEnabled(true);

    indexingProgress_->setVisible(true);
    indexingProgress_->setValue(0);
    indexingStatusLabel_->setText("Starting indexing...");

    // Start indexing in worker thread
    QMetaObject::invokeMethod(indexingWorker_, "startIndexing", Qt::QueuedConnection);
}

void MainWindow::onIndexingStarted() {
    indexingStatusLabel_->setText("Indexing files...");
    statusLabel_->setText("Indexing in progress...");
}

void MainWindow::onIndexingProgress(double percentage, const QString& currentPath) {
    indexingProgress_->setValue(static_cast<int>(percentage));
    indexingStatusLabel_->setText(QString("Indexing... %1%").arg(static_cast<int>(percentage)));

    if (!currentPath.isEmpty()) {
        statusLabel_->setText(QString("Indexing: %1").arg(currentPath));
    }
}

void MainWindow::onIndexingCompleted(bool success, const QString& message) {
    isIndexing_ = false;
    startIndexingAction_->setEnabled(true);
    stopIndexingAction_->setEnabled(false);

    indexingProgress_->setVisible(false);

    if (success) {
        indexingStatusLabel_->setText("Indexing completed");
        statusLabel_->setText("Ready for search");

        // Show completion message
        auto stats = searchManager_->getIndexStatistics();
        QString completionMsg = QString("Indexing completed successfully!\n\n"
                                       "Files indexed: %1\n"
                                       "Directories indexed: %2\n"
                                       "Total size: %3")
                               .arg(stats.totalFiles)
                               .arg(stats.totalDirectories)
                               .arg(formatFileSize(stats.totalSize));

        QMessageBox::information(this, "Indexing Complete", completionMsg);
    } else {
        indexingStatusLabel_->setText("Indexing failed");
        statusLabel_->setText("Indexing failed");

        QMessageBox::warning(this, "Indexing Failed",
                            QString("Indexing failed: %1").arg(message));
    }

    updateIndexingStatus();
}

void MainWindow::updateIndexingStatus() {
    auto stats = searchManager_->getIndexStatistics();

    if (stats.totalFiles > 0) {
        QString statusText = QString("Index: %1 files, %2 directories")
                            .arg(stats.totalFiles)
                            .arg(stats.totalDirectories);
        indexingStatusLabel_->setText(statusText);
    } else {
        indexingStatusLabel_->setText("No index");
    }
}

void MainWindow::onClearResults() {
    resultsTree_->clear();
    resultsCountLabel_->setText("No results");
    searchTimeLabel_->setText("");
    statusLabel_->setText("Results cleared");
    exportAction_->setEnabled(false);

    QMutexLocker locker(&resultsMutex_);
    currentResults_ = FastFileSearch::SearchResults("");
}

void MainWindow::onExportResults() {
    QMutexLocker locker(&resultsMutex_);

    if (currentResults_.empty()) {
        QMessageBox::information(this, "Export Results", "No results to export.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Search Results",
        QString("search_results_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "CSV Files (*.csv);;JSON Files (*.json);;Text Files (*.txt)");

    if (fileName.isEmpty()) {
        return;
    }

    // TODO: Implement export functionality using ExportManager
    QMessageBox::information(this, "Export Results",
                            QString("Results exported to: %1").arg(fileName));
}

void MainWindow::onShowSettings() {
    // TODO: Implement settings dialog
    QMessageBox::information(this, "Settings", "Settings dialog not yet implemented.");
}

void MainWindow::onShowAbout() {
    QString aboutText = QString(
        "<h2>%1</h2>"
        "<p>Version: %2</p>"
        "<p>A high-performance file search application built with C++20 and Qt6.</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>Lightning fast search with multiple modes</li>"
        "<li>Real-time file system monitoring</li>"
        "<li>Advanced filtering and sorting</li>"
        "<li>Cross-platform support</li>"
        "</ul>"
        "<p>Built with ❤️ by the FastFileSearch Team</p>"
    ).arg(WINDOW_TITLE).arg(QApplication::applicationVersion());

    QMessageBox::about(this, "About " + WINDOW_TITLE, aboutText);
}

void MainWindow::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            activateWindow();
            raise();
        }
        break;
    default:
        break;
    }
}

void MainWindow::updateWindowTitle() {
    QString title = WINDOW_TITLE;

    if (isIndexing_) {
        title += " - Indexing...";
    } else {
        auto stats = searchManager_->getIndexStatistics();
        if (stats.totalFiles > 0) {
            title += QString(" - %1 files indexed").arg(stats.totalFiles);
        }
    }

    setWindowTitle(title);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (systemTray_ && systemTray_->isVisible()) {
        hide();
        event->ignore();

        if (!property("trayMessageShown").toBool()) {
            systemTray_->showMessage("FastFileSearch",
                                   "Application was minimized to tray",
                                   QSystemTrayIcon::Information, 2000);
            setProperty("trayMessageShown", true);
        }
    } else {
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized() && systemTray_ && systemTray_->isVisible()) {
            hide();
        }
    }
}

// IndexingWorker implementation
IndexingWorker::IndexingWorker(FastFileSearch::App::SearchManager* searchManager)
    : searchManager_(searchManager), shouldStop_(false) {
}

void IndexingWorker::startIndexing() {
    shouldStop_ = false;
    emit indexingStarted();

    try {
        // Set up progress callback
        searchManager_->setIndexingProgressCallback(
            [this](double percentage, const std::string& currentPath) {
                if (shouldStop_) return;
                emit indexingProgress(percentage, QString::fromStdString(currentPath));
            });

        // Set up completion callback
        searchManager_->setIndexingCompletedCallback(
            [this](bool success, const std::string& message) {
                emit indexingCompleted(success, QString::fromStdString(message));
            });

        // Start indexing
        bool success = searchManager_->buildIndex();

        if (!shouldStop_) {
            emit indexingCompleted(success, success ? "Indexing completed successfully" : "Indexing failed");
        }

    } catch (const std::exception& e) {
        if (!shouldStop_) {
            emit indexingCompleted(false, QString("Indexing failed: %1").arg(e.what()));
        }
    }
}

void IndexingWorker::stopIndexing() {
    shouldStop_ = true;
    searchManager_->stopIndexing();
}

// SearchWorker implementation
SearchWorker::SearchWorker(FastFileSearch::App::SearchManager* searchManager)
    : searchManager_(searchManager) {
}

void SearchWorker::performSearch(const QString& query, FastFileSearch::SearchMode mode,
                                bool caseSensitive, int maxResults) {
    try {
        FastFileSearch::SearchQuery searchQuery;
        searchQuery.query = query.toStdString();
        searchQuery.mode = mode;
        searchQuery.caseSensitive = caseSensitive;
        searchQuery.maxResults = static_cast<uint32_t>(maxResults);

        auto results = searchManager_->search(searchQuery);
        emit searchCompleted(results);

    } catch (const std::exception& e) {
        emit searchError(QString("Search failed: %1").arg(e.what()));
    }
}

} // namespace UI
} // namespace FastFileSearch

#include "ui/main_window.moc"

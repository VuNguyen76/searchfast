#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItem>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QApplication>
#include <QtCore/QTimer>
#include <QtCore/QThread>
#include <QtCore/QMutex>
#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <memory>

#include "app/search_manager.h"
#include "core/types.h"

namespace FastFileSearch {
namespace UI {

class IndexingWorker;
class SearchWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    void onSearchTextChanged();
    void onSearchModeChanged();
    void onSearchTriggered();
    void onIndexingStarted();
    void onIndexingProgress(double percentage, const QString& currentPath);
    void onIndexingCompleted(bool success, const QString& message);
    void onSearchCompleted(const FastFileSearch::SearchResults& results);
    void onResultItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onResultItemSelectionChanged();
    void onShowSettings();
    void onShowAbout();
    void onToggleIndexing();
    void onClearResults();
    void onExportResults();
    void onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupSearchArea();
    void setupResultsArea();
    void setupStatusBar();
    void setupSystemTray();
    void applyDarkTheme();
    void createFileIcons();
    
    // Search and indexing
    void startIndexing();
    void performSearch();
    void updateSearchResults(const FastFileSearch::SearchResults& results);
    void updateIndexingStatus();
    
    // UI helpers
    QIcon getFileIcon(const FastFileSearch::FileEntry& entry);
    QString formatFileSize(uint64_t size);
    QString formatDateTime(std::time_t timestamp);
    void showInSystemTray();
    void hideToSystemTray();
    void updateWindowTitle();
    
    // Core components
    std::unique_ptr<FastFileSearch::App::SearchManager> searchManager_;
    
    // UI components
    QWidget* centralWidget_;
    QVBoxLayout* mainLayout_;
    QSplitter* mainSplitter_;
    
    // Search area
    QGroupBox* searchGroup_;
    QLineEdit* searchLineEdit_;
    QComboBox* searchModeCombo_;
    QPushButton* searchButton_;
    QPushButton* clearButton_;
    QCheckBox* caseSensitiveCheck_;
    QSpinBox* maxResultsSpin_;
    
    // Results area
    QGroupBox* resultsGroup_;
    QTreeWidget* resultsTree_;
    QLabel* resultsCountLabel_;
    QLabel* searchTimeLabel_;
    
    // Status and progress
    QProgressBar* indexingProgress_;
    QLabel* indexingStatusLabel_;
    QLabel* statusLabel_;
    
    // Menu and toolbar
    QMenuBar* menuBar_;
    QMenu* fileMenu_;
    QMenu* searchMenu_;
    QMenu* toolsMenu_;
    QMenu* helpMenu_;
    QAction* startIndexingAction_;
    QAction* stopIndexingAction_;
    QAction* settingsAction_;
    QAction* exportAction_;
    QAction* exitAction_;
    QAction* aboutAction_;
    
    // System tray
    QSystemTrayIcon* systemTray_;
    QMenu* trayMenu_;
    
    // Threading
    QThread* indexingThread_;
    QThread* searchThread_;
    IndexingWorker* indexingWorker_;
    SearchWorker* searchWorker_;
    
    // Icons cache
    QIcon folderIcon_;
    QIcon fileIcon_;
    QIcon textFileIcon_;
    QIcon imageFileIcon_;
    QIcon audioFileIcon_;
    QIcon videoFileIcon_;
    QIcon archiveFileIcon_;
    QIcon executableFileIcon_;
    QIcon documentFileIcon_;
    QIcon codeFileIcon_;
    
    // State
    bool isIndexing_;
    bool isSearching_;
    FastFileSearch::SearchResults currentResults_;
    QTimer* searchTimer_;
    QMutex resultsMutex_;
    
    // Constants
    static const int SEARCH_DELAY_MS = 300;
    static const int MAX_RESULTS_DEFAULT = 1000;
    static const QString WINDOW_TITLE;
    static const QString ORGANIZATION_NAME;
    static const QString APPLICATION_NAME;
};

// Worker classes for threading
class IndexingWorker : public QObject {
    Q_OBJECT

public:
    explicit IndexingWorker(FastFileSearch::App::SearchManager* searchManager);

public slots:
    void startIndexing();
    void stopIndexing();

signals:
    void indexingStarted();
    void indexingProgress(double percentage, const QString& currentPath);
    void indexingCompleted(bool success, const QString& message);

private:
    FastFileSearch::App::SearchManager* searchManager_;
    bool shouldStop_;
};

class SearchWorker : public QObject {
    Q_OBJECT

public:
    explicit SearchWorker(FastFileSearch::App::SearchManager* searchManager);

public slots:
    void performSearch(const QString& query, FastFileSearch::SearchMode mode, 
                      bool caseSensitive, int maxResults);

signals:
    void searchCompleted(const FastFileSearch::SearchResults& results);
    void searchError(const QString& error);

private:
    FastFileSearch::App::SearchManager* searchManager_;
};

} // namespace UI
} // namespace FastFileSearch

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDataStream>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QInputDialog>
#include <QLineEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSaveFile>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <memory>
#include <functional>

// Both algorithm files have a console main(). Rename those entry points when
// including them here so the GUI can call their functions without editing them.
#define main iddfsConsoleMain
#include "../IDDFS_function.cpp"
#undef main

#define main bfsConsoleMain
#include "../BFS_function.cpp"
#undef main

// Keep the loaded data between button clicks. Each algorithm has its own
// graph type, but both graphs contain the same actors and movie connections.
struct ActorData {
    Graph graph;
    coStarGraph bfsGraph;
    unordered_map<string, string> names;
    unordered_map<string, string> movies;
};

// Information needed to draw one result or ask the user to choose an actor.
struct SearchResult {
    QStringList actors;
    QStringList movies;
    QString message;
    double milliseconds = 0;
    QStringList startChoices;
    QStringList targetChoices;
    bool searched = false;
    bool bfsRan = false;
};

// Save the prepared graph under build/data so later launches can skip TSV
// parsing. File sizes and dates invalidate the cache when datasets change.
QString datasetStamp() {
    QString stamp;
    for (const QString& name : {QString("title.basics.tsv"), QString("title.principals.tsv"), QString("name.basics.tsv")}) {
        QFileInfo file(QCoreApplication::applicationDirPath() + "/data/" + name);
        stamp += QString("%1:%2;").arg(file.size()).arg(file.lastModified().toMSecsSinceEpoch());
    }
    return stamp;
}

void writeString(QDataStream& stream, const string& value) {
    stream << QByteArray::fromStdString(value);
}

string readString(QDataStream& stream) {
    QByteArray value;
    stream >> value;
    return value.toStdString();
}

void readTable(QDataStream& stream, unordered_map<string, string>& table) {
    quint32 count = 0;
    stream >> count;
    for (quint32 i = 0; i < count && stream.status() == QDataStream::Ok; i++) {
        string id = readString(stream);
        table[id] = readString(stream);
    }
}

void writeTable(QDataStream& stream, const unordered_map<string, string>& table) {
    stream << quint32(table.size());
    for (const auto& entry : table) {
        writeString(stream, entry.first);
        writeString(stream, entry.second);
    }
}

bool readCache(const QString& filename, ActorData& data) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) return false;
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    QString version, stamp;
    stream >> version >> stamp;
    if (version != "ActorGUI-1" || stamp != datasetStamp()) return false;
    ActorData loaded;
    // Read in the same order used by writeCache: movies, names, then edges.
    readTable(stream, loaded.movies);
    readTable(stream, loaded.names);
    quint32 actors = 0;
    stream >> actors;
    for (quint32 i = 0; i < actors && stream.status() == QDataStream::Ok; i++) {
        string id = readString(stream);
        quint32 count = 0;
        stream >> count;
        auto& edges = loaded.graph[id];
        for (quint32 j = 0; j < count && stream.status() == QDataStream::Ok; j++) {
            string actor = readString(stream);
            string movie = readString(stream);
            edges.push_back({actor, movie});
        }
    }
    if (stream.status() != QDataStream::Ok || loaded.graph.empty()) return false;
    data = std::move(loaded);
    return true;
}

bool writeCache(const QString& filename, const ActorData& data) {
    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) return false;
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << QString("ActorGUI-1") << datasetStamp();
    writeTable(stream, data.movies);
    writeTable(stream, data.names);
    stream << quint32(data.graph.size());
    for (const auto& actor : data.graph) {
        writeString(stream, actor.first);
        stream << quint32(actor.second.size());
        for (const Connection& edge : actor.second) {
            writeString(stream, edge.actorID);
            writeString(stream, edge.movieID);
        }
    }
    return stream.status() == QDataStream::Ok && file.commit();
}

struct ResultPanel {
    QLabel* status;
    QLabel* summary;
    QHBoxLayout* path;
    bool isBfs;
};

QLabel* textLabel(const QString& text) {
    QLabel* label = new QLabel(text);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

ResultPanel addPanel(QVBoxLayout* layout, const QString& title,
                     const QString& color, const QString& explanation, bool isBfs) {
    QWidget* panel = new QWidget;
    panel->setObjectName("panel");
    panel->setStyleSheet("QWidget#panel { background: white; border: 2px solid "
                         + color + "; border-radius: 10px; }");
    QVBoxLayout* contents = new QVBoxLayout(panel);
    contents->setContentsMargins(22, 20, 22, 20);
    contents->setSpacing(16);
    QLabel* heading = textLabel(title);
    heading->setStyleSheet("color: " + color + "; font-size: 23px; font-weight: bold;");
    contents->addWidget(heading);
    contents->addWidget(textLabel(explanation));
    QLabel* status = textLabel("Ready");
    status->setStyleSheet("color: " + color + "; font-weight: bold;");
    contents->addWidget(status);
    QWidget* chain = new QWidget;
    QHBoxLayout* path = new QHBoxLayout(chain);
    path->setContentsMargins(0, 0, 0, 0);
    QScrollArea* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(110);
    scroll->setWidget(chain);
    contents->addWidget(scroll);
    QLabel* summary = textLabel("Connections: —     Search time: —");
    contents->addWidget(summary);
    layout->addWidget(panel);
    return {status, summary, path, isBfs};
}

void clearPath(const ResultPanel& panel) {
    // Qt owns the layouts; remove only the old actor and movie labels here.
    while (QLayoutItem* item = panel.path->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    panel.summary->setText("Connections: —     Search time: —");
}

void displayResult(const ResultPanel& panel, const SearchResult& result) {
    clearPath(panel);
    panel.status->setText(result.message);
    if (result.searched) {
        QString summary = QString("Connections: %1     Search time: %2 ms")
            .arg(result.actors.isEmpty() ? QString("—") : QString::number(result.movies.size()))
            .arg(result.milliseconds, 0, 'f', 3);
        panel.summary->setText(summary);
    }
    if (result.actors.isEmpty()) return;
    for (int i = 0; i < result.actors.size(); i++) {
        if (i > 0) {
            QLabel* movie = textLabel(result.movies[i - 1] + "\n→");
            movie->setAlignment(Qt::AlignCenter);
            movie->setMinimumWidth(140);
            movie->setStyleSheet("color: #946000; background: #fff6dc; padding: 10px; border-radius: 6px;");
            panel.path->addWidget(movie);
        }
        QLabel* actor = textLabel(result.actors[i]);
        actor->setAlignment(Qt::AlignCenter);
        actor->setMinimumSize(150, 70);
        actor->setStyleSheet(panel.isBfs
            ? "background: #eaf2ff; color: #2163b3; border: 1px solid #9ebfe5; border-radius: 8px; padding: 10px;"
            : "background: #f0eaff; color: #5930a1; border: 1px solid #bca4e3; border-radius: 8px; padding: 10px;");
        panel.path->addWidget(actor);
    }
}

// The original loaders read plain TSV files. Extract the downloaded archives
// into the ignored build directory once; keep the Downloads files unchanged.
QString dataFile(const QString& filename) {
    QString folder = QCoreApplication::applicationDirPath() + "/data";
    QString target = folder + "/" + filename;
    if (QFileInfo::exists(target)) return target;
    QString archive = QDir::homePath() + "/Downloads/" + filename + ".gz";
    if (!QFileInfo::exists(archive))
        throw runtime_error(("Missing dataset: " + archive).toStdString());
    QDir().mkpath(folder);
    QProcess gzip;
    QString temporary = target + ".partial";
    gzip.setStandardOutputFile(temporary);
    gzip.start("/usr/bin/gzip", {"-dc", archive});
    if (!gzip.waitForStarted() || !gzip.waitForFinished(-1) ||
        gzip.exitStatus() != QProcess::NormalExit || gzip.exitCode() != 0) {
        QFile::remove(temporary);
        throw runtime_error(("Could not extract " + filename).toStdString());
    }
    if (!QFile::rename(temporary, target))
        throw runtime_error(("Could not save " + target).toStdString());
    return target;
}

// Load the saved graph, or build it from the three IMDb files if needed.
void loadData(ActorData& data, const std::function<void(const QString&)>& progress) {
    QString cache;
    if (data.graph.empty()) {
        cache = QCoreApplication::applicationDirPath() + "/data/actors.cache";
        progress("Opening prepared data, if available");
        readCache(cache, data);
    }
    if (data.graph.empty()) {
        progress("1/5 · Preparing movie file");
        QString movieFile = dataFile("title.basics.tsv");
        progress("1/5 · Reading movie titles (large IMDb file)");
        auto movies = loadMovies(movieFile.toStdString());
        progress("2/5 · Preparing cast file");
        QString castFile = dataFile("title.principals.tsv");
        progress("2/5 · Reading movie casts (the largest file)");
        auto cast = loadMovieCast(castFile.toStdString(), movies);
        progress("3/5 · Building actor connections");
        Graph graph = buildGraph(cast);
        cast.clear();
        progress("4/5 · Preparing actor names file");
        QString nameFile = dataFile("name.basics.tsv");
        progress("4/5 · Reading actor names");
        auto names = loadActorNames(nameFile.toStdString(), graph);
        data.movies = std::move(movies);
        data.names = std::move(names);
        data.graph = std::move(graph);
        progress("Saving prepared data for faster loading next time");
        // A cache write failure should not prevent searching loaded data.
        writeCache(cache, data);
    }
}

// Progress updates and the BFS result are sent back to the window while
// the searches run. Neither search time includes loading or drawing labels.
SearchResult compareActors(ActorData& data, const QString& startName,
                         const QString& targetName,
                         const std::function<void(const QString&)>& progress = [](const QString&) {},
                         const QString& selectedStart = "", const QString& selectedTarget = "",
                         const std::function<void(const SearchResult&)>& bfsReady = [](const SearchResult&) {}) {
    SearchResult result;
    try {
        loadData(data, progress);
        // Resolve names without the console function's interactive cin prompt.
        progress("5/5 · Looking up the two actors");
        QStringList startMatches, targetMatches;
        for (const auto& actor : data.names) {
            QString name = QString::fromStdString(actor.second);
            if (name.compare(startName, Qt::CaseInsensitive) == 0) {
                startMatches << QString::fromStdString(actor.first);
            }
            if (name.compare(targetName, Qt::CaseInsensitive) == 0) {
                targetMatches << QString::fromStdString(actor.first);
            }
        }
        if (startMatches.contains(selectedStart)) startMatches = {selectedStart};
        if (targetMatches.contains(selectedTarget)) targetMatches = {selectedTarget};
        if (startMatches.isEmpty() || targetMatches.isEmpty()) {
            result.message = "Actor not found in the movie dataset. Check both full names.";
            return result;
        }
        if (startMatches.size() > 1 || targetMatches.size() > 1) {
            if (startMatches.size() > 1) result.startChoices = startMatches;
            if (targetMatches.size() > 1) result.targetChoices = targetMatches;
            result.message = "Choose which actor you mean. Data is already loaded.";
            return result;
        }
        string start = startMatches.first().toStdString();
        string target = targetMatches.first().toStdString();
        // Both algorithms receive the same actors and movie edges. Their
        // structs differ, so adapt the data here without changing either file.
        if (data.bfsGraph.empty()) {
            progress("Preparing shared graph for BFS (once per session)");
            data.bfsGraph.reserve(data.graph.size());
            for (const auto& actor : data.graph) {
                auto& edges = data.bfsGraph[actor.first];
                edges.reserve(actor.second.size());
                for (const Connection& edge : actor.second)
                    edges.push_back({edge.actorID, edge.movieID});
            }
        }
        SearchResult bfsResult;
        vector<jump> jumps;
        size_t visited = 0;
        QElapsedTimer bfsTimer;
        progress("Running Bidirectional BFS");
        bfsTimer.start();
        bool bfsFound = bidirectionalBFS(data.bfsGraph, start, target, jumps, visited);
        bfsResult.milliseconds = bfsTimer.nsecsElapsed() / 1000000.0;
        bfsResult.searched = true;
        bfsResult.message = bfsFound ? "Complete · shortest path" : "No connection found in the graph";
        if (bfsFound) {
            bfsResult.actors << QString::fromStdString(data.names.at(start));
            for (const jump& step : jumps) {
                auto actor = data.names.find(step.actor);
                bfsResult.actors << QString::fromStdString(actor == data.names.end() ? step.actor : actor->second);
                bfsResult.movies << QString::fromStdString(data.movies.at(step.movie));
            }
        }
        result.bfsRan = true;
        bfsReady(bfsResult); // Display BFS immediately while IDDFS runs next.
        vector<Connection> path;
        QElapsedTimer timer;
        progress("5/5 · Running IDDFS (maximum 6 connections)");
        timer.start();
        bool found = IDDFS(data.graph, start, target, 6, path);
        result.milliseconds = timer.nsecsElapsed() / 1000000.0;
        result.searched = true;
        if (!found) {
            result.message = "No connection found within 6 steps.";
            return result;
        }
        result.actors << QString::fromStdString(data.names.at(start));
        for (const Connection& step : path) {
            auto actor = data.names.find(step.actorID);
            result.actors << QString::fromStdString(actor == data.names.end() ? step.actorID : actor->second);
            result.movies << QString::fromStdString(data.movies.at(step.movieID));
        }
        result.message = "Complete · shortest path within the 6-step limit";
    } catch (const exception& error) {
        result.message = "Could not search: " + QString::fromUtf8(error.what());
    }
    return result;
}

// Show recognizable movie credits instead of asking the user to guess an ID.
QString chooseActor(QStringList ids, const ActorData& data, QWidget* parent,
                    const QString& role) {
    ids.sort();
    QStringList options;
    for (const QString& id : ids) {
        QStringList credits;
        auto actor = data.graph.find(id.toStdString());
        if (actor != data.graph.end()) {
            for (const Connection& edge : actor->second) {
                auto movie = data.movies.find(edge.movieID);
                if (movie != data.movies.end()) {
                    QString title = QString::fromStdString(movie->second);
                    if (!credits.contains(title)) credits << title;
                }
            }
        }
        credits.sort();
        options << QString::fromStdString(data.names.at(id.toStdString()))
                   + " (" + id + ") — " + credits.mid(0, 4).join(", ");
    }
    bool accepted = false;
    QString choice = QInputDialog::getItem(parent, "Choose " + role,
        "IMDb has multiple people with this name. Choose using their movie credits:",
        options, 0, false, &accepted);
    return accepted ? ids.at(options.indexOf(choice)) : QString();
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setStyleSheet(R"(
        QWidget { background: #f3f5fa; color: #243047; font-size: 17px; }
        QLabel { background: transparent; }
        QLabel#title { font-size: 30px; font-weight: bold; }
        QLineEdit { background: white; border: 1px solid #bdc7d9; border-radius: 6px; padding: 14px; }
        QLineEdit:focus { border: 1px solid #5930a1; }
        QPushButton { background: #5930a1; color: white; border: none; border-radius: 6px; padding: 14px 24px; }
        QPushButton:hover { background: #482587; }
        QPushButton:disabled { background: #a99bbf; }
        QScrollArea { background: white; border: none; }
    )");
    QWidget window;
    window.setWindowTitle("Actor Connection Finder");
    window.resize(1050, 920);
    QVBoxLayout* outer = new QVBoxLayout(&window);
    QScrollArea* page = new QScrollArea;
    page->setWidgetResizable(true);
    outer->addWidget(page);
    QWidget* content = new QWidget;
    page->setWidget(content);
    QVBoxLayout* layout = new QVBoxLayout(content);
    layout->setContentsMargins(28, 28, 28, 28);
    layout->setSpacing(18);
    QLabel* title = textLabel("Actor Connection Finder");
    title->setObjectName("title");
    layout->addWidget(title);
    layout->addWidget(textLabel("1. Enter two actors     2. Run comparison     3. Compare paths and search times"));
    QHBoxLayout* inputs = new QHBoxLayout;
    QVBoxLayout* startColumn = new QVBoxLayout;
    QVBoxLayout* targetColumn = new QVBoxLayout;
    QLineEdit* startInput = new QLineEdit;
    QLineEdit* targetInput = new QLineEdit;
    startInput->setPlaceholderText("e.g. Zendaya");
    targetInput->setPlaceholderText("e.g. Tom Holland");
    startColumn->addWidget(textLabel("Starting actor"));
    startColumn->addWidget(startInput);
    targetColumn->addWidget(textLabel("Target actor"));
    targetColumn->addWidget(targetInput);
    inputs->addLayout(startColumn);
    inputs->addLayout(targetColumn);
    layout->addLayout(inputs);
    QPushButton* run = new QPushButton("Run comparison");
    layout->addWidget(run, 0, Qt::AlignLeft);
    QLabel* notice = textLabel("Both algorithms search the same movie graph. Blue cards show BFS; purple cards show IDDFS.");
    layout->addWidget(notice);
    QProgressBar* progressBar = new QProgressBar;
    progressBar->setRange(0, 0);
    progressBar->hide();
    layout->addWidget(progressBar);
    QLabel* elapsed = textLabel("");
    elapsed->hide();
    layout->addWidget(elapsed);
    QTimer* loadingTimer = new QTimer(&window);
    auto loadingTime = std::make_shared<QElapsedTimer>();
    QObject::connect(loadingTimer, &QTimer::timeout, &window, [=]() {
        elapsed->setText(QString("Total time loading / searching: %1 seconds")
                         .arg(loadingTime->elapsed() / 1000));
    });
    ResultPanel bfs = addPanel(layout, "Bidirectional BFS", "#2163b3", "Searches outward from both actors until the paths meet.", true);
    bfs.status->setText("Enter two names to start");
    ResultPanel iddfs = addPanel(layout, "IDDFS", "#7041b0", "Tries increasing path depths, up to 6 connections. Purple cards are actors; gold labels are shared movies.", false);
    iddfs.status->setText("Enter two names to start");

    // The worker keeps a shared pointer so its data stays alive if the window
    // closes before a search finishes. Qt deletes child widgets automatically.
    auto data = std::make_shared<ActorData>();
    QString selectedStart;
    QString selectedTarget;
    QFutureWatcher<SearchResult>* watcher = new QFutureWatcher<SearchResult>(&window);
    // Editing a name clears an earlier choice from the duplicate-name dialog.
    QObject::connect(startInput, &QLineEdit::textEdited, &window, [&selectedStart]() {
        selectedStart.clear();
    });
    QObject::connect(targetInput, &QLineEdit::textEdited, &window, [&selectedTarget]() {
        selectedTarget.clear();
    });
    // This handler runs on the GUI thread after the worker returns a result.
    QObject::connect(watcher, &QFutureWatcher<SearchResult>::finished, &window,
                     [=, &window, &selectedStart, &selectedTarget]() {
        SearchResult result = watcher->result();
        displayResult(iddfs, result);
        if (!result.bfsRan) {
            clearPath(bfs);
            bfs.status->setText(result.message);
        }
        loadingTimer->stop();
        progressBar->hide();
        run->setEnabled(true);
        startInput->setEnabled(true);
        targetInput->setEnabled(true);
        elapsed->hide();
        notice->setText(result.bfsRan
            ? "Comparison finished. Times measure each algorithm only; loading and graph conversion are excluded. IDDFS searches up to 6 connections."
            : result.message);
        if (!result.startChoices.isEmpty() || !result.targetChoices.isEmpty()) {
            if (!result.startChoices.isEmpty()) {
                QString id = chooseActor(result.startChoices, *data, &window, "starting actor");
                if (id.isEmpty()) {
                    notice->setText("Selection cancelled. The data stays loaded; you can try again.");
                    return;
                }
                selectedStart = id;
            }
            if (!result.targetChoices.isEmpty()) {
                QString id = chooseActor(result.targetChoices, *data, &window, "target actor");
                if (id.isEmpty()) {
                    notice->setText("Selection cancelled. The data stays loaded; you can try again.");
                    return;
                }
                selectedTarget = id;
            }
            // Continue with the chosen IDs using the graph already in memory.
            QTimer::singleShot(0, run, &QPushButton::click);
        }
    });
    QObject::connect(run, &QPushButton::clicked, &window, [=, &selectedStart, &selectedTarget]() {
        QString start = startInput->text().trimmed();
        QString target = targetInput->text().trimmed();
        // Copy the selected IDs before sending the request to the worker.
        QString startId = selectedStart;
        QString targetId = selectedTarget;
        if (start.isEmpty() || target.isEmpty()) {
            notice->setText("Please enter both actor names.");
            return;
        }
        clearPath(iddfs);
        clearPath(bfs);
        bfs.status->setText("Loading data / preparing comparison…");
        iddfs.status->setText("Loading data / searching…");
        notice->setText(data->graph.empty()
            ? "Loading the full IMDb dataset once. Each stage below may take time; keep this window open. Later searches reuse the loaded graph."
            : "Using the loaded graph. IDDFS can take longer for distant or unconnected actors.");
        loadingTime->start();
        elapsed->setText("Total time loading / searching: 0 seconds");
        elapsed->show();
        progressBar->show();
        loadingTimer->start(1000);
        run->setEnabled(false);
        startInput->setEnabled(false);
        targetInput->setEnabled(false);
        // Keep the window responsive while the existing algorithm runs.
        watcher->setFuture(QtConcurrent::run([=]() {
            auto progress = [=](const QString& stage) {
                // Widgets must only be changed on the GUI thread.
                QMetaObject::invokeMethod(watcher, [=]() {
                    iddfs.status->setText(stage);
                    if (!stage.contains("IDDFS")) bfs.status->setText(stage);
                    notice->setText(stage);
                }, Qt::QueuedConnection);
            };
            auto showBFS = [=](const SearchResult& result) {
                QMetaObject::invokeMethod(watcher, [=]() {
                    displayResult(bfs, result);
                }, Qt::QueuedConnection);
            };
            return compareActors(*data, start, target, progress, startId, targetId, showBFS);
        }));
    });
    QObject::connect(startInput, &QLineEdit::returnPressed, run, &QPushButton::click);
    QObject::connect(targetInput, &QLineEdit::returnPressed, run, &QPushButton::click);
    window.show();
    return app.exec();
}

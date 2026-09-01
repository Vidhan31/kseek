#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <memory>

#include "types.h"
#include "icon_resolver.h"
#include "process_pipeline.h"
#include "kseek_runner.h"

class TestRunner : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testMetaTypeRegistration();
    void testQueryParsing();
    void testIconResolution();
    void testActionsList();
    void testMatchStructure();
    void testErrorMatch();
    void testSearchPipelineFuzzy();
    void testSearchPipelineSpecialChars();
    void testSearchCancellation();
    void testFdCustomArgs();
    void testFzfEnvIsolation();
    void testFzfCustomArgs();

private:
    std::unique_ptr<QTemporaryDir> m_tempDir;
    QString m_testDirPath;
};

void TestRunner::initTestCase() {
    registerMetaTypes();
}

void TestRunner::cleanupTestCase() {
}

void TestRunner::init() {
    m_tempDir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tempDir->isValid());
    m_testDirPath = m_tempDir->path();

    QDir rootDir(m_testDirPath);
    rootDir.mkpath(QStringLiteral("src/nested"));
    rootDir.mkpath(QStringLiteral("docs"));
    rootDir.mkpath(QStringLiteral(".git"));

    const QStringList files = {
        QStringLiteral("main.py"),
        QStringLiteral("README.md"),
        QStringLiteral("src/kseek.cpp"),
        QStringLiteral("src/nested/C++_advanced.cpp"),
        QStringLiteral("docs/report [2024].pdf"),
        QStringLiteral("docs/spaced name file.txt"),
        QStringLiteral(".hidden_config"),
        QStringLiteral(".git/config")
    };

    for (const auto &rel : files) {
        QFile f(rootDir.filePath(rel));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("test data\n");
        f.close();
    }
}

void TestRunner::cleanup() {
    m_tempDir.reset();
}

void TestRunner::testMetaTypeRegistration() {
    RemoteAction act{QStringLiteral("open"), QStringLiteral("Open File"), QStringLiteral("system-run")};
    QVariant vAct = QVariant::fromValue(act);
    QVERIFY(vAct.isValid());
    QCOMPARE(vAct.value<RemoteAction>(), act);

    RemoteMatch match;
    match.id = QStringLiteral("/test/path");
    match.text = QStringLiteral("path");
    match.icon = QStringLiteral("text-plain");
    match.type = CategoryRelevance::High;
    match.relevance = 0.9;
    match.properties[QStringLiteral("subtext")] = QStringLiteral("/test/path");
    match.properties[QStringLiteral("multiline")] = false;

    QVariant vMatch = QVariant::fromValue(match);
    QVERIFY(vMatch.isValid());
    QCOMPARE(vMatch.value<RemoteMatch>(), match);
}

void TestRunner::testQueryParsing() {
    const QRegularExpression re(QStringLiteral(R"(^f\s+(.+)$)"));

    auto m1 = re.match(QStringLiteral("f test"));
    QVERIFY(m1.hasMatch());
    QCOMPARE(m1.captured(1), QStringLiteral("test"));

    auto m2 = re.match(QStringLiteral("f   spaced query  "));
    QVERIFY(m2.hasMatch());
    QCOMPARE(m2.captured(1), QStringLiteral("spaced query  "));

    QVERIFY(!re.match(QStringLiteral("g test")).hasMatch());
    QVERIFY(!re.match(QStringLiteral("find test")).hasMatch());
    QVERIFY(!re.match(QStringLiteral("f")).hasMatch());
}

void TestRunner::testIconResolution() {
    IconResolver resolver;

    QCOMPARE(resolver.resolve(m_testDirPath, true), QStringLiteral("inode-directory"));

    const QString pyIcon = resolver.resolve(QStringLiteral("test.py"), false);
    QVERIFY(pyIcon.contains(QLatin1StringView("python")) ||
            pyIcon.contains(QLatin1StringView("text")) ||
            pyIcon.contains(QLatin1StringView("code")) ||
            pyIcon.contains(QLatin1StringView("script")));

    const QString pdfIcon = resolver.resolve(QStringLiteral("document.pdf"), false);
    QVERIFY(pdfIcon.contains(QLatin1StringView("pdf")) ||
            pdfIcon.contains(QLatin1StringView("document")) ||
            pdfIcon.contains(QLatin1StringView("application")));

    const QString cppIcon = resolver.resolve(QStringLiteral("test.cpp"), false);
    QVERIFY(cppIcon.contains(QLatin1StringView("c")) || cppIcon.contains(QLatin1StringView("text")));

    // Extensionless file resolution
    const QString makefileIcon = resolver.resolve(QStringLiteral("Makefile"), false);
    QVERIFY(makefileIcon.contains(QLatin1StringView("makefile")) ||
            makefileIcon.contains(QLatin1StringView("text")) ||
            makefileIcon.contains(QLatin1StringView("code")));

    const QString dockerfileIcon = resolver.resolve(QStringLiteral("nested/dir/Dockerfile"), false);
    QVERIFY(dockerfileIcon.contains(QLatin1StringView("docker")) ||
            dockerfileIcon.contains(QLatin1StringView("text")) ||
            dockerfileIcon.contains(QLatin1StringView("application")));

    // Test caching consistency
    const QString icon1 = resolver.resolve(QStringLiteral("a.xyz_unique_ext"), false);
    const QString icon2 = resolver.resolve(QStringLiteral("b.xyz_unique_ext"), false);
    QCOMPARE(icon1, icon2);
}

void TestRunner::testActionsList() {
    KSeekRunner runner(m_testDirPath);
    const RemoteActions acts = runner.actions();

    QCOMPARE(acts.size(), 4);

    QStringList ids;
    for (const auto &a : acts) {
        ids.append(a.id);
    }

    QVERIFY(ids.contains(QStringLiteral("open_app")));
    QVERIFY(ids.contains(QStringLiteral("show_item")));
    QVERIFY(ids.contains(QStringLiteral("copy_path")));
    QVERIFY(ids.contains(QStringLiteral("open_terminal")));
}

void TestRunner::testMatchStructure() {
    KSeekRunner runner(m_testDirPath);
    RemoteMatch m = runner.buildMatch(QStringLiteral("src/kseek.cpp"), 0, 1);

    QVERIFY(!m.id.isEmpty());
    QCOMPARE(m.text, QStringLiteral("kseek.cpp"));
    QCOMPARE(m.type, CategoryRelevance::High);
    QCOMPARE(m.relevance, 1.0);

    QVERIFY(m.properties.contains(QStringLiteral("subtext")));
    QVERIFY(m.properties.contains(QStringLiteral("category")));
    QVERIFY(m.properties.contains(QStringLiteral("urls")));
    QVERIFY(m.properties.contains(QStringLiteral("actions")));
    QVERIFY(m.properties.contains(QStringLiteral("multiline")));

    QCOMPARE(m.properties.value(QStringLiteral("category")).toString(), QStringLiteral("Files & Folders"));
    QCOMPARE(m.properties.value(QStringLiteral("multiline")).toBool(), false);

    const QStringList urls = m.properties.value(QStringLiteral("urls")).toStringList();
    QVERIFY(!urls.isEmpty());
    QVERIFY(urls.first().startsWith(QLatin1StringView("file://")));

    const QStringList actions = m.properties.value(QStringLiteral("actions")).toStringList();
    QCOMPARE(actions.size(), 4);
    QVERIFY(actions.contains(QStringLiteral("open_app")));
    QVERIFY(actions.contains(QStringLiteral("show_item")));
    QVERIFY(actions.contains(QStringLiteral("copy_path")));
    QVERIFY(actions.contains(QStringLiteral("open_terminal")));

    // Test absolute path match (e.g. when fd -a is used)
    const QString absPath = m_testDirPath + QStringLiteral("/src/kseek.cpp");
    RemoteMatch mAbs = runner.buildMatch(absPath, 0, 1);
    QVERIFY(!mAbs.id.isEmpty());
    QCOMPARE(mAbs.id, absPath);
    QCOMPARE(mAbs.text, QStringLiteral("kseek.cpp"));
}

void TestRunner::testErrorMatch() {
    KSeekRunner runner(m_testDirPath);
    RemoteMatch m = runner.buildErrorMatch(QStringLiteral("Test error"));

    QCOMPARE(m.id, QStringLiteral("__error__"));
    QCOMPARE(m.text, QStringLiteral("Test error"));
    QCOMPARE(m.icon, QStringLiteral("dialog-warning"));
    QCOMPARE(m.type, CategoryRelevance::Lowest);
    QCOMPARE(m.relevance, 0.0);
    QCOMPARE(m.properties.value(QStringLiteral("category")).toString(), QStringLiteral("Error"));
}

void TestRunner::testSearchPipelineFuzzy() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
    QSignalSpy errorSpy(&pipeline, &ProcessPipeline::searchError);

    pipeline.startSearch(1, m_testDirPath, QStringLiteral("ksk"), {}, {}, 20, 2000);
    QVERIFY(finishedSpy.wait(3000));
    QCOMPARE(finishedSpy.count(), 1);

    const auto args = finishedSpy.takeFirst();
    QCOMPARE(args.at(0).toULongLong(), 1ULL);

    const QStringList results = args.at(1).toStringList();
    QVERIFY(!results.isEmpty());

    bool found = false;
    for (const QString &p : results) {
        if (p.contains(QLatin1StringView("kseek.cpp"))) {
            found = true;
            break;
        }
    }
    QVERIFY(found);

    // Verify .git is ignored
    for (const QString &p : results) {
        QVERIFY(!p.contains(QLatin1StringView(".git")));
    }
}

void TestRunner::testSearchPipelineSpecialChars() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    {
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(2, m_testDirPath, QStringLiteral("C++"), {}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("C++_advanced.cpp"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    {
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(3, m_testDirPath, QStringLiteral("2024"), {}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("report [2024].pdf"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }
}

void TestRunner::testSearchCancellation() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);

    pipeline.startSearch(10, m_testDirPath, QStringLiteral("main"), {}, {}, 20, 2000);
    pipeline.cancel();
    pipeline.startSearch(11, m_testDirPath, QStringLiteral("README"), {}, {}, 20, 2000);

    QVERIFY(finishedSpy.wait(3000));
    QCOMPARE(finishedSpy.count(), 1);

    const auto args = finishedSpy.takeFirst();
    QCOMPARE(args.at(0).toULongLong(), 11ULL);

    const QStringList results = args.at(1).toStringList();
    bool found = false;
    for (const QString &p : results) {
        if (p.contains(QLatin1StringView("README.md"))) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestRunner::testFdCustomArgs() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    {
        // By default, hidden files are ignored
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(20, m_testDirPath, QStringLiteral("hidden_config"), {}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        QVERIFY(results.isEmpty());
    }

    {
        // With --hidden passed in extraFdArgs
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(21, m_testDirPath, QStringLiteral("hidden_config"), {QStringLiteral("--hidden")}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView(".hidden_config"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }
}

void TestRunner::testFzfEnvIsolation() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    // Set toxic FZF_DEFAULT_OPTS in environment that would normally break fzf --filter
    qputenv("FZF_DEFAULT_OPTS", "--color=always --preview 'invalid command'");
    qputenv("FZF_DEFAULT_COMMAND", "false");

    QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
    pipeline.startSearch(30, m_testDirPath, QStringLiteral("README"), {}, {}, 20, 2000);
    QVERIFY(finishedSpy.wait(3000));
    QCOMPARE(finishedSpy.count(), 1);

    const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
    bool found = false;
    for (const QString &p : results) {
        if (p.contains(QLatin1StringView("README.md"))) {
            found = true;
            break;
        }
    }
    QVERIFY(found);

    qunsetenv("FZF_DEFAULT_OPTS");
    qunsetenv("FZF_DEFAULT_COMMAND");
}

void TestRunner::testFzfCustomArgs() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    // Create test files
    QFile f1(m_testDirPath + QStringLiteral("/apple_pie.txt"));
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("apple pie");
    f1.close();

    // Fuzzy search for "appie" should find apple_pie.txt (a-p-p-i-e)
    {
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(40, m_testDirPath, QStringLiteral("appie"), {}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("apple_pie.txt"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    // Exact search with --exact should NOT match "appie" because it requires contiguous substring
    {
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(41, m_testDirPath, QStringLiteral("appie"), {}, {QStringLiteral("--exact")}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("apple_pie.txt"))) {
                found = true;
                break;
            }
        }
        QVERIFY(!found);
    }

    // Test KSeekRunner environment parsing of KSEEK_FZF_ARGS
    qputenv("KSEEK_FZF_ARGS", "--exact -i");
    KSeekRunner runner(m_testDirPath);
    QCOMPARE(runner.extraFzfArgs(), QStringList({QStringLiteral("--exact"), QStringLiteral("-i")}));
    qunsetenv("KSEEK_FZF_ARGS");
}

QTEST_GUILESS_MAIN(TestRunner)
#include "test_runner.moc"

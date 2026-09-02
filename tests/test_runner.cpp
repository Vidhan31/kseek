#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QCommandLineParser>
#include <memory>

#include "types.h"
#include "config.h"
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
    void testArgSplitting();
    void testPrefixConfiguration();
    void testConfigEnvironmentAndCli();
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
    void testMultiRootConfigEnvironment();
    void testMultiRootConfigCli();
    void testMultiRootSearchPipeline();
    void testMultiRootMatchStructure();

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

void TestRunner::testArgSplitting() {
    // Basic unquoted split
    QCOMPARE(splitArgs(QStringLiteral("a b c")), QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));

    // Double quotes with spaces
    const QString doubleQuoted = QStringLiteral("--exclude \"My Documents\" --hidden");
    QCOMPARE(splitArgs(doubleQuoted), QStringList({QStringLiteral("--exclude"), QStringLiteral("My Documents"), QStringLiteral("--hidden")}));

    // Single quotes with wildcards
    const QString singleQuoted = QStringLiteral("-E '*.git' -E '*cache*'");
    QCOMPARE(splitArgs(singleQuoted), QStringList({QStringLiteral("-E"), QStringLiteral("*.git"), QStringLiteral("-E"), QStringLiteral("*cache*")}));

    // Backslash escapes
    const QString escaped = QStringLiteral("--path /foo\\ bar/baz\\ test");
    QCOMPARE(splitArgs(escaped), QStringList({QStringLiteral("--path"), QStringLiteral("/foo bar/baz test")}));

    // Mixed quotes & flags
    const QString mixed = QStringLiteral("--prompt=\"search> \" --preview='cat \"file\"' --exact");
    QCOMPARE(splitArgs(mixed), QStringList({QStringLiteral("--prompt=search> "), QStringLiteral("--preview=cat \"file\""), QStringLiteral("--exact")}));

    // Empty and whitespace strings
    QVERIFY(splitArgs(QStringLiteral("")).isEmpty());
    QVERIFY(splitArgs(QStringLiteral("   \t \n  ")).isEmpty());

    // Unclosed quote handling (should not crash)
    QCOMPARE(splitArgs(QStringLiteral("--foo \"bar")), QStringList({QStringLiteral("--foo"), QStringLiteral("bar")}));
}

void TestRunner::testPrefixConfiguration() {
    KSeekRunner runner(m_testDirPath);
    QString term;

    // Default prefix: 'f'
    QCOMPARE(runner.prefix(), QStringLiteral("f"));

    QVERIFY(runner.parseQuery(QStringLiteral("f test"), term));
    QCOMPARE(term, QStringLiteral("test"));

    // Case insensitivity
    QVERIFY(runner.parseQuery(QStringLiteral("F test"), term));
    QCOMPARE(term, QStringLiteral("test"));

    // Colon separator
    QVERIFY(runner.parseQuery(QStringLiteral("f:test"), term));
    QCOMPARE(term, QStringLiteral("test"));

    QVERIFY(runner.parseQuery(QStringLiteral("f: test"), term));
    QCOMPARE(term, QStringLiteral("test"));

    // Extra whitespace
    QVERIFY(runner.parseQuery(QStringLiteral("f   spaced query  "), term));
    QCOMPARE(term, QStringLiteral("spaced query"));

    // Negative tests for 'f'
    QVERIFY(!runner.parseQuery(QStringLiteral("firefox"), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("find test"), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("f"), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("f   "), term));

    // Custom word prefix: 'find'
    runner.setPrefix(QStringLiteral("find"));
    QCOMPARE(runner.prefix(), QStringLiteral("find"));

    QVERIFY(runner.parseQuery(QStringLiteral("find resume"), term));
    QCOMPARE(term, QStringLiteral("resume"));

    QVERIFY(runner.parseQuery(QStringLiteral("Find: resume"), term));
    QCOMPARE(term, QStringLiteral("resume"));

    QVERIFY(!runner.parseQuery(QStringLiteral("finding"), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("f resume"), term));

    // Symbol / punctuation prefix: '?'
    runner.setPrefix(QStringLiteral("?"));
    QCOMPARE(runner.prefix(), QStringLiteral("?"));

    QVERIFY(runner.parseQuery(QStringLiteral("?resume"), term));
    QCOMPARE(term, QStringLiteral("resume"));

    QVERIFY(runner.parseQuery(QStringLiteral("? resume"), term));
    QCOMPARE(term, QStringLiteral("resume"));

    QVERIFY(!runner.parseQuery(QStringLiteral("?"), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("?   "), term));

    // Regex special characters in prefix: '[f]'
    runner.setPrefix(QStringLiteral("[f]"));
    QVERIFY(runner.parseQuery(QStringLiteral("[f] resume"), term));
    QCOMPARE(term, QStringLiteral("resume"));
    QVERIFY(!runner.parseQuery(QStringLiteral("f resume"), term));

    // Empty / disabled prefix
    runner.setPrefix(QStringLiteral(""));
    QVERIFY(runner.prefix().isEmpty());
    QVERIFY(runner.parseQuery(QStringLiteral("resume"), term));
    QCOMPARE(term, QStringLiteral("resume"));
    QVERIFY(runner.parseQuery(QStringLiteral("  spaced query  "), term));
    QCOMPARE(term, QStringLiteral("spaced query"));
    QVERIFY(!runner.parseQuery(QStringLiteral(""), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("   "), term));

    // "none" keyword for disabled prefix
    runner.setPrefix(QStringLiteral("none"));
    QVERIFY(runner.prefix().isEmpty());
    QVERIFY(runner.parseQuery(QStringLiteral("anything.txt"), term));
    QCOMPARE(term, QStringLiteral("anything.txt"));
}

void TestRunner::testConfigEnvironmentAndCli() {
    // Test environment loading
    qputenv("KSEEK_PREFIX", "seek");
    qputenv("KSEEK_MAX_RESULTS", "42");
    qputenv("KSEEK_TIMEOUT", "4.0");
    qputenv("KSEEK_DEBOUNCE", "150");
    qputenv("KSEEK_FD_ARGS", "--exclude \"VirtualBox VMs\" --hidden");
    qputenv("KSEEK_FZF_ARGS", "--exact --prompt='> '");

    KSeekConfig envCfg = KSeekConfig::loadFromEnvironment();
    QCOMPARE(envCfg.prefix, QStringLiteral("seek"));
    QCOMPARE(envCfg.maxResults, 42);
    QCOMPARE(envCfg.timeoutMs, 4000);
    QCOMPARE(envCfg.debounceMs, 150);
    QCOMPARE(envCfg.extraFdArgs, QStringList({QStringLiteral("--exclude"), QStringLiteral("VirtualBox VMs"), QStringLiteral("--hidden")}));
    QCOMPARE(envCfg.extraFzfArgs, QStringList({QStringLiteral("--exact"), QStringLiteral("--prompt=> ")}));

    // Test fallback trigger variable
    qunsetenv("KSEEK_PREFIX");
    qputenv("KSEEK_TRIGGER", "trig");
    KSeekConfig trigCfg = KSeekConfig::loadFromEnvironment();
    QCOMPARE(trigCfg.prefix, QStringLiteral("trig"));
    qunsetenv("KSEEK_TRIGGER");

    // Clean up env
    qunsetenv("KSEEK_MAX_RESULTS");
    qunsetenv("KSEEK_TIMEOUT");
    qunsetenv("KSEEK_DEBOUNCE");
    qunsetenv("KSEEK_FD_ARGS");
    qunsetenv("KSEEK_FZF_ARGS");

    // Test CLI argument override
    QCommandLineParser parser;
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("p"), QStringLiteral("prefix")}, QString(), QStringLiteral("prefix")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("r"), QStringLiteral("root")}, QString(), QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("m"), QStringLiteral("max-results")}, QString(), QStringLiteral("count")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("t"), QStringLiteral("timeout")}, QString(), QStringLiteral("seconds")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("d"), QStringLiteral("debounce")}, QString(), QStringLiteral("ms")));
    parser.addOption(QCommandLineOption(QStringLiteral("fd-args"), QString(), QStringLiteral("args")));
    parser.addOption(QCommandLineOption(QStringLiteral("fzf-args"), QString(), QStringLiteral("args")));
    parser.addOption(QCommandLineOption(QStringLiteral("fd-bin"), QString(), QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringLiteral("fzf-bin"), QString(), QStringLiteral("path")));
    parser.addOption(QCommandLineOption(QStringLiteral("debug")));
    parser.addOption(QCommandLineOption(QStringLiteral("replace")));

    QStringList cliArgs = {
        QStringLiteral("kseek"),
        QStringLiteral("-p"), QStringLiteral("find"),
        QStringLiteral("-m"), QStringLiteral("100"),
        QStringLiteral("--fd-args"), QStringLiteral("-E \"*.tmp\""),
        QStringLiteral("--debug"),
        QStringLiteral("--replace")
    };
    parser.parse(cliArgs);

    KSeekConfig cliCfg = KSeekConfig::loadFromEnvironment();
    cliCfg.applyCommandLine(parser);

    QCOMPARE(cliCfg.prefix, QStringLiteral("find"));
    QCOMPARE(cliCfg.maxResults, 100);
    QCOMPARE(cliCfg.extraFdArgs, QStringList({QStringLiteral("-E"), QStringLiteral("*.tmp")}));
    QVERIFY(cliCfg.debug);
    QVERIFY(cliCfg.replaceExisting);
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

    // Test KSeekRunner environment parsing of KSEEK_FZF_ARGS with quotes
    qputenv("KSEEK_FZF_ARGS", "--exact -i --prompt=\"find> \"");
    KSeekRunner runner(m_testDirPath);
    QCOMPARE(runner.extraFzfArgs(), QStringList({QStringLiteral("--exact"), QStringLiteral("-i"), QStringLiteral("--prompt=find> ")}));
    qunsetenv("KSEEK_FZF_ARGS");
}

void TestRunner::testMultiRootConfigEnvironment() {
    QTemporaryDir tempDir1;
    QTemporaryDir tempDir2;
    QVERIFY(tempDir1.isValid());
    QVERIFY(tempDir2.isValid());

    const QString path1 = QFileInfo(tempDir1.path()).canonicalFilePath();
    const QString path2 = QFileInfo(tempDir2.path()).canonicalFilePath();

    // 1. Colon-separated list in KSEEK_ROOT
    const QString joined = path1 + QStringLiteral(":") + path2;
    qputenv("KSEEK_ROOT", joined.toUtf8());

    KSeekConfig envCfg = KSeekConfig::loadFromEnvironment();
    QCOMPARE(envCfg.searchRoots.size(), 2);
    QCOMPARE(envCfg.searchRoots.at(0), path1);
    QCOMPARE(envCfg.searchRoots.at(1), path2);
    QCOMPARE(envCfg.searchRoot(), path1);

    // 2. Fallback to KRUNNER_FZF_FD_ROOT
    qunsetenv("KSEEK_ROOT");
    qputenv("KRUNNER_FZF_FD_ROOT", joined.toUtf8());
    KSeekConfig fallbackCfg = KSeekConfig::loadFromEnvironment();
    QCOMPARE(fallbackCfg.searchRoots.size(), 2);
    QCOMPARE(fallbackCfg.searchRoots.at(0), path1);
    QCOMPARE(fallbackCfg.searchRoots.at(1), path2);
    qunsetenv("KRUNNER_FZF_FD_ROOT");

    // 3. Tilde expansion
    qputenv("KSEEK_ROOT", "~");
    KSeekConfig tildeCfg = KSeekConfig::loadFromEnvironment();
    const QString canonicalHome = QFileInfo(QDir::homePath()).canonicalFilePath();
    QCOMPARE(tildeCfg.searchRoots.size(), 1);
    QCOMPARE(tildeCfg.searchRoots.first(), canonicalHome.isEmpty() ? QDir::homePath() : canonicalHome);

    // 4. Deduplication
    const QString dupJoined = path1 + QStringLiteral(":") + path1 + QStringLiteral(":") + path2;
    qputenv("KSEEK_ROOT", dupJoined.toUtf8());
    KSeekConfig dupCfg = KSeekConfig::loadFromEnvironment();
    QCOMPARE(dupCfg.searchRoots.size(), 2);
    QCOMPARE(dupCfg.searchRoots.at(0), path1);
    QCOMPARE(dupCfg.searchRoots.at(1), path2);

    // 5. Non-existent path pruning
    const QString nonExistent = QStringLiteral("/non_existent_dir_123456789");
    const QString mixedJoined = nonExistent + QStringLiteral(":") + path2;
    qputenv("KSEEK_ROOT", mixedJoined.toUtf8());
    KSeekConfig mixedCfg = KSeekConfig::loadFromEnvironment();
    QCOMPARE(mixedCfg.searchRoots.size(), 1);
    QCOMPARE(mixedCfg.searchRoots.first(), path2);

    // 6. All non-existent -> fallback to home
    qputenv("KSEEK_ROOT", nonExistent.toUtf8());
    KSeekConfig noneCfg = KSeekConfig::loadFromEnvironment();
    QCOMPARE(noneCfg.searchRoots.size(), 1);
    QCOMPARE(noneCfg.searchRoots.first(), canonicalHome.isEmpty() ? QDir::homePath() : canonicalHome);

    qunsetenv("KSEEK_ROOT");
}

void TestRunner::testMultiRootConfigCli() {
    QTemporaryDir tempDir1;
    QTemporaryDir tempDir2;
    QTemporaryDir tempDir3;
    QVERIFY(tempDir1.isValid());
    QVERIFY(tempDir2.isValid());
    QVERIFY(tempDir3.isValid());

    const QString path1 = QFileInfo(tempDir1.path()).canonicalFilePath();
    const QString path2 = QFileInfo(tempDir2.path()).canonicalFilePath();
    const QString path3 = QFileInfo(tempDir3.path()).canonicalFilePath();

    auto setupParser = [](QCommandLineParser &parser) {
        parser.addOption(QCommandLineOption(QStringList{QStringLiteral("p"), QStringLiteral("prefix")}, QString(), QStringLiteral("prefix")));
        parser.addOption(QCommandLineOption(QStringList{QStringLiteral("r"), QStringLiteral("root")}, QString(), QStringLiteral("path")));
        parser.addOption(QCommandLineOption(QStringList{QStringLiteral("m"), QStringLiteral("max-results")}, QString(), QStringLiteral("count")));
        parser.addOption(QCommandLineOption(QStringList{QStringLiteral("t"), QStringLiteral("timeout")}, QString(), QStringLiteral("seconds")));
        parser.addOption(QCommandLineOption(QStringList{QStringLiteral("d"), QStringLiteral("debounce")}, QString(), QStringLiteral("ms")));
        parser.addOption(QCommandLineOption(QStringLiteral("fd-args"), QString(), QStringLiteral("args")));
        parser.addOption(QCommandLineOption(QStringLiteral("fzf-args"), QString(), QStringLiteral("args")));
        parser.addOption(QCommandLineOption(QStringLiteral("fd-bin"), QString(), QStringLiteral("path")));
        parser.addOption(QCommandLineOption(QStringLiteral("fzf-bin"), QString(), QStringLiteral("path")));
        parser.addOption(QCommandLineOption(QStringLiteral("debug")));
        parser.addOption(QCommandLineOption(QStringLiteral("replace")));
    };

    // 1. Multiple -r flags
    {
        QCommandLineParser parser;
        setupParser(parser);
        QStringList cliArgs = {
            QStringLiteral("kseek"),
            QStringLiteral("-r"), path1,
            QStringLiteral("-r"), path2
        };
        parser.parse(cliArgs);

        KSeekConfig cfg = KSeekConfig::loadFromEnvironment();
        cfg.applyCommandLine(parser);
        QCOMPARE(cfg.searchRoots.size(), 2);
        QCOMPARE(cfg.searchRoots.at(0), path1);
        QCOMPARE(cfg.searchRoots.at(1), path2);
    }

    // 2. Colon-separated in single --root flag
    {
        QCommandLineParser parser;
        setupParser(parser);
        QStringList cliArgs = {
            QStringLiteral("kseek"),
            QStringLiteral("--root"), path1 + QStringLiteral(":") + path2 + QStringLiteral(":") + path3
        };
        parser.parse(cliArgs);

        KSeekConfig cfg = KSeekConfig::loadFromEnvironment();
        cfg.applyCommandLine(parser);
        QCOMPARE(cfg.searchRoots.size(), 3);
        QCOMPARE(cfg.searchRoots.at(0), path1);
        QCOMPARE(cfg.searchRoots.at(1), path2);
        QCOMPARE(cfg.searchRoots.at(2), path3);
    }

    // 3. Combined multiple flags and colon-separated
    {
        QCommandLineParser parser;
        setupParser(parser);
        QStringList cliArgs = {
            QStringLiteral("kseek"),
            QStringLiteral("-r"), path1,
            QStringLiteral("--root"), path2 + QStringLiteral(":") + path3
        };
        parser.parse(cliArgs);

        KSeekConfig cfg = KSeekConfig::loadFromEnvironment();
        cfg.applyCommandLine(parser);
        QCOMPARE(cfg.searchRoots.size(), 3);
        QCOMPARE(cfg.searchRoots.at(0), path1);
        QCOMPARE(cfg.searchRoots.at(1), path2);
        QCOMPARE(cfg.searchRoots.at(2), path3);
    }
}

void TestRunner::testMultiRootSearchPipeline() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    QTemporaryDir tempDirA;
    QTemporaryDir tempDirB;
    QVERIFY(tempDirA.isValid());
    QVERIFY(tempDirB.isValid());

    const QString pathA = QFileInfo(tempDirA.path()).canonicalFilePath();
    const QString pathB = QFileInfo(tempDirB.path()).canonicalFilePath();

    // Create unique files in each temp dir
    QFile fileA(pathA + QStringLiteral("/alpha_project_note.md"));
    QVERIFY(fileA.open(QIODevice::WriteOnly | QIODevice::Text));
    fileA.write("Alpha content\n");
    fileA.close();

    QFile fileB(pathB + QStringLiteral("/beta_project_design.txt"));
    QVERIFY(fileB.open(QIODevice::WriteOnly | QIODevice::Text));
    fileB.write("Beta content\n");
    fileB.close();

    // Search across both roots for "project"
    QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
    pipeline.startSearch(100, QStringList{pathA, pathB}, QStringLiteral("project"), {}, {}, 20, 2000);

    QVERIFY(finishedSpy.wait(3000));
    QCOMPARE(finishedSpy.count(), 1);

    const auto args = finishedSpy.takeFirst();
    QCOMPARE(args.at(0).toULongLong(), 100ULL);

    const QStringList results = args.at(1).toStringList();
    QCOMPARE(results.size(), 2);

    bool foundA = false;
    bool foundB = false;
    for (const QString &p : results) {
        if (p.contains(QLatin1StringView("alpha_project_note.md")) && p.contains(pathA)) {
            foundA = true;
        }
        if (p.contains(QLatin1StringView("beta_project_design.txt")) && p.contains(pathB)) {
            foundB = true;
        }
    }
    QVERIFY(foundA);
    QVERIFY(foundB);
}

void TestRunner::testMultiRootMatchStructure() {
    QTemporaryDir tempDirA;
    QTemporaryDir tempDirB;
    QVERIFY(tempDirA.isValid());
    QVERIFY(tempDirB.isValid());

    const QString pathA = QFileInfo(tempDirA.path()).canonicalFilePath();
    const QString pathB = QFileInfo(tempDirB.path()).canonicalFilePath();

    const QString fileAPath = pathA + QStringLiteral("/project_alpha.cpp");
    QFile fileA(fileAPath);
    QVERIFY(fileA.open(QIODevice::WriteOnly | QIODevice::Text));
    fileA.write("code\n");
    fileA.close();

    const QString fileBPath = pathB + QStringLiteral("/project_beta.h");
    QFile fileB(fileBPath);
    QVERIFY(fileB.open(QIODevice::WriteOnly | QIODevice::Text));
    fileB.write("code\n");
    fileB.close();

    KSeekRunner runner(QStringList{pathA, pathB});
    QCOMPARE(runner.searchRoots().size(), 2);
    QCOMPARE(runner.searchRoots().at(0), pathA);
    QCOMPARE(runner.searchRoots().at(1), pathB);

    RemoteMatch matchA = runner.buildMatch(fileAPath, 0, 2);
    QVERIFY(!matchA.id.isEmpty());
    QCOMPARE(matchA.id, fileAPath);
    QCOMPARE(matchA.text, QStringLiteral("project_alpha.cpp"));
    QCOMPARE(matchA.properties.value(QStringLiteral("subtext")).toString(), fileAPath);
    QVERIFY(matchA.properties.value(QStringLiteral("urls")).toStringList().first().contains(QLatin1StringView("project_alpha.cpp")));

    RemoteMatch matchB = runner.buildMatch(fileBPath, 1, 2);
    QVERIFY(!matchB.id.isEmpty());
    QCOMPARE(matchB.id, fileBPath);
    QCOMPARE(matchB.text, QStringLiteral("project_beta.h"));
    QCOMPARE(matchB.properties.value(QStringLiteral("subtext")).toString(), fileBPath);
    QVERIFY(matchB.properties.value(QStringLiteral("urls")).toStringList().first().contains(QLatin1StringView("project_beta.h")));
}

QTEST_GUILESS_MAIN(TestRunner)
#include "test_runner.moc"

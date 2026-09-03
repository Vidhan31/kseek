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
    void testArgSplittingExtended();
    void testPrefixFastPathParsing();
    void testMultiRootTokenizerEdgeCases();
    void testSpecificMultiRootsFedowinAndHome();
    void testEdgeCaseMatchConstruction();
    void testEdgeCaseMatchConstruction_data();
    void testEdgeCasePipelineDelimitation();
    void testEdgeCasePipelineDelimitation_data();
    void testEdgeCaseConfigCombinations();
    void testCoreParsingEdgeCases();

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

    struct EnvCleaner {
        ~EnvCleaner() {
            qunsetenv("FZF_DEFAULT_OPTS");
            qunsetenv("FZF_DEFAULT_COMMAND");
        }
    } cleaner;

    // 1. Verify that presentation and terminal options in FZF_DEFAULT_OPTS do not break --filter
    qputenv("FZF_DEFAULT_OPTS", "--layout=reverse --border --height=40% --preview 'cat {}'");
    qputenv("FZF_DEFAULT_COMMAND", "false");

    {
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
    }

    // 2. Verify that --print-query in FZF_DEFAULT_OPTS is neutralized by --no-print-query
    qputenv("FZF_DEFAULT_OPTS", "--print-query");
    {
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(31, m_testDirPath, QStringLiteral("README"), {}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        QCOMPARE(finishedSpy.count(), 1);

        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        QVERIFY(!results.contains(QStringLiteral("README")));
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("README.md"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    // 3. Verify that user matching options in FZF_DEFAULT_OPTS (e.g. --exact) are respected
    qputenv("FZF_DEFAULT_OPTS", "--exact");
    {
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(32, m_testDirPath, QStringLiteral("rdm"), {}, {}, 20, 2000);
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
        QVERIFY(!found);
    }
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

void TestRunner::testArgSplittingExtended() {
    // Escaped double quotes inside double quotes
    const QString escapedDquote = QStringLiteral("--message=\"Hello \\\"world\\\"\"");
    QCOMPARE(splitArgs(escapedDquote), QStringList({QStringLiteral("--message=Hello \"world\"")}));

    // Single quotes inside double quotes
    const QString singleInDouble = QStringLiteral("--message=\"It's a test\"");
    QCOMPARE(splitArgs(singleInDouble), QStringList({QStringLiteral("--message=It's a test")}));

    // Adjacent quoted strings forming a single token
    const QString adjacentQuotes = QStringLiteral("--flag=\"foo\"'bar'");
    QCOMPARE(splitArgs(adjacentQuotes), QStringList({QStringLiteral("--flag=foobar")}));

    // Multiple spaces between arguments
    const QString multiSpaces = QStringLiteral("  -a    --long-option   value  ");
    QCOMPARE(splitArgs(multiSpaces), QStringList({QStringLiteral("-a"), QStringLiteral("--long-option"), QStringLiteral("value")}));

    // Trailing backslash tolerance
    const QString trailingSlash = QStringLiteral("arg1 arg2\\");
    QCOMPARE(splitArgs(trailingSlash), QStringList({QStringLiteral("arg1"), QStringLiteral("arg2\\")}));

    // Empty string
    QVERIFY(splitArgs(QString()).isEmpty());
    QVERIFY(splitArgs(QStringLiteral("     ")).isEmpty());
}

void TestRunner::testPrefixFastPathParsing() {
    KSeekRunner runner;
    QString term;

    // Single char alphanumeric prefix 'f'
    runner.setPrefix(QStringLiteral("f"));
    QVERIFY(runner.parseQuery(QStringLiteral("f hello"), term));
    QCOMPARE(term, QStringLiteral("hello"));
    QVERIFY(runner.parseQuery(QStringLiteral("F hello"), term));
    QCOMPARE(term, QStringLiteral("hello"));
    QVERIFY(runner.parseQuery(QStringLiteral("f:hello"), term));
    QCOMPARE(term, QStringLiteral("hello"));
    QVERIFY(runner.parseQuery(QStringLiteral("f: hello"), term));
    QCOMPARE(term, QStringLiteral("hello"));
    QVERIFY(runner.parseQuery(QStringLiteral("F: hello"), term));
    QCOMPARE(term, QStringLiteral("hello"));
    QVERIFY(runner.parseQuery(QStringLiteral("f\thello"), term));
    QCOMPARE(term, QStringLiteral("hello"));

    // Multi-char alphanumeric prefix 'find'
    runner.setPrefix(QStringLiteral("find"));
    QVERIFY(runner.parseQuery(QStringLiteral("find my document.pdf"), term));
    QCOMPARE(term, QStringLiteral("my document.pdf"));
    QVERIFY(runner.parseQuery(QStringLiteral("Find: my document.pdf"), term));
    QCOMPARE(term, QStringLiteral("my document.pdf"));
    QVERIFY(!runner.parseQuery(QStringLiteral("finding something"), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("find"), term));
    QVERIFY(!runner.parseQuery(QStringLiteral("find   "), term));

    // Symbol prefix '?'
    runner.setPrefix(QStringLiteral("?"));
    QVERIFY(runner.parseQuery(QStringLiteral("?test"), term));
    QCOMPARE(term, QStringLiteral("test"));
    QVERIFY(runner.parseQuery(QStringLiteral("? test"), term));
    QCOMPARE(term, QStringLiteral("test"));
    QVERIFY(!runner.parseQuery(QStringLiteral("?"), term));

    // Disabled prefix
    runner.setPrefix(QStringLiteral("-"));
    QVERIFY(runner.prefix().isEmpty());
    QVERIFY(runner.parseQuery(QStringLiteral("raw query text"), term));
    QCOMPARE(term, QStringLiteral("raw query text"));
}

void TestRunner::testMultiRootTokenizerEdgeCases() {
    QTemporaryDir temp1;
    QTemporaryDir temp2;
    QVERIFY(temp1.isValid() && temp2.isValid());

    const QString p1 = QFileInfo(temp1.path()).canonicalFilePath();
    const QString p2 = QFileInfo(temp2.path()).canonicalFilePath();

    // Mixed colons and semicolons with empty segments
    const QString input = QStringLiteral(":::") + p1 + QStringLiteral(";;;:") + p2 + QStringLiteral(":::");
    const QStringList roots = parseSearchRoots(input);
    QCOMPARE(roots.size(), 2);
    QCOMPARE(roots.at(0), p1);
    QCOMPARE(roots.at(1), p2);

    // Whitespace padded entries
    const QString padded = QStringLiteral("  ") + p1 + QStringLiteral("  :  ") + p2 + QStringLiteral("  ");
    const QStringList paddedRoots = parseSearchRoots(padded);
    QCOMPARE(paddedRoots.size(), 2);
    QCOMPARE(paddedRoots.at(0), p1);
    QCOMPARE(paddedRoots.at(1), p2);
}

void TestRunner::testSpecificMultiRootsFedowinAndHome() {
    const QString fedowinPath = QStringLiteral("/mnt/fedowin");
    const QString homeDevPath = QStringLiteral("/home/dev");

    // Verify parser handles /mnt/fedowin and /home/dev/ (with trailing slash)
    const QStringList parsed = parseSearchRoots(QStringList{fedowinPath, homeDevPath + QStringLiteral("/")});
    if (QDir(fedowinPath).exists() && QDir(homeDevPath).exists()) {
        const QString canonFedowin = QFileInfo(fedowinPath).canonicalFilePath();
        const QString canonHomeDev = QFileInfo(homeDevPath).canonicalFilePath();

        QCOMPARE(parsed.size(), 2);
        QCOMPARE(parsed.at(0), canonFedowin);
        QCOMPARE(parsed.at(1), canonHomeDev);

        // Test colon-separated format
        const QStringList parsedColon = parseSearchRoots(fedowinPath + QStringLiteral(":") + homeDevPath + QStringLiteral("/"));
        QCOMPARE(parsedColon.size(), 2);
        QCOMPARE(parsedColon.at(0), canonFedowin);
        QCOMPARE(parsedColon.at(1), canonHomeDev);

        // Test pipeline search across both roots
        ProcessPipeline pipeline;
        if (pipeline.isAvailable()) {
            QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
            pipeline.startSearch(501, parsed, QStringLiteral("kseek"), {}, {}, 20, 2500);
            QVERIFY(finishedSpy.wait(3000));
            QCOMPARE(finishedSpy.count(), 1);

            const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
            QVERIFY(!results.isEmpty());
            bool foundKseekInHome = false;
            for (const QString &p : results) {
                if (p.contains(canonHomeDev) && p.contains(QLatin1StringView("kseek"))) {
                    foundKseekInHome = true;
                    break;
                }
            }
            QVERIFY(foundKseekInHome);
        }

        // Test KSeekRunner buildMatch
        KSeekRunner runner(parsed);
        QCOMPARE(runner.searchRoots().size(), 2);

        // Match for a file under /mnt/fedowin
        const QString fedowinSample = canonFedowin + QStringLiteral("/Backup");
        if (QFileInfo::exists(fedowinSample)) {
            RemoteMatch match = runner.buildMatch(fedowinSample, 0, 1);
            QVERIFY(!match.id.isEmpty());
            QCOMPARE(match.id, fedowinSample);
            QCOMPARE(match.text, QStringLiteral("Backup"));
            QCOMPARE(match.properties.value(QStringLiteral("subtext")).toString(), fedowinSample);
        }

        // Match for a file under /home/dev
        const QString homeSample = canonHomeDev + QStringLiteral("/Projects");
        if (QFileInfo::exists(homeSample)) {
            RemoteMatch match = runner.buildMatch(homeSample, 0, 1);
            QVERIFY(!match.id.isEmpty());
            QCOMPARE(match.id, homeSample);
            QCOMPARE(match.text, QStringLiteral("Projects"));
            QCOMPARE(match.properties.value(QStringLiteral("subtext")).toString(), homeSample);
        }
    }
}

static QString edgeCasesRoot() {
#ifdef KSEEK_SOURCE_DIR
    const QString srcDir = QStringLiteral(KSEEK_SOURCE_DIR) + QStringLiteral("/tests/edge_cases");
    if (QDir(srcDir).exists()) {
        return QFileInfo(srcDir).canonicalFilePath();
    }
#endif
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList fallbacks = {
        appDir + QStringLiteral("/../../tests/edge_cases"),
        appDir + QStringLiteral("/../tests/edge_cases"),
        QStringLiteral("/home/dev/Projects/kseek/tests/edge_cases")
    };
    for (const auto &p : fallbacks) {
        if (QDir(p).exists()) {
            return QFileInfo(p).canonicalFilePath();
        }
    }
    return QString();
}

void TestRunner::testEdgeCaseMatchConstruction_data() {
    QTest::addColumn<QString>("relPath");
    QTest::addColumn<QString>("expectedLeafText");
    QTest::addColumn<bool>("isDir");

    // Whitespace & Spacing
    QTest::newRow("spaces_dir") << QStringLiteral("01_whitespace_and_spacing/folder with spaces") << QStringLiteral("folder with spaces") << true;
    QTest::newRow("spaces_inner_file") << QStringLiteral("01_whitespace_and_spacing/folder with spaces/inner file with space.txt") << QStringLiteral("inner file with space.txt") << false;
    QTest::newRow("multi_spaces_dir") << QStringLiteral("01_whitespace_and_spacing/multiple   spaces   dir") << QStringLiteral("multiple   spaces   dir") << true;
    QTest::newRow("multi_spaces_file") << QStringLiteral("01_whitespace_and_spacing/multiple   spaces   dir/spaced   inner   file.log") << QStringLiteral("spaced   inner   file.log") << false;
    QTest::newRow("leading_space_dir") << QStringLiteral("01_whitespace_and_spacing/ leading_space_dir") << QStringLiteral(" leading_space_dir") << true;
    QTest::newRow("leading_space_file") << QStringLiteral("01_whitespace_and_spacing/ leading_space.txt") << QStringLiteral(" leading_space.txt") << false;
    QTest::newRow("trailing_space_file") << QStringLiteral("01_whitespace_and_spacing/trailing_space.txt ") << QStringLiteral("trailing_space.txt ") << false;
    QTest::newRow("tab_separated_file") << QStringLiteral("01_whitespace_and_spacing/tab\tseparated.tsv") << QStringLiteral("tab\tseparated.tsv") << false;
    QTest::newRow("mixed_spacing_doc") << QStringLiteral("01_whitespace_and_spacing/mixed _ spacing _ and _ underscores .doc") << QStringLiteral("mixed _ spacing _ and _ underscores .doc") << false;

    // Brackets & Special Chars
    QTest::newRow("brackets_year_report") << QStringLiteral("02_special_and_shell_chars/brackets_and_parens/[2024] financial_report [Q1-Q4].pdf") << QStringLiteral("[2024] financial_report [Q1-Q4].pdf") << false;
    QTest::newRow("parentheses_copy") << QStringLiteral("02_special_and_shell_chars/brackets_and_parens/document (copy) (1).docx") << QStringLiteral("document (copy) (1).docx") << false;
    QTest::newRow("braces_config") << QStringLiteral("02_special_and_shell_chars/brackets_and_parens/{config_template}.json") << QStringLiteral("{config_template}.json") << false;
    QTest::newRow("array_indices_cpp") << QStringLiteral("02_special_and_shell_chars/brackets_and_parens/array[0][1].cpp") << QStringLiteral("array[0][1].cpp") << false;
    QTest::newRow("angle_brackets_xml") << QStringLiteral("02_special_and_shell_chars/brackets_and_parens/<angle_brackets>.xml") << QStringLiteral("<angle_brackets>.xml") << false;

    // Quotes & Escapes
    QTest::newRow("apostrophe_odt") << QStringLiteral("02_special_and_shell_chars/quotes_and_escapes/john's report.odt") << QStringLiteral("john's report.odt") << false;
    QTest::newRow("single_quoted_txt") << QStringLiteral("02_special_and_shell_chars/quotes_and_escapes/'single_quoted'.txt") << QStringLiteral("'single_quoted'.txt") << false;
    QTest::newRow("double_quoted_txt") << QStringLiteral("02_special_and_shell_chars/quotes_and_escapes/\"double_quoted\".txt") << QStringLiteral("\"double_quoted\".txt") << false;
    QTest::newRow("backtick_command_sh") << QStringLiteral("02_special_and_shell_chars/quotes_and_escapes/`backtick_command`.sh") << QStringLiteral("`backtick_command`.sh") << false;
    QTest::newRow("quote_in_middle_txt") << QStringLiteral("02_special_and_shell_chars/quotes_and_escapes/quote\"in'middle.txt") << QStringLiteral("quote\"in'middle.txt") << false;

    // Shell Operators
    QTest::newRow("ampersand_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/file_with_&_ampersand.txt") << QStringLiteral("file_with_&_ampersand.txt") << false;
    QTest::newRow("semicolon_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/semicolon;separated.txt") << QStringLiteral("semicolon;separated.txt") << false;
    QTest::newRow("pipe_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/pipe|file.txt") << QStringLiteral("pipe|file.txt") << false;
    QTest::newRow("dollar_var_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/dollar_$HOME_var.txt") << QStringLiteral("dollar_$HOME_var.txt") << false;
    QTest::newRow("eval_sh") << QStringLiteral("02_special_and_shell_chars/shell_operators/eval_$(whoami).sh") << QStringLiteral("eval_$(whoami).sh") << false;
    QTest::newRow("asterisk_glob_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/asterisk_*_glob.txt") << QStringLiteral("asterisk_*_glob.txt") << false;
    QTest::newRow("question_mark_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/question_?_mark.txt") << QStringLiteral("question_?_mark.txt") << false;
    QTest::newRow("exclamation_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/exclamation!mark.txt") << QStringLiteral("exclamation!mark.txt") << false;
    QTest::newRow("hash_tag_md") << QStringLiteral("02_special_and_shell_chars/shell_operators/hash_#tag_file.md") << QStringLiteral("hash_#tag_file.md") << false;
    QTest::newRow("percent_encoded_html") << QStringLiteral("02_special_and_shell_chars/shell_operators/percent%20encoded.html") << QStringLiteral("percent%20encoded.html") << false;
    QTest::newRow("at_user_json") << QStringLiteral("02_special_and_shell_chars/shell_operators/at_@user.json") << QStringLiteral("at_@user.json") << false;
    QTest::newRow("caret_power_txt") << QStringLiteral("02_special_and_shell_chars/shell_operators/caret^power.txt") << QStringLiteral("caret^power.txt") << false;
    QTest::newRow("tilde_backup_bak") << QStringLiteral("02_special_and_shell_chars/shell_operators/tilde~backup.bak") << QStringLiteral("tilde~backup.bak") << false;
    QTest::newRow("math_calc") << QStringLiteral("02_special_and_shell_chars/shell_operators/math+plus=minus-sign.calc") << QStringLiteral("math+plus=minus-sign.calc") << false;
    QTest::newRow("comma_csv") << QStringLiteral("02_special_and_shell_chars/shell_operators/comma,separated,values.csv") << QStringLiteral("comma,separated,values.csv") << false;

    // CLI Flags & Hyphens
    QTest::newRow("flag_txt") << QStringLiteral("02_special_and_shell_chars/cli_flags_and_hyphens/-flag.txt") << QStringLiteral("-flag.txt") << false;
    QTest::newRow("help_flag_txt") << QStringLiteral("02_special_and_shell_chars/cli_flags_and_hyphens/--help.txt") << QStringLiteral("--help.txt") << false;
    QTest::newRow("rf_flag_txt") << QStringLiteral("02_special_and_shell_chars/cli_flags_and_hyphens/-rf.txt") << QStringLiteral("-rf.txt") << false;
    QTest::newRow("dash_dir") << QStringLiteral("02_special_and_shell_chars/cli_flags_and_hyphens/-dash_folder") << QStringLiteral("-dash_folder") << true;
    QTest::newRow("nested_flag_dir") << QStringLiteral("02_special_and_shell_chars/cli_flags_and_hyphens/--nested_flag_folder") << QStringLiteral("--nested_flag_folder") << true;

    // Unicode & i18n
    QTest::newRow("cafe_pdf") << QStringLiteral("03_unicode_and_i18n/latin_accents/café_menu.pdf") << QStringLiteral("café_menu.pdf") << false;
    QTest::newRow("resume_docx") << QStringLiteral("03_unicode_and_i18n/latin_accents/résumé_2025.docx") << QStringLiteral("résumé_2025.docx") << false;
    QTest::newRow("munchen_txt") << QStringLiteral("03_unicode_and_i18n/latin_accents/München_über_alles.txt") << QStringLiteral("München_über_alles.txt") << false;
    QTest::newRow("naive_py") << QStringLiteral("03_unicode_and_i18n/latin_accents/naïve_bayes_model.py") << QStringLiteral("naïve_bayes_model.py") << false;
    QTest::newRow("sao_paulo_xlsx") << QStringLiteral("03_unicode_and_i18n/latin_accents/São_Paulo_relatório.xlsx") << QStringLiteral("São_Paulo_relatório.xlsx") << false;
    QTest::newRow("facade_kt") << QStringLiteral("03_unicode_and_i18n/latin_accents/façade_pattern.kt") << QStringLiteral("façade_pattern.kt") << false;
    QTest::newRow("smorgasbord_recipe") << QStringLiteral("03_unicode_and_i18n/latin_accents/smörgåsbord.recipe") << QStringLiteral("smörgåsbord.recipe") << false;
    QTest::newRow("kobenhavn_md") << QStringLiteral("03_unicode_and_i18n/latin_accents/København_guide.md") << QStringLiteral("København_guide.md") << false;
    QTest::newRow("cyrillic_doc_pdf") << QStringLiteral("03_unicode_and_i18n/cyrillic/документ_отчет.pdf") << QStringLiteral("документ_отчет.pdf") << false;
    QTest::newRow("cyrillic_dir") << QStringLiteral("03_unicode_and_i18n/cyrillic/кириллица_папка") << QStringLiteral("кириллица_папка") << true;
    QTest::newRow("chinese_doc_md") << QStringLiteral("03_unicode_and_i18n/cjk_asian/chinese_中文/项目文档.md") << QStringLiteral("项目文档.md") << false;
    QTest::newRow("japanese_doc_txt") << QStringLiteral("03_unicode_and_i18n/cjk_asian/japanese_日本語/日本語ドキュメント.txt") << QStringLiteral("日本語ドキュメント.txt") << false;
    QTest::newRow("korean_doc_docx") << QStringLiteral("03_unicode_and_i18n/cjk_asian/korean_한국어/프로젝트_기획서.docx") << QStringLiteral("프로젝트_기획서.docx") << false;
    QTest::newRow("indic_doc_txt") << QStringLiteral("03_unicode_and_i18n/indic_devanagari/दस्तावेज़.txt") << QStringLiteral("दस्तावेज़.txt") << false;
    QTest::newRow("greek_notes_txt") << QStringLiteral("03_unicode_and_i18n/greek/ελληνικά_σημειώσεις.txt") << QStringLiteral("ελληνικά_σημειώσεις.txt") << false;
    QTest::newRow("arabic_pdf") << QStringLiteral("03_unicode_and_i18n/rtl_scripts/arabic_مستند/تقرير_مالي.pdf") << QStringLiteral("تقرير_مالي.pdf") << false;
    QTest::newRow("hebrew_txt") << QStringLiteral("03_unicode_and_i18n/rtl_scripts/hebrew_עברית/מסמך_בדיקה.txt") << QStringLiteral("מסמך_בדיקה.txt") << false;
    QTest::newRow("emoji_rocket_yaml") << QStringLiteral("03_unicode_and_i18n/emojis_and_symbols/🚀_launch_manifest.yaml") << QStringLiteral("🚀_launch_manifest.yaml") << false;
    QTest::newRow("emoji_hotfix_patch") << QStringLiteral("03_unicode_and_i18n/emojis_and_symbols/🔥_hotfix.patch") << QStringLiteral("🔥_hotfix.patch") << false;
    QTest::newRow("emoji_favorites_json") << QStringLiteral("03_unicode_and_i18n/emojis_and_symbols/❤️_favorites.json") << QStringLiteral("❤️_favorites.json") << false;
    QTest::newRow("emoji_docs_dir") << QStringLiteral("03_unicode_and_i18n/emojis_and_symbols/📁_documents") << QStringLiteral("📁_documents") << true;

    // Extensionless & Compounds
    QTest::newRow("no_ext_makefile") << QStringLiteral("05_extensions_and_names/no_extension/Makefile") << QStringLiteral("Makefile") << false;
    QTest::newRow("no_ext_dockerfile") << QStringLiteral("05_extensions_and_names/no_extension/Dockerfile") << QStringLiteral("Dockerfile") << false;
    QTest::newRow("no_ext_license") << QStringLiteral("05_extensions_and_names/no_extension/LICENSE") << QStringLiteral("LICENSE") << false;
    QTest::newRow("uppercase_ext_png") << QStringLiteral("05_extensions_and_names/uppercase_and_mixed_extensions/IMAGE.PNG") << QStringLiteral("IMAGE.PNG") << false;
    QTest::newRow("compound_tar_gz") << QStringLiteral("05_extensions_and_names/compound_extensions/archive.tar.gz") << QStringLiteral("archive.tar.gz") << false;
    QTest::newRow("compound_min_js") << QStringLiteral("05_extensions_and_names/compound_extensions/bundle.min.js") << QStringLiteral("bundle.min.js") << false;
    QTest::newRow("compound_d_ts") << QStringLiteral("05_extensions_and_names/compound_extensions/component.module.css.d.ts") << QStringLiteral("component.module.css.d.ts") << false;
    QTest::newRow("trailing_dot") << QStringLiteral("05_extensions_and_names/trailing_dots/ending_with_dot.") << QStringLiteral("ending_with_dot.") << false;

    // Case Sensitivity
    QTest::newRow("case_lower_txt") << QStringLiteral("06_case_sensitivity/lowercase.txt") << QStringLiteral("lowercase.txt") << false;
    QTest::newRow("case_upper_txt") << QStringLiteral("06_case_sensitivity/LOWERCASE.TXT") << QStringLiteral("LOWERCASE.TXT") << false;
    QTest::newRow("case_camel_ts") << QStringLiteral("06_case_sensitivity/camelCase.ts") << QStringLiteral("camelCase.ts") << false;

    // Deep Nesting
    QTest::newRow("deep_nested_target") << QStringLiteral("07_deep_nesting_and_lengths/deep/l1/l2/l3/l4/l5/l6/l7/l8/l9/l10/deeply_nested_target.txt") << QStringLiteral("deeply_nested_target.txt") << false;

    // Symlinks
    QTest::newRow("valid_file_symlink") << QStringLiteral("09_symlinks/valid_file_link.txt") << QStringLiteral("valid_file_link.txt") << false;
    QTest::newRow("valid_dir_symlink") << QStringLiteral("09_symlinks/valid_dir_link") << QStringLiteral("valid_dir_link") << true;
    QTest::newRow("special_char_symlink") << QStringLiteral("09_symlinks/link with [special] & characters.txt") << QStringLiteral("link with [special] & characters.txt") << false;
}

void TestRunner::testEdgeCaseMatchConstruction() {
    QFETCH(QString, relPath);
    QFETCH(QString, expectedLeafText);
    QFETCH(bool, isDir);

    const QString root = edgeCasesRoot();
    if (root.isEmpty() || !QDir(root).exists()) {
        QSKIP("edge_cases directory not available");
    }

    const QString fullPath = root + u'/' + relPath;
    if (!QFileInfo::exists(fullPath)) {
        QSKIP(qPrintable(QStringLiteral("Fixture does not exist: %1").arg(fullPath)));
    }

    KSeekRunner runner(root);
    RemoteMatch m = runner.buildMatch(relPath, 0, 10);
    QVERIFY2(!m.id.isEmpty(), qPrintable(QStringLiteral("Failed to build match for: %1").arg(relPath)));
    QCOMPARE(m.id, fullPath);
    QCOMPARE(m.text, expectedLeafText);
    QCOMPARE(m.properties.value(QStringLiteral("subtext")).toString(), fullPath);

    // Verify valid URL roundtrip
    const QStringList urls = m.properties.value(QStringLiteral("urls")).toStringList();
    QCOMPARE(urls.size(), 1);
    const QUrl parsedUrl(urls.first());
    QVERIFY(parsedUrl.isValid());
    QCOMPARE(parsedUrl.scheme(), QStringLiteral("file"));
    QCOMPARE(parsedUrl.toLocalFile(), fullPath);

    // Verify icon
    QVERIFY(!m.icon.isEmpty());
    if (isDir) {
        QCOMPARE(m.icon, QStringLiteral("inode-directory"));
    }

    // Verify actions
    const QStringList actions = m.properties.value(QStringLiteral("actions")).toStringList();
    QCOMPARE(actions.size(), 4);
    QVERIFY(actions.contains(QStringLiteral("open_app")));
    QVERIFY(actions.contains(QStringLiteral("show_item")));
    QVERIFY(actions.contains(QStringLiteral("copy_path")));
    QVERIFY(actions.contains(QStringLiteral("open_terminal")));
}

void TestRunner::testEdgeCasePipelineDelimitation_data() {
    QTest::addColumn<QString>("query");
    QTest::addColumn<QString>("expectedContainedFile");

    // Whitespace
    QTest::newRow("spaces_inner_file") << QStringLiteral("inner file with space") << QStringLiteral("inner file with space.txt");
    QTest::newRow("leading_space") << QStringLiteral("leading_space") << QStringLiteral(" leading_space.txt");
    QTest::newRow("trailing_space") << QStringLiteral("trailing_space") << QStringLiteral("trailing_space.txt ");
    QTest::newRow("tab_separated") << QStringLiteral("tab separated") << QStringLiteral("tab\tseparated.tsv");

    // Brackets and Quotes
    QTest::newRow("bracket_year") << QStringLiteral("[2024]") << QStringLiteral("[2024] financial_report [Q1-Q4].pdf");
    QTest::newRow("parentheses_copy") << QStringLiteral("document (copy)") << QStringLiteral("document (copy) (1).docx");
    QTest::newRow("curly_config") << QStringLiteral("{config_template}") << QStringLiteral("{config_template}.json");
    QTest::newRow("apostrophe_john") << QStringLiteral("john's report") << QStringLiteral("john's report.odt");
    QTest::newRow("single_quoted") << QStringLiteral("'single_quoted'") << QStringLiteral("'single_quoted'.txt");
    QTest::newRow("double_quoted") << QStringLiteral("\"double_quoted\"") << QStringLiteral("\"double_quoted\".txt");
    QTest::newRow("backtick_cmd") << QStringLiteral("`backtick_command`") << QStringLiteral("`backtick_command`.sh");

    // Shell Operators
    QTest::newRow("ampersand") << QStringLiteral("file_with_&_ampersand") << QStringLiteral("file_with_&_ampersand.txt");
    QTest::newRow("semicolon") << QStringLiteral("semicolon;separated") << QStringLiteral("semicolon;separated.txt");
    QTest::newRow("dollar_var") << QStringLiteral("dollar_$HOME") << QStringLiteral("dollar_$HOME_var.txt");
    QTest::newRow("percent_enc") << QStringLiteral("percent%20") << QStringLiteral("percent%20encoded.html");
    QTest::newRow("hash_tag") << QStringLiteral("hash_#tag") << QStringLiteral("hash_#tag_file.md");

    // CLI Flags
    QTest::newRow("flag_txt") << QStringLiteral("-flag.txt") << QStringLiteral("-flag.txt");
    QTest::newRow("help_flag") << QStringLiteral("--help.txt") << QStringLiteral("--help.txt");

    // Unicode & i18n
    QTest::newRow("cafe_accent") << QStringLiteral("café") << QStringLiteral("café_menu.pdf");
    QTest::newRow("resume_accent") << QStringLiteral("résumé") << QStringLiteral("résumé_2025.docx");
    QTest::newRow("munchen_accent") << QStringLiteral("München") << QStringLiteral("München_über_alles.txt");
    QTest::newRow("cyrillic_doc") << QStringLiteral("документ") << QStringLiteral("документ_отчет.pdf");
    QTest::newRow("chinese_doc") << QStringLiteral("项目文档") << QStringLiteral("项目文档.md");
    QTest::newRow("japanese_doc") << QStringLiteral("日本語ドキュメント") << QStringLiteral("日本語ドキュメント.txt");
    QTest::newRow("korean_doc") << QStringLiteral("프로젝트_기획서") << QStringLiteral("프로젝트_기획서.docx");
    QTest::newRow("indic_doc") << QStringLiteral("दस्तावेज़") << QStringLiteral("दस्तावेज़.txt");
    QTest::newRow("greek_notes") << QStringLiteral("ελληνικά") << QStringLiteral("ελληνικά_σημειώσεις.txt");
    QTest::newRow("arabic_doc") << QStringLiteral("تقرير_مالي") << QStringLiteral("تقرير_مالي.pdf");
    QTest::newRow("hebrew_doc") << QStringLiteral("מסמך_בדיקה") << QStringLiteral("מסמך_בדיקה.txt");
    QTest::newRow("emoji_rocket") << QStringLiteral("🚀") << QStringLiteral("🚀_launch_manifest.yaml");
    QTest::newRow("emoji_hotfix") << QStringLiteral("🔥") << QStringLiteral("🔥_hotfix.patch");

    // Compounds and Deep
    QTest::newRow("compound_tar") << QStringLiteral("archive.tar.gz") << QStringLiteral("archive.tar.gz");
    QTest::newRow("compound_d_ts") << QStringLiteral("component.module.css.d.ts") << QStringLiteral("component.module.css.d.ts");
    QTest::newRow("deep_nested") << QStringLiteral("deeply_nested_target") << QStringLiteral("deeply_nested_target.txt");
}

void TestRunner::testEdgeCasePipelineDelimitation() {
    QFETCH(QString, query);
    QFETCH(QString, expectedContainedFile);

    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    const QString root = edgeCasesRoot();
    if (root.isEmpty() || !QDir(root).exists()) {
        QSKIP("edge_cases directory not available");
    }

    QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
    pipeline.startSearch(100, QStringList{root}, query, {}, {}, 20, 3000);
    QVERIFY(finishedSpy.wait(4000));
    QCOMPARE(finishedSpy.count(), 1);

    const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
    QVERIFY(!results.isEmpty());

    bool found = false;
    for (const QString &p : results) {
        if (p.contains(expectedContainedFile)) {
            found = true;
            break;
        }
    }
    QVERIFY2(found, qPrintable(QStringLiteral("Expected '%1' for query '%2', got: %3").arg(expectedContainedFile, query, results.join(u", "))));
}

void TestRunner::testEdgeCaseConfigCombinations() {
    ProcessPipeline pipeline;
    if (!pipeline.isAvailable()) {
        QSKIP("fd or fzf not installed");
    }

    const QString root = edgeCasesRoot();
    if (root.isEmpty() || !QDir(root).exists()) {
        QSKIP("edge_cases directory not available");
    }

    // 1. --hidden config integration
    {
        // Default search without --hidden should NOT find hidden files
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(201, QStringList{root}, QStringLiteral("hidden_file"), {}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool foundHidden = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView(".hidden_file"))) {
                foundHidden = true;
                break;
            }
        }
        QVERIFY(!foundHidden);
    }
    {
        // With extraFdArgs = {"--hidden"}, hidden files should be found
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(202, QStringList{root}, QStringLiteral("hidden_file"), {QStringLiteral("--hidden")}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool foundHidden = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView(".hidden_file"))) {
                foundHidden = true;
                break;
            }
        }
        QVERIFY(foundHidden);
    }

    // 2. --type d vs --type f config integration
    {
        // Folders only
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(203, QStringList{root}, QStringLiteral("chinese_中文"), {QStringLiteral("--type"), QStringLiteral("d")}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        QVERIFY(!results.isEmpty());
        for (const QString &p : results) {
            const QString fullPath = p.startsWith(u'/') ? p : (root + u'/' + p);
            QVERIFY(QFileInfo(fullPath).isDir());
        }
    }
    {
        // Files only
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(204, QStringList{root}, QStringLiteral("chinese_中文"), {QStringLiteral("--type"), QStringLiteral("f")}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        QVERIFY(!results.isEmpty());
        for (const QString &p : results) {
            const QString fullPath = p.startsWith(u'/') ? p : (root + u'/' + p);
            QVERIFY(QFileInfo(fullPath).isFile());
        }
    }

    // 3. --max-depth config integration
    {
        // Max depth 3 should NOT reach 10-level nested target
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(205, QStringList{root}, QStringLiteral("deeply_nested_target"), {QStringLiteral("--max-depth"), QStringLiteral("3")}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool foundTarget = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("deeply_nested_target.txt"))) {
                foundTarget = true;
                break;
            }
        }
        QVERIFY(!foundTarget);
    }
    {
        // Max depth 15 should reach 10-level nested target
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(206, QStringList{root}, QStringLiteral("deeply_nested_target"), {QStringLiteral("--max-depth"), QStringLiteral("15")}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool foundTarget = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("deeply_nested_target.txt"))) {
                foundTarget = true;
                break;
            }
        }
        QVERIFY(foundTarget);
    }

    // 4. --exclude config integration
    {
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(207, QStringList{root}, QStringLiteral("café"), {QStringLiteral("--exclude"), QStringLiteral("latin_accents")}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool foundCafe = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("café_menu.pdf"))) {
                foundCafe = true;
                break;
            }
        }
        QVERIFY(!foundCafe);
    }

    // 5. --exact in extraFzfArgs config integration
    {
        // Without --exact, fuzzy substring matches
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(208, QStringList{root}, QStringLiteral("compcss"), {}, {}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("component.module.css.d.ts"))) {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }
    {
        // With --exact, non-contiguous substring does NOT match
        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(209, QStringList{root}, QStringLiteral("compcss"), {}, {QStringLiteral("--exact")}, 20, 2000);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        bool found = false;
        for (const QString &p : results) {
            if (p.contains(QLatin1StringView("component.module.css.d.ts"))) {
                found = true;
                break;
            }
        }
        QVERIFY(!found);
    }

    // 6. Multi-Root configuration integration across edge case folders
    {
        const QString rootA = QFileInfo(root + QStringLiteral("/01_whitespace_and_spacing")).canonicalFilePath();
        const QString rootB = QFileInfo(root + QStringLiteral("/03_unicode_and_i18n")).canonicalFilePath();
        const QStringList multiRoots = {rootA, rootB};

        KSeekRunner runner(multiRoots);
        QCOMPARE(runner.searchRoots().size(), 2);

        QSignalSpy finishedSpy(&pipeline, &ProcessPipeline::searchFinished);
        pipeline.startSearch(210, multiRoots, QStringLiteral("inner file with space"), {}, {}, 20, 2500);
        QVERIFY(finishedSpy.wait(3000));
        const QStringList results = finishedSpy.takeFirst().at(1).toStringList();
        QVERIFY(!results.isEmpty());

        RemoteMatch match = runner.buildMatch(results.first(), 0, results.size());
        QVERIFY(!match.id.isEmpty());
        QVERIFY(match.id.startsWith(rootA));
        QCOMPARE(match.text, QStringLiteral("inner file with space.txt"));
    }
}

void TestRunner::testCoreParsingEdgeCases() {
    // 1. splitArgs edge cases
    {
        // Nested single quotes inside double quotes
        const QString nested1 = QStringLiteral("--title=\"User's Profile\" --type='doc'");
        QCOMPARE(splitArgs(nested1), QStringList({QStringLiteral("--title=User's Profile"), QStringLiteral("--type=doc")}));

        // Unbalanced double quote
        const QString unbal1 = QStringLiteral("--opt \"unbalanced string");
        QCOMPARE(splitArgs(unbal1), QStringList({QStringLiteral("--opt"), QStringLiteral("unbalanced string")}));

        // Trailing backslash
        const QString trailingBs = QStringLiteral("--path /foo/bar\\");
        QCOMPARE(splitArgs(trailingBs), QStringList({QStringLiteral("--path"), QStringLiteral("/foo/bar\\")}));

        // Mixed tabs and whitespace
        const QString tabbed = QStringLiteral(" \t  -a  \t  --b=\"val with spaces\" \n -c ");
        QCOMPARE(splitArgs(tabbed), QStringList({QStringLiteral("-a"), QStringLiteral("--b=val with spaces"), QStringLiteral("-c")}));

        // Shell chars inside quotes
        const QString shellChars = QStringLiteral("--pattern=\"[0-9]+ $HOME & ; | < >\"");
        QCOMPARE(splitArgs(shellChars), QStringList({QStringLiteral("--pattern=[0-9]+ $HOME & ; | < >")}));
    }

    // 2. parseQuery edge cases
    {
        KSeekRunner runner(QStringLiteral("/tmp"));
        QString term;

        // Multiple colons with prefix 'f'
        runner.setPrefix(QStringLiteral("f"));
        QVERIFY(runner.parseQuery(QStringLiteral("f::test"), term));
        QCOMPARE(term, QStringLiteral(":test"));

        // Colon followed by space
        QVERIFY(runner.parseQuery(QStringLiteral("f:  spaced_after_colon"), term));
        QCOMPARE(term, QStringLiteral("spaced_after_colon"));

        // Space before colon
        QVERIFY(runner.parseQuery(QStringLiteral("f : test"), term));
        QCOMPARE(term, QStringLiteral(": test"));

        // Emoji query
        QVERIFY(runner.parseQuery(QStringLiteral("f 🚀_launch"), term));
        QCOMPARE(term, QStringLiteral("🚀_launch"));

        // Disabled prefix mode ("none")
        runner.setPrefix(QStringLiteral("none"));
        QVERIFY(runner.parseQuery(QStringLiteral("plain query"), term));
        QCOMPARE(term, QStringLiteral("plain query"));
        QVERIFY(runner.parseQuery(QStringLiteral("f 123"), term));
        QCOMPARE(term, QStringLiteral("f 123"));

        // Disabled prefix mode ("-")
        runner.setPrefix(QStringLiteral("-"));
        QVERIFY(runner.parseQuery(QStringLiteral("some_file.pdf"), term));
        QCOMPARE(term, QStringLiteral("some_file.pdf"));

        // Symbol prefix
        runner.setPrefix(QStringLiteral("?"));
        QVERIFY(runner.parseQuery(QStringLiteral("?  find_me"), term));
        QCOMPARE(term, QStringLiteral("find_me"));
    }

    // 3. parseSearchRoots edge cases
    {
        const QString home = QFileInfo(QDir::homePath()).canonicalFilePath();

        // Multiple consecutive colons
        const QString consecutiveColons = QStringLiteral(":::") + home + QStringLiteral("::::") + home + QStringLiteral(":::");
        const QStringList parsedConsecutive = parseSearchRoots(consecutiveColons);
        QCOMPARE(parsedConsecutive.size(), 1);
        QCOMPARE(parsedConsecutive.first(), home);

        // Tilde path
        const QStringList parsedTilde = parseSearchRoots(QStringLiteral("~/"));
        QCOMPARE(parsedTilde.size(), 1);
        QCOMPARE(parsedTilde.first(), home);

        // Deduplication with and without trailing slash
        const QStringList parsedDups = parseSearchRoots(QStringList{home, home + QStringLiteral("/"), home + QStringLiteral("//")});
        QCOMPARE(parsedDups.size(), 1);
        QCOMPARE(parsedDups.first(), home);

        // Non-existent path combined with valid path
        const QString nonExistent = QStringLiteral("/this/path/does/not/exist/99999");
        const QStringList parsedMixed = parseSearchRoots(QStringList{nonExistent, home});
        QCOMPARE(parsedMixed.size(), 1);
        QCOMPARE(parsedMixed.first(), home);
    }
}

QTEST_GUILESS_MAIN(TestRunner)
#include "test_runner.moc"


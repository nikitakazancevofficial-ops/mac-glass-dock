#include "controlcenterstate.h"
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class ControlCenterStateTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QByteArray originalPath;
    QByteArray originalState;

    QString stateFile() const { return directory.filePath("volume"); }
    QByteArray storedVolume() const {
        QFile file(stateFile());
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll().trimmed();
    }
    void storeVolume(int volume) {
        QFile file(stateFile());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QByteArray::number(volume));
    }
private slots:
    void initTestCase() {
        QVERIFY(directory.isValid());
        originalPath = qgetenv("PATH");
        originalState = qgetenv("MACGLASS_TEST_AUDIO_STATE");
        QFile wpctl(directory.filePath("wpctl"));
        QVERIFY(wpctl.open(QIODevice::WriteOnly));
        // Exercise the actual QProcess boundary. Reject malformed arguments just
        // as wpctl does, and expose accepted writes through subsequent reads.
        wpctl.write(R"SH(#!/bin/bash
[[ $# == 2 || $# == 3 ]] || exit 1
[[ $2 == @DEFAULT_AUDIO_SINK@ ]] || exit 1
case $1 in
    get-volume)
        level=$(<"$MACGLASS_TEST_AUDIO_STATE")
        printf 'Volume: %d.%02d\n' "$((level / 100))" "$((level % 100))"
        ;;
    set-volume)
        [[ $# == 3 && $3 =~ ^[0-9]+%$ ]] || exit 1
        level=${3%%%}
        ((level >= 0 && level <= 100)) || exit 1
        printf '%s\n' "$level" > "$MACGLASS_TEST_AUDIO_STATE"
        ;;
    *) exit 1 ;;
esac
)SH");
        wpctl.close();
        QVERIFY(wpctl.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        // Keep optional desktop services out of this audio regression test.
        for (const auto &name : {"kscreen-doctor", "nmcli", "bluetoothctl", "upower", "busctl"}) {
            QFile stub(directory.filePath(name));
            QVERIFY(stub.open(QIODevice::WriteOnly));
            stub.write("#!/bin/bash\nexit 0\n");
            stub.close();
            QVERIFY(stub.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        }
        qputenv("PATH", directory.path().toUtf8() + ':' + originalPath);
        qputenv("MACGLASS_TEST_AUDIO_STATE", stateFile().toUtf8());
    }
    void cleanupTestCase() {
        qputenv("PATH", originalPath);
        if (originalState.isNull()) qunsetenv("MACGLASS_TEST_AUDIO_STATE");
        else qputenv("MACGLASS_TEST_AUDIO_STATE", originalState);
    }
    void writesVolumeAndResumesPolling_data() {
        QTest::addColumn<int>("requested");
        QTest::addColumn<int>("expected");
        QTest::newRow("silent") << 0 << 0;
        QTest::newRow("intermediate") << 37 << 37;
        QTest::newRow("maximum") << 100 << 100;
        QTest::newRow("clamp-low") << -10 << 0;
        QTest::newRow("clamp-high") << 120 << 100;
    }
    void writesVolumeAndResumesPolling() {
        QFETCH(int, requested);
        QFETCH(int, expected);
        storeVolume(45);
        ControlCenterState state;
        QTRY_COMPARE(state.volume(), 45);
        state.setVolume(requested);
        QCOMPARE(state.volume(), expected);
        QTRY_COMPARE_WITH_TIMEOUT(storedVolume(), QByteArray::number(expected), 1500);
        // An external mixer change must be visible after the write completes.
        storeVolume(23);
        QTRY_COMPARE_WITH_TIMEOUT(state.volume(), 23, 1500);
    }
    void rapidChangesApplyLatestVolume() {
        storeVolume(45);
        ControlCenterState state;
        QTRY_COMPARE(state.volume(), 45);
        state.setVolume(10);
        state.setVolume(60);
        state.setVolume(29);
        QTRY_COMPARE_WITH_TIMEOUT(storedVolume(), QByteArray("29"), 1500);
        storeVolume(23);
        QTRY_COMPARE_WITH_TIMEOUT(state.volume(), 23, 1500);
    }
};

QTEST_GUILESS_MAIN(ControlCenterStateTests)
#include "controlcenterstatetests.moc"

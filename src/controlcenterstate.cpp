#include "controlcenterstate.h"
#include <QProcess>
#include <QDateTime>
#include <functional>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <memory>

ControlCenterState::ControlCenterState(QObject *parent) : QObject(parent) {
    m_timer.setInterval(15000);
    connect(&m_timer, &QTimer::timeout, this, &ControlCenterState::refresh);
    QDBusConnection::systemBus().connect(
        QStringLiteral("org.freedesktop.UPower"), QStringLiteral("/org/freedesktop/UPower"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"),
        this, SLOT(onUpowerPropertiesChanged(QString,QVariantMap,QStringList)));
    m_sliderTimer.setInterval(200);
    connect(&m_sliderTimer, &QTimer::timeout, this, &ControlCenterState::refreshSliderState);
    m_radioTimer.setInterval(1000);
    connect(&m_radioTimer, &QTimer::timeout, this, &ControlCenterState::refreshRadioState);
    m_writeTimer.setSingleShot(true);
    m_writeTimer.setInterval(70);
    connect(&m_writeTimer, &QTimer::timeout, this, &ControlCenterState::applyPendingWrites);
    m_timer.start();
    m_sliderTimer.start();
    m_radioTimer.start();
    refresh();
    refreshSliderState();
    refreshRadioState();
}

void ControlCenterState::onUpowerPropertiesChanged(const QString &interfaceName,
                                                    const QVariantMap &changedProperties,
                                                    const QStringList &invalidatedProperties) {
    if (interfaceName == QStringLiteral("org.freedesktop.UPower")
        && (changedProperties.contains(QStringLiteral("OnBattery"))
            || invalidatedProperties.contains(QStringLiteral("OnBattery")))) {
        refresh();
    }
}

void ControlCenterState::run(const QString &command, std::function<void(const QString &)> done) {
    runProgram(QStringLiteral("/bin/bash"), {QStringLiteral("-c"), command},
               [done = std::move(done)](const QString &output, int) { done(output); });
}

void ControlCenterState::runProgram(const QString &program, const QStringList &arguments,
                                    std::function<void(const QString &, int)> done) {
    auto *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [process, done = std::move(done)](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(process->readAllStandardOutput()).trimmed();
        done(output, status == QProcess::NormalExit ? exitCode : -1);
        process->deleteLater();
    });
    process->start(program, arguments);
}

void ControlCenterState::refresh() {
    run(QStringLiteral("bat=$(upower -i \"$(upower -e 2>/dev/null | grep BAT | head -1)\" 2>/dev/null | awk '/percentage/ {gsub(\"%\",\"\",$2); print $2; exit}'); [ -n \"$bat\" ] || bat=0; power=0; onbattery=$(busctl --system get-property org.freedesktop.UPower /org/freedesktop/UPower org.freedesktop.UPower OnBattery 2>/dev/null | awk '{print $2}'); if [ \"$onbattery\" = false ]; then power=1; elif [ \"$onbattery\" != true ]; then for supply in /sys/class/power_supply/*; do if [ -r \"$supply/online\" ] && [ \"$(cat \"$supply/online\")\" = 1 ]; then power=1; break; fi; done; fi; ch=$(upower -i \"$(upower -e 2>/dev/null | grep BAT | head -1)\" 2>/dev/null | awk '/state/ {print $2; exit}'); wifi=$(nmcli -t -f active,ssid dev wifi 2>/dev/null | awk -F: '$1==\"yes\" {print $2; exit}'); [ -n \"$wifi\" ] || wifi=\"Нет сети\"; printf \"%s|%s|%s|%s|%s|%s\" \"$bat\" \"$ch\" \"$power\" \"$wifi\" \"$(LC_TIME=C date '+%-I:%M %p')\" \"$(LC_TIME=C date '+%a %b %-d')\""), [this](const QString &value) {
        const auto parts = value.split('|');
        if (parts.size() < 6) return;
        m_battery = qBound(0, parts[0].toInt(), 100);
        m_charging = parts[1] == QStringLiteral("charging") || parts[2] == QStringLiteral("1");
        m_wifiName = parts[3].isEmpty() ? QStringLiteral("Нет сети") : parts[3];
        m_timeText = parts[4];
        m_dateText = parts[5];
        emit changed();
    });
}

void ControlCenterState::refreshSliderState() {
    if (m_sliderRefreshInFlight) return;
    m_sliderRefreshInFlight = true;
    runProgram(QStringLiteral("/bin/bash"), {
        QStringLiteral("-c"),
        QStringLiteral("v=$(wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null | awk '{printf \"%d\", $2*100}'); "
                       "state=$(kscreen-doctor -o 2>/dev/null); "
                       "o=$(printf '%s\\n' \"$state\" | awk '$1==\"Output:\" {print $2; exit}'); "
                       "b=$(printf '%s\\n' \"$state\" | sed -n 's/.*Brightness control:.*set to \\([0-9]*\\)%.*/\\1/p' | head -1); "
                       "kd=$(find /sys/class/leds -maxdepth 1 -type l -name '*kbd_backlight' -print -quit); "
                       "if [ -n \"$kd\" ] && [ -r \"$kd/brightness\" ] && [ -r \"$kd/max_brightness\" ]; then k=$((100*$(cat \"$kd/brightness\")/$(cat \"$kd/max_brightness\"))); else k=0; fi; "
                       "printf '%s|%s|%s|%s' \"${v:-0}\" \"${b:-0}\" \"$k\" \"${o:-1}\"")
    }, [this](const QString &value, int exitCode) {
        m_sliderRefreshInFlight = false;
        const auto parts = value.split('|');
        if (exitCode != 0 || parts.size() < 4) return;
        if (parts[0].toInt() >= 0 && m_pendingVolume < 0)
            m_volume = qBound(0, parts[0].toInt(), 100);
        if (parts[1].toInt() >= 0 && m_pendingBrightness < 0)
            m_brightness = qBound(0, parts[1].toInt(), 100);
        if (parts[2].toInt() >= 0 && m_pendingKeyboardBrightness < 0)
            m_keyboardBrightness = qBound(0, parts[2].toInt(), 100);
        if (!parts[3].isEmpty()) m_outputId = parts[3];
        emit changed();
    });
}

void ControlCenterState::refreshRadioState() {
    if (m_radioRefreshInFlight) return;
    m_radioRefreshInFlight = true;
    auto remaining = std::make_shared<int>(2);
    const auto done = [this, remaining] {
        if (--(*remaining) == 0) {
            m_radioRefreshInFlight = false;
            emit changed();
        }
    };
    runProgram(QStringLiteral("nmcli"), {QStringLiteral("radio"), QStringLiteral("wifi")},
               [this, done](const QString &value, int exitCode) {
        if (exitCode == 0) m_wifiEnabled = value.trimmed() == QStringLiteral("enabled");
        done();
    });
    runProgram(QStringLiteral("bluetoothctl"), {QStringLiteral("show")},
               [this, done](const QString &value, int exitCode) {
        if (exitCode == 0) {
            const auto lines = value.split('\n');
            for (const auto &line : lines) {
                const QString trimmed = line.trimmed();
                if (trimmed.startsWith(QStringLiteral("Powered:"))) {
                    m_bluetoothEnabled = trimmed.endsWith(QStringLiteral("yes"));
                    break;
                }
            }
        }
        done();
    });
}

void ControlCenterState::setVolume(int value) {
    m_pendingVolume = qBound(0, value, 100);
    m_volume = m_pendingVolume;
    emit changed();
    if (!m_writeTimer.isActive()) m_writeTimer.start();
}

void ControlCenterState::setBrightness(int value) {
    m_pendingBrightness = qBound(0, value, 100);
    m_brightness = m_pendingBrightness;
    emit changed();
    if (!m_writeTimer.isActive()) m_writeTimer.start();
}

void ControlCenterState::setKeyboardBrightness(int value) {
    m_pendingKeyboardBrightness = qBound(0, value, 100);
    m_keyboardBrightness = m_pendingKeyboardBrightness;
    emit changed();
    if (!m_writeTimer.isActive()) m_writeTimer.start();
}

void ControlCenterState::setWifiEnabled(bool enabled) {
    if (m_wifiEnabled == enabled) return;
    m_wifiEnabled = enabled;
    emit changed();
    runProgram(QStringLiteral("nmcli"), {QStringLiteral("radio"), QStringLiteral("wifi"), enabled ? QStringLiteral("on") : QStringLiteral("off")},
               [this, enabled](const QString &, int exitCode) {
        if (exitCode != 0) refreshRadioState();
        else m_wifiEnabled = enabled;
        emit changed();
    });
}

void ControlCenterState::setBluetoothEnabled(bool enabled) {
    if (m_bluetoothEnabled == enabled) return;
    m_bluetoothEnabled = enabled;
    emit changed();
    runProgram(QStringLiteral("bluetoothctl"), {QStringLiteral("power"), enabled ? QStringLiteral("on") : QStringLiteral("off")},
               [this, enabled](const QString &, int exitCode) {
        if (exitCode != 0) refreshRadioState();
        else m_bluetoothEnabled = enabled;
        emit changed();
    });
}

void ControlCenterState::applyPendingWrites() {
    const int volume = m_pendingVolume;
    const int brightness = m_pendingBrightness;
    const int keyboardBrightness = m_pendingKeyboardBrightness;

    if (volume >= 0) {
        runProgram(QStringLiteral("wpctl"), {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), QStringLiteral("%1%").arg(volume)},
                   [this, volume](const QString &, int exitCode) {
            if (exitCode == 0 && m_pendingVolume == volume) m_pendingVolume = -1;
            refreshSliderState();
        });
    }
    if (brightness >= 0) {
        runProgram(QStringLiteral("kscreen-doctor"), {QStringLiteral("output.%1.brightness.%2").arg(m_outputId).arg(brightness)},
                   [this, brightness](const QString &, int exitCode) {
            if (exitCode == 0 && m_pendingBrightness == brightness) m_pendingBrightness = -1;
            refreshSliderState();
        });
    }
    if (keyboardBrightness >= 0) {
        QDBusInterface keyboard(QStringLiteral("org.kde.Solid.PowerManagement"),
                                QStringLiteral("/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"),
                                QStringLiteral("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"),
                                QDBusConnection::sessionBus());
        const QDBusReply<int> maximum = keyboard.call(QStringLiteral("keyboardBrightnessMax"));
        if (maximum.isValid() && maximum.value() > 0) {
            const int level = qRound(keyboardBrightness * maximum.value() / 100.0);
            const QDBusMessage call = QDBusMessage::createMethodCall(
                QStringLiteral("org.kde.Solid.PowerManagement"),
                QStringLiteral("/org/kde/Solid/PowerManagement/Actions/KeyboardBrightnessControl"),
                QStringLiteral("org.kde.Solid.PowerManagement.Actions.KeyboardBrightnessControl"),
                QStringLiteral("setKeyboardBrightnessSilent"));
            auto request = call;
            request << level;
            QDBusConnection::sessionBus().asyncCall(request);
            if (m_pendingKeyboardBrightness == keyboardBrightness) m_pendingKeyboardBrightness = -1;
        }
        refreshSliderState();
    }
}

void ControlCenterState::setFocusMode(bool enabled) {
    const auto bus = QDBusConnection::sessionBus();
    if (enabled && !m_focusActive) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("Inhibit"));
        QVariantMap hints;
        hints.insert(QStringLiteral("suppress-sound"), true);
        hints.insert(QStringLiteral("show-banners"), false);
        hints.insert(QStringLiteral("show-bubble"), false);
        call << QStringLiteral("mac-glass-dock") << QStringLiteral("Focus mode") << hints;
        const QDBusReply<unsigned int> reply = bus.call(call);
        if (reply.isValid()) {
            m_focusCookie = reply.value();
            m_focusActive = true;
        }
    } else if (!enabled && m_focusActive) {
        QDBusMessage call = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("UnInhibit"));
        call << m_focusCookie;
        bus.call(call);
        m_focusCookie = 0;
        m_focusActive = false;
    }
    emit changed();
}

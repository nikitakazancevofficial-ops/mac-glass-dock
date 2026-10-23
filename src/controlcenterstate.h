#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class ControlCenterState : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by the application")
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(int brightness READ brightness NOTIFY changed)
    Q_PROPERTY(int keyboardBrightness READ keyboardBrightness NOTIFY changed)
    Q_PROPERTY(int battery READ battery NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
    Q_PROPERTY(bool focusActive READ focusActive NOTIFY changed)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY changed)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY changed)
    Q_PROPERTY(QString wifiName READ wifiName NOTIFY changed)
    Q_PROPERTY(QString timeText READ timeText NOTIFY changed)
    Q_PROPERTY(QString dateText READ dateText NOTIFY changed)
public:
    explicit ControlCenterState(QObject *parent = nullptr);
    int volume() const { return m_volume; }
    int brightness() const { return m_brightness; }
    int keyboardBrightness() const { return m_keyboardBrightness; }
    int battery() const { return m_battery; }
    bool charging() const { return m_charging; }
    bool focusActive() const { return m_focusActive; }
    bool wifiEnabled() const { return m_wifiEnabled; }
    bool bluetoothEnabled() const { return m_bluetoothEnabled; }
    QString wifiName() const { return m_wifiName; }
    QString timeText() const { return m_timeText; }
    QString dateText() const { return m_dateText; }
    Q_INVOKABLE void setVolume(int value);
    Q_INVOKABLE void setBrightness(int value);
    Q_INVOKABLE void setKeyboardBrightness(int value);
    Q_INVOKABLE void setFocusMode(bool enabled);
    Q_INVOKABLE void setWifiEnabled(bool enabled);
    Q_INVOKABLE void setBluetoothEnabled(bool enabled);
    Q_INVOKABLE void refresh();
signals:
    void changed();
private:
    Q_SLOT void onUpowerPropertiesChanged(const QString &interfaceName,
                                          const QVariantMap &changedProperties,
                                          const QStringList &invalidatedProperties);
    void run(const QString &command, std::function<void(const QString &)> done);
    void runProgram(const QString &program, const QStringList &arguments,
                    std::function<void(const QString &, int)> done);
    void refreshSliderState();
    void refreshRadioState();
    void applyPendingWrites();
    int m_volume = 0, m_brightness = 0, m_keyboardBrightness = 0, m_battery = 0;
    bool m_charging = false;
    bool m_focusActive = false;
    bool m_wifiEnabled = true;
    bool m_bluetoothEnabled = false;
    unsigned int m_focusCookie = 0;
    QString m_wifiName = QStringLiteral("Нет сети"), m_timeText, m_dateText;
    QTimer m_timer;
    QTimer m_sliderTimer;
    QTimer m_radioTimer;
    QTimer m_writeTimer;
    QString m_outputId = QStringLiteral("1");
    bool m_sliderRefreshInFlight = false;
    bool m_radioRefreshInFlight = false;
    int m_pendingVolume = -1;
    int m_pendingBrightness = -1;
    int m_pendingKeyboardBrightness = -1;
};

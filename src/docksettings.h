#pragma once
#include <QObject>
#include <QStringList>
#include <QColor>
#include <QVariant>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class DockSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(qreal iconSize MEMBER iconSize NOTIFY changed)
    Q_PROPERTY(qreal maxScale MEMBER maxScale NOTIFY changed)
    Q_PROPERTY(qreal spacing MEMBER spacing NOTIFY changed)
    Q_PROPERTY(qreal animationSpeed MEMBER animationSpeed NOTIFY changed)
    Q_PROPERTY(qreal surfaceOpacity MEMBER surfaceOpacity NOTIFY changed)
    Q_PROPERTY(qreal highlightIntensity MEMBER highlightIntensity NOTIFY changed)
    Q_PROPERTY(qreal thickness MEMBER thickness NOTIFY changed)
    Q_PROPERTY(qreal edgeReflection MEMBER edgeReflection NOTIFY changed)
    Q_PROPERTY(qreal springDamping MEMBER springDamping NOTIFY changed)
    Q_PROPERTY(qreal rimWidth MEMBER rimWidth NOTIFY changed)
    Q_PROPERTY(qreal bevelCurve MEMBER bevelCurve NOTIFY changed)
    Q_PROPERTY(qreal fresnelPower MEMBER fresnelPower NOTIFY changed)
    Q_PROPERTY(qreal edgeDispersion MEMBER edgeDispersion NOTIFY changed)
    Q_PROPERTY(qreal innerShadow MEMBER innerShadow NOTIFY changed)
    Q_PROPERTY(qreal shadowOpacity MEMBER shadowOpacity NOTIFY changed)
    Q_PROPERTY(qreal shadowRadius MEMBER shadowRadius NOTIFY changed)
    Q_PROPERTY(qreal highlightSize MEMBER highlightSize NOTIFY changed)
    Q_PROPERTY(qreal iconGlassOpacity MEMBER iconGlassOpacity NOTIFY changed)
    Q_PROPERTY(qreal iconThickness MEMBER iconThickness NOTIFY changed)
    Q_PROPERTY(qreal iconRadius MEMBER iconRadius NOTIFY changed)
    Q_PROPERTY(qreal iconPadding MEMBER iconPadding NOTIFY changed)
    Q_PROPERTY(qreal iconReflection MEMBER iconReflection NOTIFY changed)
    Q_PROPERTY(qreal blurRadius MEMBER blurRadius NOTIFY changed)
    Q_PROPERTY(qreal refractionStrength MEMBER refractionStrength NOTIFY changed)
    Q_PROPERTY(qreal refractionWidth MEMBER refractionWidth NOTIFY changed)
    Q_PROPERTY(qreal refractionCurve MEMBER refractionCurve NOTIFY changed)
    Q_PROPERTY(qreal chromaticAberration MEMBER chromaticAberration NOTIFY changed)
    Q_PROPERTY(qreal centerZoom MEMBER centerZoom NOTIFY changed)
    Q_PROPERTY(qreal iconBlurRadius MEMBER iconBlurRadius NOTIFY changed)
    Q_PROPERTY(qreal iconRefractionStrength MEMBER iconRefractionStrength NOTIFY changed)
    Q_PROPERTY(qreal iconRefractionWidth MEMBER iconRefractionWidth NOTIFY changed)
    Q_PROPERTY(bool iconBlurEnabled MEMBER iconBlurEnabled NOTIFY changed)
    Q_PROPERTY(bool gpuEffects MEMBER gpuEffects NOTIFY changed)
    Q_PROPERTY(bool blurEnabled MEMBER blurEnabled NOTIFY changed)
    Q_PROPERTY(bool autoHide MEMBER autoHide NOTIFY changed)
    Q_PROPERTY(bool reducedMotion MEMBER reducedMotion NOTIFY changed)
    Q_PROPERTY(bool bounceEnabled MEMBER bounceEnabled NOTIFY changed)
    Q_PROPERTY(bool windowIntegration MEMBER windowIntegration NOTIFY changed)
    Q_PROPERTY(bool windowAnimations MEMBER windowAnimations NOTIFY changed)
    Q_PROPERTY(bool smartAutoHide MEMBER smartAutoHide NOTIFY changed)
    Q_PROPERTY(QString iconStyle MEMBER iconStyle NOTIFY changed)
    Q_PROPERTY(QString screenName MEMBER screenName NOTIFY changed)
    Q_PROPERTY(QString theme MEMBER theme NOTIFY changed)
    Q_PROPERTY(QColor tint MEMBER tint NOTIFY changed)
    Q_PROPERTY(QString saveError READ saveError NOTIFY changed)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemThemeChanged)
public:
    explicit DockSettings(QObject *parent = nullptr);
    ~DockSettings() override;
    bool systemDark() const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);
    Q_INVOKABLE void screenshotDefaults();
    Q_INVOKABLE void preset(const QString &name);
    Q_INVOKABLE void flush();
    void setPinnedApps(const QStringList &ids);
    QString saveError() const { return m_saveError; }
    qreal iconSize = 56, maxScale = 1.5, spacing = 10, animationSpeed = 2.55;
    qreal surfaceOpacity = 0.04, highlightIntensity = 0.84;
    qreal thickness = 24, edgeReflection = 1, springDamping = 0.46;
    qreal rimWidth = 1.2;
    qreal bevelCurve = 1.35;
    qreal fresnelPower = 2.1;
    qreal edgeDispersion = 0.025;
    qreal innerShadow = 0.06;
    qreal shadowOpacity = 0.16;
    qreal shadowRadius = 8;
    qreal highlightSize = 170;
    qreal iconGlassOpacity = 0.08;
    qreal iconThickness = 12;
    qreal iconRadius = 0.25;
    qreal iconPadding = 0.105;
    qreal iconReflection = 1;
    qreal blurRadius = 3;
    qreal refractionStrength = 20;
    qreal refractionWidth = 30;
    qreal refractionCurve = 1.6;
    qreal chromaticAberration = 0.8;
    qreal centerZoom = 1;
    qreal iconBlurRadius = 4;
    qreal iconRefractionStrength = 17;
    qreal iconRefractionWidth = 19;
    bool iconBlurEnabled = true;
    bool gpuEffects = true, blurEnabled = true, autoHide = true, reducedMotion = false;
    bool bounceEnabled = true, windowIntegration = false;
    bool windowAnimations = true;
    bool smartAutoHide = true;
    QString theme = QStringLiteral("auto"), iconStyle = QStringLiteral("glass"), screenName;
    QColor tint = QColor(QStringLiteral("#a7cbe5"));
    QStringList pinnedApps;
    QString fileName;
signals:
    void changed();
    void pinsChanged();
    void systemThemeChanged();
private:
    void read();
    void save();
    void scheduleSave();
    QTimer m_saveTimer;
    QString m_saveError;
};

// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#include <QQmlExtensionPlugin>
#include <QQmlEngine>
#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>
#include <effect/effecthandler.h>

class Bootstrap : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool load(const QUrl &url) {
        if(!url.isLocalFile() || !KWin::effects) return false;
        const QString root=url.toLocalFile();
        if(!QFileInfo::exists(root+"/kwin/effects/plugins/macglassrefraction.so")) return false;
        // A session-only public Qt/KWin loader. Restore all search paths immediately.
        const QStringList before=QCoreApplication::libraryPaths();
        QCoreApplication::addLibraryPath(root);
        const bool ok=KWin::effects->loadEffect("macglassrefraction");
        QCoreApplication::setLibraryPaths(before);
        return ok;
    }
};
class BootstrapPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override { qmlRegisterType<Bootstrap>(uri,1,0,"Bootstrap"); }
};
#include "bootstrap.moc"

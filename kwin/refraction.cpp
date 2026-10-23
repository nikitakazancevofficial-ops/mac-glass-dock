// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#include "refraction.h"
#include "geniegeometry.h"
#include <effect/effecthandler.h>
#include <effect/effectwindow.h>
#include <core/rendertarget.h>
#include <core/renderviewport.h>
#include <wayland/surface.h>
#include <wayland/layershell_v1.h>
#include <scene/backgroundeffectitem.h>
#include <scene/windowitem.h>
#include <cursor.h>
#include <input.h>
#include <opengl/glframebuffer.h>
#include <opengl/gltexture.h>
#include <opengl/glshader.h>
#include <opengl/glshadermanager.h>
#include <opengl/glvertexbuffer.h>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QImage>
#include <QTimer>
#include <cmath>
#ifdef MACGLASS_NESTED_TEST_CAPTURE
#include <QCryptographicHash>
#include <core/outputconfiguration.h>
#include <core/outputbackend.h>
#include <main.h>
#include <workspace.h>
#endif

namespace KWin {
namespace {
constexpr int padding=64;
double option(const QJsonObject &o,const char *key,double fallback,double lo,double hi) {
    const double n=o.value(key).toDouble(fallback);
    return std::isfinite(n) ? qBound(lo,n,hi) : fallback;
}
QRectF rect(const QJsonObject &o) {
    return {o["x"].toDouble(),o["y"].toDouble(),o["w"].toDouble(),o["h"].toDouble()};
}
}
MacGlassRefraction::MacGlassRefraction() {
    input()->installInputEventSpy(this);
    setVertexSnappingMode(RenderGeometry::VertexSnappingMode::None);
    m_anchorClock.start();
    QFile vert(MACGLASS_SHADER_PREFIX "lens.vert"),frag(MACGLASS_SHADER_PREFIX "lens.frag");
    if(vert.open(QIODevice::ReadOnly) && frag.open(QIODevice::ReadOnly))
        m_shader=ShaderManager::instance()->loadShaderFromCode(vert.readAll(),frag.readAll());
    m_ready=bool(m_shader);
    m_motionShader=ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,QString(),QStringLiteral(MACGLASS_SHADER_PREFIX "motion.frag"));
    auto bus=QDBusConnection::sessionBus();
    bus.registerObject("/MacGlassRefraction",this,QDBusConnection::ExportScriptableSlots|QDBusConnection::ExportScriptableSignals);
    bus.connect("org.macglass.Dock","/Dock","org.macglass.Dock","opticalStateChanged",this,SLOT(updateState(QString)));
    m_watcher.setConnection(bus); m_watcher.setWatchedServices({"org.macglass.Dock"});
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForRegistration|QDBusServiceWatcher::WatchForUnregistration);
    connect(&m_watcher,&QDBusServiceWatcher::serviceRegistered,this,[this]{requestState();});
    connect(&m_watcher,&QDBusServiceWatcher::serviceUnregistered,this,[this]{
        m_pid=0; m_windows={};m_dock={};m_anchors.clear();m_hiddenPanels.clear();clearMotions();syncPanelMotions();syncBackgrounds();handlePointer({});effects->addRepaintFull();
    });
    connect(effects,&EffectsHandler::windowDeleted,this,[this](EffectWindow *w){
        m_panelMotions.remove(w);
        m_hiddenPanels.remove(w);
        m_settledHidden.remove(w);
        effects->makeOpenGLContextCurrent(); m_buffers.erase(w);
    });
    connect(effects,&EffectsHandler::windowAdded,this,[this]{syncBackgrounds();syncPanelMotions();});
    connect(effects,&EffectsHandler::windowAdded,this,[this](EffectWindow *w){observeWindow(w);beginMotion(w,Transition::Open);});
    connect(effects,&EffectsHandler::windowClosed,this,[this](EffectWindow *w){
        beginMotion(w,Transition::Close);
        if(!m_panelMotions.contains(w) && !m_hiddenPanels.contains(w))return;
        m_hiddenPanels.insert(w);
#ifdef MACGLASS_NESTED_TEST_CAPTURE
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_PANEL_TAIL_HOLD")) {
            struct Hold {
                EffectWindowDeletedRef deleted;
                EffectWindowVisibleRef visible;
                explicit Hold(EffectWindow *window):deleted(window),visible(window,EffectWindow::PAINT_DISABLED){}
            };
            const auto hold=std::make_shared<Hold>(w);
            QTimer::singleShot(600,this,[hold]{});
        }
#endif
    });
    for(auto *w:effects->stackingOrder()) observeWindow(w);
    connect(effects,&EffectsHandler::desktopChanged,this,&MacGlassRefraction::syncOcclusion);
    connect(effects,&EffectsHandler::stackingOrderChanged,this,&MacGlassRefraction::syncOcclusion);
    connect(effects,&EffectsHandler::windowActivated,this,&MacGlassRefraction::syncOcclusion);
    connect(effects,&EffectsHandler::showingDesktopChanged,this,[this](bool showing){m_showingDesktop=showing;syncOcclusion();});
    connect(effects,&EffectsHandler::currentActivityChanged,this,&MacGlassRefraction::syncOcclusion);
    if(auto *cursors=Cursors::self();cursors && cursors->mouse())
        connect(cursors->mouse(),&Cursor::posChanged,this,[this](const QPointF &p){handlePointer(p);});
    connect(effects,&EffectsHandler::windowDataChanged,this,[this](EffectWindow *w,int role){
        const auto it=m_motions.find(w);
        if(it!=m_motions.end() && it->second->role==role && w->data(role).value<void *>()!=this) finishMotion(w);
    });
    connect(effects,&EffectsHandler::viewRemoved,this,[this](RenderView *view){
        effects->makeOpenGLContextCurrent();
        for(auto &[window,resources]:m_buffers) resources.views.erase(view);
    });
    requestState();
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    // Only the isolated QA binary may configure its synthetic WL-* output.
    // Production has neither this code nor an output-configuration endpoint.
    if(qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE") && qEnvironmentVariableIsSet("MACGLASS_TEST_OUTPUT_SCALE")) {
        const double scale=qEnvironmentVariable("MACGLASS_TEST_OUTPUT_SCALE").toDouble();
        if(scale>=1 && scale<=2) QTimer::singleShot(100,this,[this,scale]{
            OutputConfiguration configuration;
            configuration.source=OutputConfiguration::Source::User;
            bool nested=true;
            for(auto *output:kwinApp()->outputBackend()->outputs()) {
                if(!output->name().startsWith("WL-")) {nested=false;break;}
                configuration.changeSet(output)->scale=scale;
                configuration.changeSet(output)->scaleSetting=scale;
            }
            if(nested) {
                // WaylandOutput deliberately ignores requested scale in favor
                // of its host's fractional-scale protocol. For this synthetic
                // QA output only, seed the base state before updating the layout.
                for(auto *output:kwinApp()->outputBackend()->outputs()) output->BackendOutput::applyChanges(configuration);
                m_qaScaleApplied=int(bool(workspace()->applyOutputConfiguration(configuration)));
            } else m_qaScaleApplied=-2;
        });
    }
#endif
}
MacGlassRefraction::~MacGlassRefraction() {
    QDBusConnection::sessionBus().unregisterObject("/MacGlassRefraction");
    effects->makeOpenGLContextCurrent(); m_windows={};syncPanelMotions();clearMotions(); m_buffers.clear(); m_shader.reset();
    effects->addRepaintFull();
}
bool MacGlassRefraction::supported() { return effects && effects->isOpenGLCompositing(); }
QString MacGlassRefraction::status() const {return m_ready ? "ready:v1" : "failed:v1";}
QString MacGlassRefraction::diagnostics() const {
    QJsonObject textures;
    auto coordinates=[](const QRectF &r){return QJsonObject{{"x",r.x()},{"y",r.y()},{"w",r.width()},{"h",r.height()}};};
    const auto *dock=dockWindow();
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    QJsonArray names;
    for(auto *output:kwinApp()->outputBackend()->outputs()) names.append(output->name());
    textures["qa"]=QJsonObject{{"scaleRequest",qEnvironmentVariable("MACGLASS_TEST_OUTPUT_SCALE")},{"applied",m_qaScaleApplied},{"outputs",names}};
    textures["qaSceneCaptures"]=qint64(m_qaSceneCaptures);
    textures["qaSceneHash"]=QString::fromLatin1(m_qaSceneHash);
#endif
    for(const auto &[w,resources]:m_buffers) {
        for(const auto &[view,buffer]:resources.views) {
            if(!buffer.texture) continue;
            QJsonObject info{{"width",buffer.texture->size().width()},{"height",buffer.texture->size().height()},{"scale",buffer.scale}};
#ifdef MACGLASS_NESTED_TEST_CAPTURE
            info["backgroundChanges"]=qint64(buffer.changes);
#endif
            textures[key(w)]=info;
        }
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"ready",m_ready},{"pid",m_pid},
        {"windows",m_windows.size()},{"draws",qint64(m_draws)},{"iconDraws",qint64(m_iconDraws)},
        {"options",m_options},{"revision","desktop-v11-isolated-shaders"},{"shaderPrefix",MACGLASS_SHADER_PREFIX},{"openings",qint64(m_openings)},
        {"panelTailSuppressed",qint64(m_panelTailSuppressed)},{"panelBackgroundRepaints",qint64(m_panelBackgroundRepaints)},
        {"scissoredCaptures",qint64(m_windowDraws.value("scissoredCaptures"))},
        {"controlCenterRepaints",qint64(m_controlCenterRepaints)},
        {"panelGenie",panelGenieSupported()},{"panelMotionFrames",qint64(m_panelMotionFrames)},{"warpedLenses",qint64(m_warpedLenses)},
        {"tailSuppressed",qint64(m_tailSuppressed)},{"settledHidden",m_settledHidden.size()},
        {"minimizeConflicts",qint64(m_minimizeConflicts)},{"squashLoaded",effects->loadedEffects().contains("squash")},
        {"minimizeStartFrames",qint64(m_minimizeStartFrames)},{"minimizeStartFades",qint64(m_minimizeStartFades)},
        {"dockFrame",dock?coordinates(dock->frameGeometry()):QJsonObject()},
        {"dockHidden",m_dock["hidden"].toBool()},
        {"lastOrigin",coordinates(m_lastOrigin)},
        {"closings",qint64(m_closings)},{"motionFrames",qint64(m_motionFrames)},
        {"activeMotions",int(m_motions.size())},{"motionShader",bool(m_motionShader)},
        {"textures",textures},{"minimizes",qint64(m_minimizes)},{"restores",qint64(m_restores)},
        {"anchoredMotions",qint64(m_anchoredMotions)},{"reveals",qint64(m_reveals)},{"edgeHovered",m_edgeHover},{"obstructed",m_obstructed}}).toJson(QJsonDocument::Compact));
}
bool MacGlassRefraction::isActive() const {
    if(effects->isScreenLocked())return false;
    if(!m_hiddenPanels.isEmpty() || !m_motions.empty() || (m_ready && m_pid>0 && !m_windows.isEmpty()))return true;
    // A retained minimized window normally has no scene visibility. Stay in
    // the chain only if another effect explicitly keeps it drawable.
    for(auto *w:m_settledHidden)if((w->isDeleted() || w->isMinimized()) && w->windowItem()->isVisible())return true;
    return false;
}
QString MacGlassRefraction::key(EffectWindow *w) const {
    if(!w || w->isDeleted() || w->pid()!=m_pid) return {};
    auto *surface=w->surface();
    if(!surface || surface->role()!=LayerSurfaceV1Interface::role()) return {};
    for(auto it=m_windows.begin();it!=m_windows.end();++it)
        if(it.value().toObject()["surface"].toInt()==int(surface->id())) return it.key();
    return {};
}
void MacGlassRefraction::requestState() {
    auto bus=QDBusConnection::sessionBus();
    auto pid=bus.interface()->servicePid("org.macglass.Dock");
    m_pid=pid.isValid() ? pid.value() : 0;
    if(!m_pid) return;
    auto call=QDBusMessage::createMethodCall("org.macglass.Dock","/Dock","org.macglass.Dock","opticalState");
    auto *watch=new QDBusPendingCallWatcher(bus.asyncCall(call,1000),this);
    connect(watch,&QDBusPendingCallWatcher::finished,this,[this,watch]{
        QDBusPendingReply<QString> reply=*watch;
        if(!reply.isError()) updateState(reply.value());
        watch->deleteLater();
    });
}
void MacGlassRefraction::updateState(const QString &json) {
    if(json.size()>262144) return;
    const auto state=QJsonDocument::fromJson(json.toUtf8()).object();
    if(state["protocol"].toInt()!=1 || state["pid"].toInteger()!=m_pid) return;
    const auto windows=state["windows"].toObject();
    if(windows.size()>8) return;
    for(const auto &value:windows) {
        const auto w=value.toObject();
        if(w.contains("genie")) {
            const auto motion=w["genie"].toObject();
            for(const char *name:{"frame","icon"}) {
                const auto r=rect(motion[name].toObject());
                if(!std::isfinite(r.x()) || !std::isfinite(r.y()) || !std::isfinite(r.width()) || !std::isfinite(r.height())
                    || r.width()<=0 || r.height()<=0 || r.width()>16384 || r.height()>16384
                    || std::abs(r.x())>16384 || std::abs(r.y())>16384)return;
            }
            const double openness=motion["openness"].toDouble(-1);
            if(!std::isfinite(openness) || openness<0 || openness>1)return;
        }
        if(w["lenses"].toArray().size()>128 || w["width"].toDouble()>16384 || w["height"].toDouble()>16384) return;
        for(const auto &v:w["lenses"].toArray()) {
            const auto o=v.toObject();const auto r=rect(o);
            if(!std::isfinite(r.x()) || !std::isfinite(r.y()) || !std::isfinite(r.width()) || !std::isfinite(r.height())
                || r.width()<0 || r.height()<0 || r.width()>16384 || r.height()>16384
                || std::abs(r.x())>16384 || std::abs(r.y())>16384) return;
            if(o.contains("clip")) {
                const auto c=rect(o["clip"].toObject());
                if(!std::isfinite(c.x()) || !std::isfinite(c.y()) || !std::isfinite(c.width()) || !std::isfinite(c.height())
                    || c.width()<0 || c.height()<0 || c.width()>16384 || c.height()>16384
                    || std::abs(c.x())>16384 || std::abs(c.y())>16384) return;
            }
        }
    }
    const auto dock=state["dock"].toObject();
    if(dock["targets"].toArray().size()>128)return;
    for(const auto &target:dock["targets"].toArray()) {
        const auto t=target.toObject();const auto r=rect(t);
        if(t["aliases"].toArray().size()>8 || !std::isfinite(r.x()) || !std::isfinite(r.y())
            || !std::isfinite(r.width()) || !std::isfinite(r.height()) || r.width()<0 || r.height()<0
            || r.width()>512 || r.height()>512 || std::abs(r.x())>16384 || std::abs(r.y())>16384)return;
        for(const auto &alias:t["aliases"].toArray())if(alias.toString().size()>512)return;
    }
    m_options=state["options"].toObject(); m_windows=windows;m_dock=dock;
    m_syntheticInput=state["syntheticInput"].toBool();
    refreshAnchors();handlePointer(effects->cursorPos(),true);
    syncOcclusion();
    if(!m_options["windowAnimations"].toBool()) clearMotions();
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    m_clientReady=state["backend"].toBool();
#endif
    syncBackgrounds();
    syncPanelMotions();
    for(auto *w:effects->stackingOrder()) if(!key(w).isEmpty()) w->addRepaintFull();
}
void MacGlassRefraction::syncBackgrounds() {
    effects->makeOpenGLContextCurrent();
    for(auto it=m_buffers.begin();it!=m_buffers.end();) {
        if(key(it->first).isEmpty()) it=m_buffers.erase(it); else ++it;
    }
    for(auto *w:effects->stackingOrder()) {
        if(key(w).isEmpty()) continue;
        auto &resources=m_buffers[w];
        if(!resources.background) resources.background=std::make_unique<BackgroundEffectItem>(w->windowItem());
        // KWin 6.8 tracks background sampling in the scene graph. Merely
        // marking a window translucent does not expand damage behind lenses.
        QRectF bounds(QPointF(0,0),w->size());
        if(key(w)=="launcherWindow") {
            const auto state=m_windows.value("launcherWindow").toObject();
            if(state.contains("genie"))bounds=rect(state["input"].toObject());
            else {
                bounds={};
                for(const auto &lens:state["lenses"].toArray())bounds=bounds.united(rect(lens.toObject()));
            }
        }
        resources.background->setEffectBoundingRect(RectF(bounds));
        resources.background->setPixelsToExpandRepaintsBelowOpaqueRegions(padding);
    }
}
void MacGlassRefraction::prePaintScreen(ScreenPrePaintData &data) {
    m_currentView=data.view;
    // A partial hover repaint must refresh every pixel sampled by the control
    // center's lenses, rather than reading last frame's already-composited
    // glass outside the small damage rectangle. This expands this frame only;
    // it does not request another frame or keep an idle panel repainting.
    const auto controlCenter=m_windows.value("controlCenterWindow").toObject();
    if(controlCenter["presented"].toBool() && !controlCenter.contains("genie")) {
        for(auto *w:effects->stackingOrder()) {
            if(key(w)!="controlCenterWindow" || w->isDeleted())continue;
            QRectF bounds;
            for(const auto &lens:controlCenter["lenses"].toArray())
                bounds=bounds.united(rect(lens.toObject()));
            if(!bounds.isEmpty()) {
                data.paint|=RectF(bounds.translated(w->frameGeometry().topLeft())
                    .adjusted(-padding,-padding,padding,padding)).roundedOut();
                ++m_controlCenterRepaints;
            }
            break;
        }
    }
    if(effects->isScreenLocked() || effects->activeFullScreenEffect()) clearMotions();
    if(!m_panelMotions.isEmpty())data.mask|=PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    for(auto &[w,motion]:m_motions) {
        motion->time.advance(data.view);
        data.mask|=PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    }
    effects->prePaintScreen(data);
}
void MacGlassRefraction::prePaintWindow(RenderView *view,EffectWindow *w,WindowPrePaintData &data) {
    if(m_windows.contains(key(w))) data.setTranslucent();
    if(m_motions.contains(w) || m_panelMotions.contains(w)) {data.setTransformed();data.setTranslucent();}
    effects->prePaintWindow(view,w,data);
}
#ifdef MACGLASS_NESTED_TEST_CAPTURE
void MacGlassRefraction::testCaptureOwnedScene() {
    if(!qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE"))return;
    m_qaCapturePending=true;effects->addRepaintFull();
}
bool MacGlassRefraction::paintScreen(const RenderTarget &target,const RenderViewport &viewport,int mask,const Region &damage,LogicalOutput *output) {
    const bool ok=effects->paintScreen(target,viewport,mask,damage,output);
    if(!ok || !m_qaCapturePending || !target.framebuffer())return ok;
    m_qaCapturePending=false;
    const auto r=viewport.renderRect().roundedOut();
    auto texture=GLTexture::allocate(GL_RGBA8,QSize(qCeil(r.width()*viewport.scale()),qCeil(r.height()*viewport.scale())));
    if(!texture)return ok;
    GLFramebuffer frame(texture.get());
    if(!frame.valid() || !frame.blitFromRenderTarget(target,viewport,r,Rect(QPoint(0,0),texture->size())))return ok;
    const auto image=texture->toImage().flipped(Qt::Vertical);
    // Exclude the Dock row: hover/spring differences are not a popup ghost.
    const auto upper=image.copy(0,0,image.width(),image.height()*2/3).convertToFormat(QImage::Format_RGBA8888);
    m_qaSceneHash=QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char *>(upper.constBits()),upper.sizeInBytes()),QCryptographicHash::Sha256).toHex();
    ++m_qaSceneCaptures;
    image.save(qEnvironmentVariable("MACGLASS_NESTED_CAPTURE")+"-ordinary-"+QString::number(m_qaSceneCaptures)+".png");
    return ok;
}
#endif
bool MacGlassRefraction::renderLenses(const RenderTarget &target,const RenderViewport &viewport,EffectWindow *w,const Region &damage,WindowPaintData &data) {
    const QString id=key(w);
    if(!m_windows.contains(id) || (data.xScale()!=1 || data.yScale()!=1 || data.xTranslation()!=0 || data.yTranslation()!=0)
        || effects->activeFullScreenEffect() || !target.framebuffer()) return true;
    const QJsonArray list=m_windows.value(id).toObject()["lenses"].toArray();
    if(list.isEmpty()) return true;
    const QRectF windowRect=w->frameGeometry();
    const auto motion=m_windows.value(id).toObject()["genie"].toObject();
    const bool warped=!motion.isEmpty();
    const QRectF movingFrame=rect(motion["frame"].toObject()).translated(windowRect.topLeft());
    const QRectF movingIcon=rect(motion["icon"].toObject()).translated(windowRect.topLeft());
    const double openness=qBound(0.,motion["openness"].toDouble(1),1.);
    QRectF sampleBounds=windowRect;
    if(id=="launcherWindow") {
        if(warped)sampleBounds=panelMotionBounds(w);
        else {
            sampleBounds={};
            for(const auto &lens:list)sampleBounds=sampleBounds.united(rect(lens.toObject()).translated(windowRect.topLeft()));
        }
        sampleBounds=sampleBounds.adjusted(-padding,-padding,padding,padding).intersected(windowRect);
    }
    const QRectF crop=sampleBounds.intersected(QRectF(viewport.renderRect()));
    if(crop.isEmpty()) return true;
    const Rect source=RectF(crop).roundedOut();
    const QSize size(qMax(1,qCeil(source.width()*viewport.scale())),qMax(1,qCeil(source.height()*viewport.scale())));
    const GLenum format=target.texture() ? target.texture()->internalFormat() : GL_RGBA8;
    auto &buffer=m_buffers[w].views[m_currentView];
    buffer.scale=viewport.scale();
    if(!buffer.texture || buffer.texture->size()!=size || buffer.texture->internalFormat()!=format) {
        buffer.fbo.reset(); buffer.texture=GLTexture::allocate(format,size);
        if(!buffer.texture) return false;
        buffer.texture->setFilter(GL_LINEAR); buffer.texture->setWrapMode(GL_CLAMP_TO_EDGE);
        buffer.fbo=std::make_unique<GLFramebuffer>(buffer.texture.get());
        if(!buffer.fbo->valid()) return false;
    }
    const auto capture=[&] {
        // Blits obey GL_SCISSOR_TEST in destination framebuffer coordinates.
        // The compositor's inherited scissor is in SCREEN coordinates, whereas
        // this cropped texture starts at (0,0). Leaving it enabled silently
        // skips/partially updates the backdrop while reporting a successful blit.
        const bool scissor=glIsEnabled(GL_SCISSOR_TEST);
        if(scissor) {++m_windowDraws["scissoredCaptures"];glDisable(GL_SCISSOR_TEST);}
        const bool ok=buffer.fbo->blitFromRenderTarget(target,viewport,source,Rect(QPoint(0,0),size));
        if(scissor)glEnable(GL_SCISSOR_TEST);
        return ok;
    };
    if(!capture()) return false;
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    // QA only: prove real source pixels change while the Dock itself is idle.
    const QImage backdrop=buffer.texture->toImage();
    const auto hash=QCryptographicHash::hash(QByteArrayView(reinterpret_cast<const char *>(backdrop.constBits()),backdrop.sizeInBytes()),QCryptographicHash::Sha256);
    if(buffer.lastHash!=hash) {buffer.lastHash=hash;++buffer.changes;}
#endif
    GLint active=0,binding=0,src=0,dst=0,srcAlpha=0,dstAlpha=0;
    glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&binding);
    const bool blending=glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB,&src);glGetIntegerv(GL_BLEND_DST_RGB,&dst);
    glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
    glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);
    ShaderBinder binder(m_shader.get());
    m_shader->setUniform("modelViewProjectionMatrix",viewport.projectionMatrix());
    m_shader->setUniform("backdrop",0);
    m_shader->setUniform("cropSize",QVector2D(source.width(),source.height()));
    m_shader->setUniform("warped",warped);
    m_shader->setUniform("renderScale",float(viewport.scale()));
    m_shader->setUniform("cropOrigin",QVector2D(source.x(),source.y()));
    const bool blur=m_options["blurEnabled"].toBool(true);
    const Region logicalDamage=viewport.mapFromDeviceCoordinatesAligned(damage);
    auto *vbo=GLVertexBuffer::streamingBuffer();
    auto draw=[&](const QJsonObject &lens){
        const QRectF r=rect(lens).translated(windowRect.topLeft());
        QRectF visible=r.intersected(windowRect);
        if(lens.contains("clip")) visible=visible.intersected(rect(lens["clip"].toObject()).translated(windowRect.topLeft()));
        if(visible.isEmpty())return;
        const bool icon=lens["icon"].toBool();
        const bool iconBlur=m_options["iconBlurEnabled"].toBool(true);
        m_shader->setUniform("box",QVector4D(r.x()-source.x(),r.y()-source.y(),r.width(),r.height()));
        const QPointF center=warped?MacGlassGenie::map(r.center()-movingFrame.topLeft(),movingFrame,movingIcon,1.-openness):r.center();
        m_shader->setUniform("lensCenter",QVector2D(center.x()-source.x(),center.y()-source.y()));
        m_shader->setUniform("radius",float(option(lens,"r",20,0,qMin(r.width(),r.height())*.5)));
        m_shader->setUniform("band",float(option(lens,"refractionWidth",option(m_options,icon?"iconRefractionWidth":"refractionWidth",icon?14:24,1,60),1,60)));
        m_shader->setUniform("bend",float(option(lens,"refractionStrength",option(m_options,icon?"iconRefractionStrength":"refractionStrength",icon?10:12,0,40),0,40)));
        m_shader->setUniform("profile",float(option(m_options,"refractionCurve",1.6,.25,4)));
        m_shader->setUniform("blurRadius",float(blur && (!icon || iconBlur) ? option(lens,"blurRadius",option(m_options,icon?"iconBlurRadius":"blurRadius",icon?3:2,0,12),0,12) : 0));
        m_shader->setUniform("dispersion",float(option(m_options,"chromaticAberration",.5,0,3)));
        m_shader->setUniform("zoom",float(option(m_options,"centerZoom",1,.9,1.15)));
        m_shader->setUniform("opacity",float(option(lens,"opacity",1,0,1)*w->opacity()*data.opacity()*(warped?MacGlassGenie::opacity(openness):1)));
        QList<GLVertex2D> vertices;
        const auto vertex=[&](qreal x,qreal y){
            const QPointF p=warped?MacGlassGenie::map(QPointF(x,y)-movingFrame.topLeft(),movingFrame,movingIcon,1.-openness):QPointF(x,y);
            return GLVertex2D{QVector2D(p.x()*viewport.scale(),p.y()*viewport.scale()),
                QVector2D((x-source.x())/source.width(),1-(y-source.y())/source.height())};
        };
        const auto quad=[&](const QRectF &q){
            vertices<<vertex(q.left(),q.top())<<vertex(q.right(),q.top())<<vertex(q.left(),q.bottom())
                <<vertex(q.left(),q.bottom())<<vertex(q.right(),q.top())<<vertex(q.right(),q.bottom());
        };
        if(warped) {
            const int cols=qBound(2,qCeil(visible.width()/qMax(1.,movingFrame.width())*48),48);
            const int rows=qBound(2,qCeil(visible.height()/qMax(1.,movingFrame.height())*96),96);
            vertices.reserve(cols*rows*6);
            for(int y=0;y<rows;++y)for(int x=0;x<cols;++x)
                quad(QRectF(visible.x()+visible.width()*x/cols,visible.y()+visible.height()*y/rows,visible.width()/cols,visible.height()/rows));
        } else for(const auto &clip:logicalDamage.rects()) {
            const QRectF q=visible.intersected(QRectF(clip));if(!q.isEmpty())quad(q);
        }
        if(vertices.isEmpty())return;
        ++m_draws; if(icon)++m_iconDraws;
        buffer.texture->bind();vbo->reset();vbo->setVertices(vertices);
        if(warped) {
            ++m_warpedLenses;
            const bool scissor=glIsEnabled(GL_SCISSOR_TEST);
            GLint box[4];glGetIntegerv(GL_SCISSOR_BOX,box);glEnable(GL_SCISSOR_TEST);
            vbo->render(viewport.mapToRenderTarget(logicalDamage),GL_TRIANGLES,true);
            glScissor(box[0],box[1],box[2],box[3]);if(!scissor)glDisable(GL_SCISSOR_TEST);
        } else vbo->render(GL_TRIANGLES);
    };
    // The largest surface is the base lens. Child controls and icons get additional
    // distortion from a fresh cropped underlay, with no client content in the copy.
    int base=-1;double area=0;
    for(int i=0;i<list.size();++i){const auto o=list[i].toObject();const auto r=rect(o);if(!o["icon"].toBool() && r.width()*r.height()>area){base=i;area=r.width()*r.height();}}
    if(id=="controlCenterWindow") {
        // These are separate tiles, not nested lenses. They must all sample
        // the same untouched backdrop; recapturing after the largest tile
        // feeds the glass back into neighboring controls on partial redraws.
        for(const auto &lens:list)draw(lens.toObject());
    } else {
        if(base>=0) draw(list[base].toObject());
        bool hasControls=false,hasIcons=false;
        for(int i=0;i<list.size();++i)if(i!=base){if(list[i].toObject()["icon"].toBool())hasIcons=true;else hasControls=true;}
        if(base>=0 && (hasControls || hasIcons)) capture();
        if(hasControls)for(int i=0;i<list.size();++i)if(i!=base && !list[i].toObject()["icon"].toBool())draw(list[i].toObject());
        if(hasControls && hasIcons)capture();
        if(hasIcons)for(int i=0;i<list.size();++i)if(list[i].toObject()["icon"].toBool())draw(list[i].toObject());
    }
    glBindTexture(GL_TEXTURE_2D,binding);glActiveTexture(active);
    glBlendFuncSeparate(src,dst,srcAlpha,dstAlpha);
    if(!blending)glDisable(GL_BLEND);
    return true;
}
bool MacGlassRefraction::drawWindow(const RenderTarget &target,const RenderViewport &viewport,EffectWindow *w,int mask,const Region &damage,WindowPaintData &data) {
    if(m_hiddenPanels.contains(w)) {++m_panelTailSuppressed;return true;}
    if(m_panelMotions.contains(w)) {
        data.setOpacity(1);data.setXScale(1);data.setYScale(1);data.setZScale(1);
        data.setXTranslation(0);data.setYTranslation(0);data.setZTranslation(0);data.setRotationAngle(0);
        ShaderBinder binder(m_motionShader.get());
        m_motionShader->setUniform("motionBlur",float(1.25*(1-m_windows.value(key(w)).toObject()["genie"].toObject()["openness"].toDouble(1))*viewport.scale()));
    }
    if(m_settledHidden.contains(w) && !m_motions.contains(w) && (w->isMinimized() || w->isDeleted())) {
        ++m_tailSuppressed;
        return true;
    }
    if(m_ready && !renderLenses(target,viewport,w,damage,data)) {
        // Fail closed: stop using the backend. Client watchdog restores standard blur.
        m_ready=false; qWarning("MacGlass refraction: framebuffer failure, backend disabled");
    }
    if(m_motionShader && m_motions.contains(w)) {
        ShaderBinder binder(m_motionShader.get());
        m_motionShader->setUniform("motionBlur",float(2.2*std::pow(1-m_motions[w]->openness(),1.3)*viewport.scale()));
        ++m_motionFrames;
    }
    const bool ok=OffscreenEffect::drawWindow(target,viewport,w,mask,damage,data);
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    // Compiled only into a separate QA build, run in the isolated compositor.
    // No capture endpoint exists in the installed/production module.
    const QString id=key(w),prefix=qEnvironmentVariable("MACGLASS_NESTED_CAPTURE");
    const auto lenses=m_windows.value(id).toObject()["lenses"].toArray();
    const bool probe=w->windowClass().contains("mac-glass-dock-probe") && !m_motions.contains(w);
    const QString captureId=probe?QString("probeWindow"):id;
    const auto panelMotion=m_windows.value(id).toObject()["genie"].toObject();
    const bool settled=probe || (panelMotion.isEmpty() && !lenses.isEmpty() && lenses.first().toObject()["opacity"].toDouble()>.99);
    if(ok && id=="launcherWindow" && !panelMotion.isEmpty() && panelMotion["openness"].toDouble()>.35
        && panelMotion["openness"].toDouble()<.7 && m_clientReady && !prefix.isEmpty() && !m_captured.contains("launcher-moving-glass")) {
        const Rect r=w->frameGeometry().intersected(viewport.renderRect()).roundedOut();
        auto texture=GLTexture::allocate(GL_RGBA8,QSize(qCeil(r.width()*viewport.scale()),qCeil(r.height()*viewport.scale())));
        if(texture) {
            GLFramebuffer frame(texture.get());
            if(frame.valid() && frame.blitFromRenderTarget(target,viewport,r,Rect(QPoint(0,0),texture->size())))
                if(texture->toImage().flipped(Qt::Vertical).save(prefix+"-launcher-moving-glass.png"))m_captured.insert("launcher-moving-glass");
        }
    }
    if(ok && settled && ++m_windowDraws[captureId]>(probe?1:20) && m_clientReady && !prefix.isEmpty() && !captureId.isEmpty() && !m_captured.contains(captureId)) {
        const Rect r=w->frameGeometry().intersected(viewport.renderRect()).roundedOut();
        auto texture=GLTexture::allocate(GL_RGBA8,QSize(qCeil(r.width()*viewport.scale()),qCeil(r.height()*viewport.scale())));
        if(texture) {
            GLFramebuffer frame(texture.get());
            if(frame.valid() && frame.blitFromRenderTarget(target,viewport,r,Rect(QPoint(0,0),texture->size()))) {
                if(texture->toImage().flipped(Qt::Vertical).save(prefix+"-"+captureId+".png")) m_captured.insert(captureId);
            }
        }
    }
    if(ok && !prefix.isEmpty() && w->windowClass().contains("mac-glass-dock-probe") && m_motions.contains(w)
        // Ignore recursive rendering into the window's own offscreen texture.
        && (viewport.renderRect().width()>w->expandedGeometry().width()+4 || viewport.renderRect().height()>w->expandedGeometry().height()+4)) {
        const auto &motion=m_motions[w];
        const double progress=1.-motion->openness();
        const bool minimize=motion->role==WindowMinimizedGrabRole;
        const QString direction=minimize?"minimize":motion->role==WindowUnminimizedGrabRole?"restore":motion->closing?"close":"open";
        for(int phase=minimize?-1:0;phase<3;++phase) {
            const double at=phase<0?0:.20+phase*.25;
            const QString name="genie-"+direction+"-"+QString::number(phase);
            if(progress<at || progress>at+.16 || m_captured.contains(name))continue;
            const QRectF bounds=QRectF(w->frameGeometry()).united(motion->origin).adjusted(-16,-16,16,16);
            const Rect r=RectF(bounds).intersected(viewport.renderRect()).roundedOut();
            auto texture=GLTexture::allocate(GL_RGBA8,QSize(qCeil(r.width()*viewport.scale()),qCeil(r.height()*viewport.scale())));
            if(texture) {
                GLFramebuffer frame(texture.get());
                if(frame.valid() && frame.blitFromRenderTarget(target,viewport,r,Rect(QPoint(0,0),texture->size())))
                    if(texture->toImage().flipped(Qt::Vertical).save(prefix+"-"+name+".png"))m_captured.insert(name);
            }
        }
    }
#endif
    return ok;
}
KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED(MacGlassRefraction,"metadata.json",return MacGlassRefraction::supported();,return false;)
}
#include "refraction.moc"

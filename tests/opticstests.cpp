#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QPainter>
#include <QTest>
#include <memory>
class OpticsTests : public QObject {
    Q_OBJECT
    QOpenGLContext context;
    QOffscreenSurface surface;
    QOpenGLShaderProgram program;
    QOpenGLBuffer vertices{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject vao;
    QImage pattern;
    QImage render(const QImage &input,float bend,float blur,const QRectF &box=QRectF(8,8,240,176),float radius=32,float band=24,
                  bool warped=false,float sourceUvOffset=0,float zoom=1) {
        QOpenGLFramebufferObject fbo(256,192);
        fbo.bind();
        auto *gl=context.functions();
        gl->glViewport(0,0,256,192);gl->glClearColor(0,0,0,0);gl->glClear(GL_COLOR_BUFFER_BIT);
#if QT_VERSION >= QT_VERSION_CHECK(6,9,0)
        QOpenGLTexture texture(input.flipped(Qt::Vertical));
#else
        QOpenGLTexture texture(input.mirrored());
#endif
        texture.setMinMagFilters(QOpenGLTexture::Linear,QOpenGLTexture::Linear);
        texture.setWrapMode(QOpenGLTexture::ClampToEdge);
        program.bind();
        QMatrix4x4 projection;
        if(warped)projection.ortho(0.f,256.f,192.f,0.f,-1.f,1.f);
        program.setUniformValue("modelViewProjectionMatrix",projection);
        program.setUniformValue("warped",warped);
        program.setUniformValue("renderScale",1.f);
        program.setUniformValue("cropOrigin",QVector2D(0,0));
        program.setUniformValue("lensCenter",QVector2D(128,96));
        program.setUniformValue("backdrop",0);
        program.setUniformValue("cropSize",QVector2D(256,192));
        program.setUniformValue("box",QVector4D(box.x(),box.y(),box.width(),box.height()));
        program.setUniformValue("radius",radius);program.setUniformValue("bend",bend);
        program.setUniformValue("band",band);program.setUniformValue("profile",1.6f);
        program.setUniformValue("blurRadius",blur);program.setUniformValue("dispersion",0.f);
        program.setUniformValue("zoom",zoom);program.setUniformValue("opacity",1.f);
        vao.bind(); vertices.bind();texture.bind();
        const float left=warped?0:-1,right=warped?256:1,bottom=warped?192:-1,top=warped?0:1;
        const float v[]={left,bottom,sourceUvOffset,0, right,bottom,1+sourceUvOffset,0,
                         left,top,sourceUvOffset,1, right,top,1+sourceUvOffset,1};
        vertices.allocate(v,sizeof(v));
        gl->glDrawArrays(GL_TRIANGLE_STRIP,0,4);
        gl->glFinish();
        const auto image=fbo.toImage();
        texture.release();vertices.release();vao.release();program.release();fbo.release();
        return image;
    }
    static int differences(const QImage &a,const QImage &b,const QRect &r) {
        int n=0;for(int y=r.top();y<=r.bottom();++y)for(int x=r.left();x<=r.right();++x)
            if(a.pixelColor(x,y)!=b.pixelColor(x,y))++n;
        return n;
    }
private slots:
    void initTestCase() {
        QSurfaceFormat format;format.setVersion(3,3);format.setProfile(QSurfaceFormat::CoreProfile);
        context.setFormat(format);
        if(!context.create()) QSKIP("No OpenGL context in this QPA backend; run natively for GPU coverage");
        surface.setFormat(context.format());surface.create();
        QVERIFY(context.makeCurrent(&surface));
        QVERIFY2(program.addShaderFromSourceFile(QOpenGLShader::Vertex,QStringLiteral(DOCK_SOURCE_DIR "/kwin/lens.vert")),qPrintable(program.log()));
        QVERIFY2(program.addShaderFromSourceFile(QOpenGLShader::Fragment,QStringLiteral(DOCK_SOURCE_DIR "/kwin/lens.frag")),qPrintable(program.log()));
        program.bindAttributeLocation("position",0);program.bindAttributeLocation("texcoord",1);
        QVERIFY2(program.link(),qPrintable(program.log()));
        QVERIFY(vao.create());vao.bind();QVERIFY(vertices.create());vertices.bind();
        const float v[]={-1,-1,0,0, 1,-1,1,0, -1,1,0,1, 1,1,1,1};
        vertices.allocate(v,sizeof(v));
        program.bind();program.enableAttributeArray(0);program.enableAttributeArray(1);
        program.setAttributeBuffer(0,GL_FLOAT,0,2,4*sizeof(float));
        program.setAttributeBuffer(1,GL_FLOAT,2*sizeof(float),2,4*sizeof(float));
        vertices.release();vao.release();program.release();
        pattern=QImage(256,192,QImage::Format_RGBA8888);
        for(int y=0;y<192;++y)for(int x=0;x<256;++x) {
            const bool line=x%16<2 || y%16<2;
            pattern.setPixelColor(x,y,line ? QColor("#f2f5ff") : QColor::fromHsv((x/32*35+y/32*16)%360,180,110+(x+y)%90));
        }
    }
    void realEdgeDisplacement() {
        const auto baseline=render(pattern,0,0),bent=render(pattern,12,0);
        QVERIFY(!bent.isNull());
        QVERIFY(differences(baseline,bent,QRect(24,12,200,20))>100);
        QCOMPARE(differences(baseline,bent,QRect(100,75,50,40)),0);
        QCOMPARE(bent.pixelColor(0,0).alpha(),0);
    }
    void blurAndSeparateIconLens() {
        const auto clear=render(pattern,0,0),soft=render(pattern,0,2),frost=render(pattern,0,8);
        QVERIFY(differences(clear,soft,QRect(24,24,200,144))>1000);
        QVERIFY(differences(soft,frost,QRect(24,24,200,144))>1000);
        const auto base=render(pattern,12,2);
        const auto icon=render(base,10,3,QRectF(88,56,80,80),20,14);
        QVERIFY(differences(base,icon,QRect(90,58,76,76))>100);
        const auto root=QStringLiteral(DOCK_BINARY_DIR "/optics-shader-comparison.png");
        QImage comparison(768,216,QImage::Format_ARGB32_Premultiplied);comparison.fill(QColor("#142335"));
        QPainter p(&comparison);
        p.drawImage(0,24,pattern);p.drawImage(256,24,base);
        p.drawImage(512,24,base);p.drawImage(512,24,icon);
        p.setPen(Qt::white);p.drawText(8,17,"Owned test image");p.drawText(264,17,"Edge refraction + blur 2 px");p.drawText(520,17,"Additional icon lens + blur 3 px");
        p.end();QVERIFY(comparison.save(root));
    }
    void movingGlassDoesNotCarrySourceBackground() {
        // Hold destination geometry still but change its source material UVs,
        // as happens when a different part of a Genie sheet reaches this spot.
        // The clear interior must sample the SAME screen pixels even with the
        // user's .94 zoom, not drag the source-window image through the neck.
        const QRectF clearInterior(-512,-512,1280,1216);
        const auto first=render(pattern,0,0,clearInterior,0,24,true,0,.94);
        const auto shifted=render(pattern,0,0,clearInterior,0,24,true,.4,.94);
        QVERIFY(!first.isNull());
        QCOMPARE(differences(first,shifted,QRect(24,24,200,144)),0);
        const auto updated=render(pattern.rgbSwapped(),0,0,clearInterior,0,24,true,.4,.94);
        QVERIFY(differences(shifted,updated,QRect(24,24,200,144))>1000);
    }
    void cleanupTestCase() { if(context.isValid()){context.makeCurrent(&surface);vertices.destroy();vao.destroy();program.removeAllShaders();} }
};
QTEST_MAIN(OpticsTests)
#include "opticstests.moc"

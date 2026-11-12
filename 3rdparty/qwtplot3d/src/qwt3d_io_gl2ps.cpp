#if defined(_MSC_VER) /* MSVC Compiler */
#pragma warning(disable : 4786)
#endif

#include <stdio.h>
#include <time.h>
#include <cmath>
#ifdef _WIN32
#include <wchar.h>
#endif
#include <QByteArray>
#include <QFile>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <memory>
#include "qwt3d_openglhelper.h"
#include "gl2ps.h"
#include "qwt3d_io_gl2ps.h"
#include "qwt3d_plot.h"

using namespace Qwt3D;

namespace {

FILE *openGl2psOutputFile(QString const &path)
{
    if (path.isEmpty())
        return nullptr;
#ifdef _WIN32
    return _wfopen(reinterpret_cast<const wchar_t *>(path.utf16()), L"wb");
#else
    const QByteArray encoded = QFile::encodeName(path);
    return fopen(encoded.constData(), "wb");
#endif
}

} // namespace

//! Provides a new VectorWriter object.
IO::Functor *VectorWriter::clone() const
{
    return new VectorWriter(*this);
}

VectorWriter::VectorWriter()
    : gl2ps_format_(GL2PS_EPS),
      formaterror_(false),
#ifdef GL2PS_HAVE_ZLIB
      compressed_(true),
#else
      compressed_(false),
#endif
      sortmode_(SIMPLESORT),
      landscape_(VectorWriter::AUTO),
      textmode_(VectorWriter::PIXEL),
      texfname_("")
{
}

/*!
  Sets the mode for text output:\n
  \param val The underlying format for the generated output:\n
  PIXEL - poor quality but exact positioning\n
  NATIVE - high quality but inexact positioning\n
  TEX - high quality and exact positioning, arbitrary TeX strings as content for
  the saved labels are possible. The disadvantage is the need for an additionally TeX run
  to get the final output.\n
  \param fname Optional, used only in conjunction with TeX output; file name
  for the generated TeX file. If not set, a file called "OUTPUT.FOR.tex"
  will be generated, where "OUTPUT.FOR" describes the file name argument for IO::save().\n\n
  (04/05/27: On Linux platforms, pdflatex seems a file named 'dump_0.pdf.tex' mistakenly to
  identify as PDF file.)
*/
void VectorWriter::setTextMode(TEXTMODE val, QString fname)
{
    textmode_ = val;
    texfname_ = (fname.isEmpty()) ? QString("") : fname;
}

#ifdef GL2PS_HAVE_ZLIB
//! Turns compressed output on or off (no effect if zlib support has not been set)
void VectorWriter::setCompressed(bool val)
{
    compressed_ = val;
}
#else
//! Turns compressed output on or off (no effect if zlib support has not been set)
void VectorWriter::setCompressed(bool)
{
    compressed_ = false;
}
#endif

/*!
Set output format, must be one of "EPS_GZ", "PS_GZ", "EPS",
"PS", "PDF", "SVG" or "PGF" (case sensitive)
*/
bool VectorWriter::setFormat(QString const &format)
{
    if (format == QString("EPS")) {
        gl2ps_format_ = GL2PS_EPS;
    } else if (format == QString("PS")) {
        gl2ps_format_ = GL2PS_PS;
    } else if (format == QString("PDF")) {
        gl2ps_format_ = GL2PS_PDF;
    } else if (format == QString("SVG")) {
        gl2ps_format_ = GL2PS_SVG;
    } else if (format == QString("PGF")) {
        gl2ps_format_ = GL2PS_PGF;
    }
#ifdef GL2PS_HAVE_ZLIB
    else if (format == QString("EPS_GZ")) {
        gl2ps_format_ = GL2PS_EPS;
    } else if (format == QString("PS_GZ")) {
        gl2ps_format_ = GL2PS_PS;
    }
#endif
    else {
        formaterror_ = true;
        return false;
    }
    formaterror_ = false;
    return true;
}

//! Performs actual output
bool VectorWriter::operator()(Plot3D *plot, QString const &fname)
{
    if (formaterror_ || !plot)
        return false;

    if (plot->width() <= 0 || plot->height() <= 0)
        plot->resize(500, 400);

    std::unique_ptr<QOffscreenSurface> offscreenSurface;
    std::unique_ptr<QOpenGLContext> offscreenContext;
    bool usingOffscreen = false;

    if (!plot->context()) {
        (void)plot->grabFramebuffer();
    }
    if (plot->context()) {
        plot->makeCurrent();
        if (!plot->initializedGL())
            plot->initializeGL();
        plot->updateData();
    } else {
        QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
        fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
        fmt.setDepthBufferSize(24);
        offscreenSurface = std::make_unique<QOffscreenSurface>();
        offscreenSurface->setFormat(fmt);
        offscreenSurface->create();
        offscreenContext = std::make_unique<QOpenGLContext>();
        offscreenContext->setFormat(fmt);
        if (!offscreenSurface->isValid() || !offscreenContext->create()
            || !offscreenContext->makeCurrent(offscreenSurface.get())) {
            return false;
        }
        usingOffscreen = true;
        plot->initializeGL();
        GLStateBewarer dt(GL_DEPTH_TEST, true);
        GLStateBewarer ls(GL_LINE_SMOOTH, true);
        plot->calculateHull();
        SaveGlDeleteLists(plot->displaylists_p[Plot3D::DataObject], 1);
        plot->displaylists_p[Plot3D::DataObject] = glGenLists(1);
        glNewList(plot->displaylists_p[Plot3D::DataObject], GL_COMPILE);
        plot->createEnrichments();
        plot->createData();
        glEndList();
    }

    auto makeActiveCurrent = [&]() {
        if (usingOffscreen)
            offscreenContext->makeCurrent(offscreenSurface.get());
        else
            plot->makeCurrent();
    };
    auto cleanupOffscreen = [&]() {
        if (usingOffscreen && offscreenContext && offscreenSurface) {
            offscreenContext->makeCurrent(offscreenSurface.get());
            SaveGlDeleteLists(plot->displaylists_p[Plot3D::DataObject], 1);
            plot->displaylists_p[Plot3D::DataObject] = 0;
            plot->initializedGL_ = false;
            offscreenContext->doneCurrent();
        }
    };

    GLint bufsize = 0, state = GL2PS_OVERFLOW;
    GLint viewport[4] = { 0, 0, 0, 0 };

    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) {
        viewport[0] = 0;
        viewport[1] = 0;
        viewport[2] = (plot->width() > 0) ? plot->width() : 500;
        viewport[3] = (plot->height() > 0) ? plot->height() : 400;
        glViewport(0, 0, viewport[2], viewport[3]);
    }
    glClearColor(static_cast<GLclampf>(plot->bgcolor_.r),
                 static_cast<GLclampf>(plot->bgcolor_.g),
                 static_cast<GLclampf>(plot->bgcolor_.b),
                 static_cast<GLclampf>(plot->bgcolor_.a));

    GLint options = GL2PS_SIMPLE_LINE_OFFSET | GL2PS_SILENT | GL2PS_DRAW_BACKGROUND
            | GL2PS_OCCLUSION_CULL | GL2PS_BEST_ROOT;

    if (compressed_
        && !(gl2ps_format_ == GL2PS_SVG
             && !fname.endsWith(QLatin1String(".svgz"), Qt::CaseInsensitive))) {
        options |= GL2PS_COMPRESS;
    }

    switch (landscape_) {
    case VectorWriter::AUTO:
        if (viewport[2] - viewport[0] > viewport[3] - viewport[0])
            options |= GL2PS_LANDSCAPE;
        break;
    case VectorWriter::ON:
        options |= GL2PS_LANDSCAPE;
        break;
    default:
        break;
    }

    int sortmode = GL2PS_SIMPLE_SORT;
    switch (sortmode_) {
    case VectorWriter::NOSORT:
        sortmode = GL2PS_NO_SORT;
        break;
    case VectorWriter::SIMPLESORT:
        sortmode = GL2PS_SIMPLE_SORT;
        break;
    case VectorWriter::BSPSORT:
        sortmode = GL2PS_BSP_SORT;
        break;
    default:
        break;
    }

    switch (textmode_) {
    case NATIVE:
        Label::useDeviceFonts(true);
        break;
    case PIXEL:
        Label::useDeviceFonts(false);
        break;
    case TEX:
        options |= GL2PS_NO_PIXMAP | GL2PS_NO_TEXT;
        break;
    default:
        break;
    }

    QString version = QString::number(QWT3D_MAJOR_VERSION) + "."
            + QString::number(QWT3D_MINOR_VERSION) + "." + QString::number(QWT3D_PATCH_VERSION);

    QString producer = QString("QwtPlot3D ") + version + " (beta)";
    const QByteArray utf8Producer = producer.toUtf8();
    const QByteArray utf8Fname = fname.toUtf8();

    FILE *fp = openGl2psOutputFile(fname);
    if (!fp) {
        Label::useDeviceFonts(false);
        cleanupOffscreen();
        return false;
    }
    while (state == GL2PS_OVERFLOW) {
        if (bufsize > 0)
            rewind(fp);
        bufsize += 2 * 1024 * 1024;
        makeActiveCurrent();
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glClearColor(static_cast<GLclampf>(plot->bgcolor_.r),
                     static_cast<GLclampf>(plot->bgcolor_.g),
                     static_cast<GLclampf>(plot->bgcolor_.b),
                     static_cast<GLclampf>(plot->bgcolor_.a));
        gl2psBeginPage("---", utf8Producer.constData(), viewport, gl2ps_format_, sortmode, options,
                       GL_RGBA, 0, NULL, 0, 0, 0, bufsize, fp, utf8Fname.constData());

        plot->paintGL();
        glFlush();
        state = gl2psEndPage();
    }
    fclose(fp);

    // extra TeX file
    if (textmode_ == TEX) {
        QString fn = (texfname_.isEmpty()) ? fname + ".tex" : texfname_;
        const QByteArray utf8TexFn = fn.toUtf8();

        fp = openGl2psOutputFile(fn);
        if (!fp) {
            Label::useDeviceFonts(false);
            cleanupOffscreen();
            return false;
        }
        Label::useDeviceFonts(true);
        options &= ~GL2PS_NO_PIXMAP & ~GL2PS_NO_TEXT;
        state = GL2PS_OVERFLOW;
        bufsize = 0;
        while (state == GL2PS_OVERFLOW) {
            if (bufsize > 0)
                rewind(fp);
            bufsize += 2 * 1024 * 1024;
            makeActiveCurrent();
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            gl2psBeginPage("---", utf8Producer.constData(), viewport, GL2PS_TEX, sortmode, options,
                           GL_RGBA, 0, NULL, 0, 0, 0, bufsize, fp, utf8TexFn.constData());

            plot->paintGL();
            glFlush();
            state = gl2psEndPage();
        }
        fclose(fp);
    }

    Label::useDeviceFonts(false);
    cleanupOffscreen();

    return (state != GL2PS_ERROR && state != GL2PS_UNINITIALIZED);
}

// moved

GLint Qwt3D::setDeviceLineWidth(GLfloat val)
{
    if (val < 0)
        val = 0;

    GLint ret = gl2psLineWidth(val);

    GLfloat lw[2];
    glGetFloatv(GL_LINE_WIDTH_RANGE, lw);

    if (val < lw[0])
        val = lw[0];
    else if (val > lw[1])
        val = lw[1];

    glLineWidth(val);
    return ret;
}

GLint Qwt3D::setDevicePointSize(GLfloat val)
{
    if (val < 0)
        val = 0;

    GLint ret = gl2psPointSize(val);

    GLfloat lw[2];
    glGetFloatv(GL_POINT_SIZE_RANGE, lw);

    if (val < lw[0])
        val = lw[0];
    else if (val > lw[1])
        val = lw[1];

    glPointSize(val);
    return ret;
}

GLint Qwt3D::drawDevicePixels(GLsizei width, GLsizei height, GLenum format, GLenum type,
                              const void *pixels)
{
    glDrawPixels(width, height, format, type, pixels);

    if (format != GL_RGBA || type != GL_UNSIGNED_BYTE)
        return GL2PS_ERROR;

    GLfloat *convertedpixel = (GLfloat *)malloc(3 * width * height * sizeof(GLfloat));
    if (!convertedpixel)
        return GL2PS_ERROR;

    GLubyte *px = (GLubyte *)pixels;
    for (int i = 0; i != 3 * width * height; i += 3) {
        int pxi = (4 * i) / 3;
        convertedpixel[i] = px[pxi] / float(255);
        convertedpixel[i + 1] = px[pxi + 1] / float(255);
        convertedpixel[i + 2] = px[pxi + 2] / float(255);
    }
    GLint ret = gl2psDrawPixels(width, height, 0, 0, GL_RGB, GL_FLOAT, convertedpixel);
    free(convertedpixel);
    return ret;
}

GLint Qwt3D::drawDeviceText(const char *str, const char *fontname, int fontsize, Triple pos,
                            RGBA rgba, ANCHOR align, double gap)
{
    if (!str || !*str)
        return GL2PS_SUCCESS;

    double vp[3];

    World2ViewPort(vp[0], vp[1], vp[2], pos.x, pos.y, pos.z);
    Triple start(vp[0], vp[1], vp[2]);

    GLdouble fcol[4];
    glGetDoublev(GL_CURRENT_COLOR, fcol);
    GLdouble bcol[4];
    glGetDoublev(GL_COLOR_CLEAR_VALUE, bcol);

    const double alpha = (rgba.a > 0.0) ? rgba.a : 1.0;
    glColor4d(rgba.r, rgba.g, rgba.b, alpha);

    GLint ret = GL2PS_SUCCESS;

    GLint a = GL2PS_TEXT_BL;
    switch (align) {
    case Center:
        a = GL2PS_TEXT_C;
        break;
    case CenterLeft:
        a = GL2PS_TEXT_CL;
        start += Triple(gap, 0, 0);
        break;
    case CenterRight:
        a = GL2PS_TEXT_CR;
        start += Triple(-gap, 0, 0);
        break;
    case BottomCenter:
        a = GL2PS_TEXT_B;
        start += Triple(0, gap, 0);
        break;
    case BottomLeft:
        a = GL2PS_TEXT_BL;
        start += Triple(gap, gap, 0);
        break;
    case BottomRight:
        a = GL2PS_TEXT_BR;
        start += Triple(-gap, gap, 0);
        break;
    case TopCenter:
        a = GL2PS_TEXT_T;
        start += Triple(0, -gap, 0);
        break;
    case TopLeft:
        a = GL2PS_TEXT_TL;
        start += Triple(gap, -gap, 0);
        break;
    case TopRight:
        a = GL2PS_TEXT_TR;
        start += Triple(-gap, -gap, 0);
        break;
    default:
        break;
    }

    ViewPort2World(vp[0], vp[1], vp[2], start.x, start.y, start.z);
    Triple adjpos(vp[0], vp[1], vp[2]);

    GL2PSrgba textColor = {
        static_cast<GLfloat>(rgba.r),
        static_cast<GLfloat>(rgba.g),
        static_cast<GLfloat>(rgba.b),
        static_cast<GLfloat>(alpha)
    };

    glRasterPos3d(adjpos.x, adjpos.y, adjpos.z);
    GLboolean rasterValid = GL_TRUE;
    glGetBooleanv(GL_CURRENT_RASTER_POSITION_VALID, &rasterValid);
    if (!rasterValid && std::isfinite(start.x) && std::isfinite(start.y)) {
        GL2PSvertex forcedVert;
        forcedVert.xyz[0] = static_cast<GLfloat>(start.x);
        forcedVert.xyz[1] = static_cast<GLfloat>(start.y);
        forcedVert.xyz[2] =
                static_cast<GLfloat>(std::isfinite(start.z) ? qBound(0.0, start.z, 1.0) : 0.5);
        forcedVert.rgba[0] = textColor[0];
        forcedVert.rgba[1] = textColor[1];
        forcedVert.rgba[2] = textColor[2];
        forcedVert.rgba[3] = textColor[3];
        gl2psForceRasterPos(&forcedVert);
    }

    const GLint fileFormat = gl2psGetFileFormat();
    QByteArray svgEscapedText;
    const char *outStr = str;
    if (fileFormat == GL2PS_SVG) {
        for (const char *p = str; *p != '\0'; ++p) {
            switch (*p) {
            case '&':
                svgEscapedText.append("&amp;");
                break;
            case '<':
                svgEscapedText.append("&lt;");
                break;
            case '>':
                svgEscapedText.append("&gt;");
                break;
            default:
                svgEscapedText.append(*p);
                break;
            }
        }
        outStr = svgEscapedText.constData();
    }

    QByteArray sanitizedFont;
    const char *outFont = (fontname && *fontname) ? fontname : "Arial";
    if (fileFormat == GL2PS_PDF) {
        for (const char *p = outFont; *p != '\0'; ++p) {
            if (*p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
                sanitizedFont.append(*p);
        }
        if (sanitizedFont.isEmpty())
            sanitizedFont = QByteArrayLiteral("Arial");
        outFont = sanitizedFont.constData();
    } else if (fileFormat == GL2PS_SVG) {
        for (const char *p = outFont; *p != '\0'; ++p) {
            switch (*p) {
            case '"':
                sanitizedFont.append("&quot;");
                break;
            case '&':
                sanitizedFont.append("&amp;");
                break;
            case '<':
                sanitizedFont.append("&lt;");
                break;
            case '>':
                sanitizedFont.append("&gt;");
                break;
            default:
                sanitizedFont.append(*p);
                break;
            }
        }
        if (sanitizedFont.isEmpty())
            sanitizedFont = QByteArrayLiteral("Arial");
        outFont = sanitizedFont.constData();
    }

    const GLshort safeSize = static_cast<GLshort>((fontsize > 0) ? fontsize : 12);
    ret = gl2psTextOptColor(outStr, outFont, safeSize, a, 0.0f, textColor);
    glColor4dv(fcol);
    glClearColor(static_cast<GLclampf>(bcol[0]), static_cast<GLclampf>(bcol[1]),
                 static_cast<GLclampf>(bcol[2]), static_cast<GLclampf>(bcol[3]));
    return ret;
}

void Qwt3D::setDevicePolygonOffset(GLfloat factor, GLfloat units)
{
    glPolygonOffset(factor, units);
    gl2psEnable(GL2PS_POLYGON_OFFSET_FILL);
}

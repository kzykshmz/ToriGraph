/***************************************************************************
    File                 : ModernUiTheme.cpp
    Project              : ToriGraph / SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2026 ToriGraph Project
    Description          : Modern Fluent Scientific Theme & Legacy XPM Icon Upgrader
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *  This program is free software; you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation; either version 2 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor,                    *
 *   Boston, MA  02110-1301  USA                                           *
 *                                                                         *
 ***************************************************************************/

#include "ModernUiTheme.h"
#include "IconLoader.h"
#include "Folder.h"
#include "MyWidget.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QFocusEvent>
#include <QFocusFrame>
#include <QPaintEvent>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFontDatabase>
#include <QHash>
#include <QIcon>
#include <QKeySequence>
#include <QLineEdit>
#include <QMdiSubWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QPainter>
#include <QPalette>
#include <QPointer>
#include <QProxyStyle>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QStyleFactory>
#include <QStyleOption>
#include <QStyleOptionComboBox>
#include <QStyleOptionSpinBox>
#include <QStyleOptionTitleBar>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVariant>
#include <QWidget>

namespace {

class ComboSpinArrowPainter : public QObject
{
public:
    explicit ComboSpinArrowPainter(QObject *parent = nullptr) : QObject(parent) { }

    static ComboSpinArrowPainter *instance()
    {
        static QPointer<ComboSpinArrowPainter> s_inst;
        if (!s_inst)
            s_inst = new ComboSpinArrowPainter(QCoreApplication::instance());
        return s_inst.data();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (!event || event->type() != QEvent::Paint)
            return false;
        QWidget *w = qobject_cast<QWidget *>(watched);
        if (!w || !w->isVisible() || d_painting.contains(w))
            return false;

        const auto *cb = qobject_cast<const QComboBox *>(w);
        const auto *sb = cb ? nullptr : qobject_cast<const QAbstractSpinBox *>(w);
        const bool isSpin = sb && (qobject_cast<const QSpinBox *>(w) != nullptr
                                   || qobject_cast<const QDoubleSpinBox *>(w) != nullptr);
        if (!cb && !isSpin)
            return false;

        // 1. Let QStyleSheetStyle paint the styled frame and drop-down/spin buttons first
        d_painting.insert(w);
        QCoreApplication::sendEvent(w, event);
        d_painting.remove(w);

        // 2. Paint crisp antialiased vector chevrons directly over the subcontrol buttons
        QPainter painter(w);
        if (!painter.isActive())
            return true;
        painter.setRenderHint(QPainter::Antialiasing, true);
        const bool enabled = w->isEnabled();
        const QColor col = enabled ? QColor(0x47, 0x55, 0x69) : QColor(0x94, 0xA3, 0xB8);

        if (cb) {
            QStyleOptionComboBox opt;
            opt.initFrom(cb);
            opt.editable = cb->isEditable();
            opt.frame = cb->hasFrame();
            opt.subControls = QStyle::SC_All;
            QRect arrowR = w->style()->subControlRect(QStyle::CC_ComboBox, &opt,
                                                      QStyle::SC_ComboBoxArrow, cb);
            if (!arrowR.isValid() || arrowR.width() < 6 || arrowR.height() < 6)
                arrowR = QRect(w->width() - 20, 1, 19, qMax(6, w->height() - 2));
            const QPointF c = QRectF(arrowR).center();
            const qreal dx = qMin<qreal>(3.6, qMax<qreal>(2.0, (arrowR.width() - 4.0) * 0.35));
            const qreal dy = qMin<qreal>(2.1, qMax<qreal>(1.4, (arrowR.height() - 4.0) * 0.25));
            painter.setPen(QPen(col, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            const QPointF pts[3] = { QPointF(c.x() - dx, c.y() - dy * 0.55),
                                     QPointF(c.x(), c.y() + dy * 0.75),
                                     QPointF(c.x() + dx, c.y() - dy * 0.55) };
            painter.drawPolyline(pts, 3);
        } else if (sb && sb->buttonSymbols() != QAbstractSpinBox::NoButtons) {
            QStyleOptionSpinBox opt;
            opt.initFrom(sb);
            opt.frame = sb->hasFrame();
            opt.buttonSymbols = sb->buttonSymbols();
            opt.subControls = QStyle::SC_All;
            QRect upR = w->style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                                   QStyle::SC_SpinBoxUp, sb);
            QRect dnR = w->style()->subControlRect(QStyle::CC_SpinBox, &opt,
                                                   QStyle::SC_SpinBoxDown, sb);
            if (!upR.isValid() || upR.width() < 6 || !dnR.isValid() || dnR.width() < 6) {
                const int midY = w->height() / 2;
                upR = QRect(w->width() - 18, 1, 17, qMax(6, midY - 1));
                dnR = QRect(w->width() - 18, midY, 17, qMax(6, w->height() - midY - 1));
            }
            painter.setPen(QPen(col, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            {
                const QPointF c = QRectF(upR).center();
                const qreal dx = qMin<qreal>(3.0, qMax<qreal>(2.0, (upR.width() - 4.0) * 0.34));
                const qreal dy = qMin<qreal>(1.6, qMax<qreal>(1.2, (upR.height() - 3.0) * 0.30));
                const QPointF pts[3] = { QPointF(c.x() - dx, c.y() + dy * 0.55),
                                         QPointF(c.x(), c.y() - dy * 0.75),
                                         QPointF(c.x() + dx, c.y() + dy * 0.55) };
                painter.drawPolyline(pts, 3);
            }
            {
                const QPointF c = QRectF(dnR).center();
                const qreal dx = qMin<qreal>(3.0, qMax<qreal>(2.0, (dnR.width() - 4.0) * 0.34));
                const qreal dy = qMin<qreal>(1.6, qMax<qreal>(1.2, (dnR.height() - 3.0) * 0.30));
                const QPointF pts[3] = { QPointF(c.x() - dx, c.y() - dy * 0.55),
                                         QPointF(c.x(), c.y() + dy * 0.75),
                                         QPointF(c.x() + dx, c.y() - dy * 0.55) };
                painter.drawPolyline(pts, 3);
            }
        }
        return true;
    }

private:
    QSet<const QWidget *> d_painting;
};

// H5: visible keyboard focus. Most chrome widgets are styled by style sheets, which do not
// draw a focus rectangle, so a 2 px accent ring (a QFocusFrame, which follows the widget's
// geometry) is shown around the widget that received focus from the keyboard (Tab,
// Shift+Tab, a shortcut). Mouse focus does not show it, so clicking looks as before.
static bool isFocusRingCandidate(const QWidget *w)
{
    return w
            && (qobject_cast<const QAbstractButton *>(w) || qobject_cast<const QComboBox *>(w)
                || qobject_cast<const QTabBar *>(w) || qobject_cast<const QAbstractSlider *>(w));
}

class KeyboardFocusRing : public QFocusFrame
{
public:
    explicit KeyboardFocusRing(QWidget *parent = nullptr) : QFocusFrame(parent)
    {
        setObjectName(QStringLiteral("TgKeyboardFocusRing"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(ModernUiTheme::focusRingColor(), 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0), 5.0, 5.0);
    }
};

class KeyboardFocusTracker : public QObject
{
public:
    static KeyboardFocusTracker *instance()
    {
        static QPointer<KeyboardFocusTracker> s_inst;
        if (!s_inst)
            s_inst = new KeyboardFocusTracker(qApp);
        return s_inst;
    }

    const QWidget *ringTarget() const
    {
        return (d_ring && d_ring->isVisible()) ? d_ring->widget() : nullptr;
    }

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev->type() == QEvent::FocusIn && obj->isWidgetType()) {
            QWidget *w = static_cast<QWidget *>(obj);
            const Qt::FocusReason reason = static_cast<QFocusEvent *>(ev)->reason();
            const bool keyboard = reason == Qt::TabFocusReason
                    || reason == Qt::BacktabFocusReason || reason == Qt::ShortcutFocusReason;
            if (keyboard && isFocusRingCandidate(w) && !w->isWindow()) {
                if (!d_ring)
                    d_ring = new KeyboardFocusRing();
                d_ring->setWidget(w);
                d_ring->show();
                d_ring->raise();
            } else if (d_ring && d_ring->widget() != w) {
                d_ring->setWidget(nullptr);
                d_ring->hide();
            }
        } else if (ev->type() == QEvent::MouseButtonPress && d_ring && d_ring->isVisible()) {
            d_ring->setWidget(nullptr);
            d_ring->hide();
        }
        return QObject::eventFilter(obj, ev);
    }

private:
    explicit KeyboardFocusTracker(QObject *parent) : QObject(parent) { }
    QPointer<KeyboardFocusRing> d_ring;
};

class ModernFluentProxyStyle : public QProxyStyle
{
public:
    explicit ModernFluentProxyStyle(QStyle *baseStyle) : QProxyStyle(baseStyle) { }

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override
    {
        switch (metric) {
        case QStyle::PM_SmallIconSize:
            return 20;
        case QStyle::PM_ToolBarIconSize:
            return 20;
        case QStyle::PM_ButtonIconSize:
            return 18;
        case QStyle::PM_TabBarIconSize:
            return 18;
        case QStyle::PM_ListViewIconSize:
            return 18;
        case QStyle::PM_ToolBarItemSpacing:
            return 3;
        case QStyle::PM_ToolBarFrameWidth:
            return 2;
        case QStyle::PM_MenuHMargin:
            return 4;
        case QStyle::PM_MenuVMargin:
            return 4;
        case QStyle::PM_SplitterWidth:
            return 7;
        case QStyle::PM_MdiSubWindowFrameWidth:
            return 7;
        default:
            return QProxyStyle::pixelMetric(metric, option, widget);
        }
    }

    int styleHint(StyleHint hint, const QStyleOption *option = nullptr,
                  const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override
    {
        switch (hint) {
        case QStyle::SH_EtchDisabledText:
            return 0;
        case QStyle::SH_DitherDisabledText:
            return 0;
        case QStyle::SH_Menu_MouseTracking:
            return 1;
        case QStyle::SH_MenuBar_MouseTracking:
            return 1;
        case QStyle::SH_Menu_SubMenuPopupDelay:
            return 110;
        case QStyle::SH_ToolTip_WakeUpDelay:
            return 320;
        default:
            return QProxyStyle::styleHint(hint, option, widget, returnData);
        }
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override
    {
        // H5: no dotted Windows-95 rectangles, but keyboard focus must stay visible. Buttons,
        // combos, tab bars and sliders get the KeyboardFocusRing; everything else (item views,
        // check boxes in tables, ...) gets a 2 px accent frame when focus came from the
        // keyboard. Mouse focus draws nothing, as before.
        if (element == QStyle::PE_FrameFocusRect) {
            if (!option || !painter || isFocusRingCandidate(widget)
                || !(option->state & QStyle::State_KeyboardFocusChange))
                return;
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(QPen(ModernUiTheme::focusRingColor(), 2.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(QRectF(option->rect).adjusted(1.0, 1.0, -1.0, -1.0), 3.0,
                                     3.0);
            painter->restore();
            return;
        }
        if (element == QStyle::PE_FrameWindow && qobject_cast<const QMdiSubWindow *>(widget)
            && option && painter) {
            const auto *sub = qobject_cast<const QMdiSubWindow *>(widget);
            const bool isActive = (option->state & QStyle::State_Active) != 0;
            const QRect r = option->rect;
            painter->save();
            // Clean light frame band over the 7px resize grab border
            painter->fillRect(r, isActive ? QColor(0xEF, 0xF6, 0xFF) : QColor(0xF1, 0xF5, 0xF9));
            painter->setPen(QPen(isActive ? QColor(0x3B, 0x82, 0xF6) : QColor(0xCB, 0xD5, 0xE1), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(r.adjusted(0, 0, -1, -1));
            // Subtle bottom-right diagonal resize grip when resizable (not maximized)
            if (sub && !sub->isMaximized() && !sub->isMinimized() && r.width() > 40 && r.height() > 40) {
                painter->setRenderHint(QPainter::Antialiasing, true);
                painter->setPen(QPen(isActive ? QColor(0x3B, 0x82, 0xF6, 180)
                                              : QColor(0x94, 0xA3, 0xB8, 170),
                                     1.2));
                const int rx = r.right() - 2;
                const int by = r.bottom() - 2;
                painter->drawLine(QPointF(rx - 4, by), QPointF(rx, by - 4));
                painter->drawLine(QPointF(rx - 8, by), QPointF(rx, by - 8));
                painter->drawLine(QPointF(rx - 12, by), QPointF(rx, by - 12));
            }
            painter->restore();
            return;
        }
        if ((element == QStyle::PE_IndicatorArrowDown || element == QStyle::PE_IndicatorArrowUp
             || element == QStyle::PE_IndicatorSpinUp || element == QStyle::PE_IndicatorSpinDown)
            && option && painter) {
            const bool enabled = (option->state & QStyle::State_Enabled) != 0;
            const bool active = ((option->state & QStyle::State_Sunken) != 0)
                    || ((option->state & QStyle::State_MouseOver) != 0);
            const QColor col = !enabled ? QColor(0x94, 0xA3, 0xB8)
                    : active            ? QColor(0x1D, 0x4E, 0xD8)
                                        : QColor(0x47, 0x55, 0x69);
            const QRectF r = QRectF(option->rect);
            const QPointF c = r.center();
            const bool isSpin = (element == QStyle::PE_IndicatorSpinUp
                                 || element == QStyle::PE_IndicatorSpinDown);
            const bool isUp = (element == QStyle::PE_IndicatorArrowUp
                               || element == QStyle::PE_IndicatorSpinUp);
            const qreal maxDx = qMax<qreal>(2.0, (r.width() - 2.0) * 0.42);
            const qreal maxDy = qMax<qreal>(1.2, (r.height() - 2.0) * 0.36);
            const qreal dx = qMin<qreal>(isSpin ? 3.0 : 3.6, maxDx);
            const qreal dy = qMin<qreal>(isSpin ? 1.6 : 2.1, maxDy);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(QPen(col, isSpin ? 1.4 : 1.5, Qt::SolidLine, Qt::RoundCap,
                                 Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
            if (isUp) {
                const QPointF pts[3] = { QPointF(c.x() - dx, c.y() + dy * 0.55),
                                         QPointF(c.x(), c.y() - dy * 0.75),
                                         QPointF(c.x() + dx, c.y() + dy * 0.55) };
                painter->drawPolyline(pts, 3);
            } else {
                const QPointF pts[3] = { QPointF(c.x() - dx, c.y() - dy * 0.55),
                                         QPointF(c.x(), c.y() + dy * 0.75),
                                         QPointF(c.x() + dx, c.y() - dy * 0.55) };
                painter->drawPolyline(pts, 3);
            }
            painter->restore();
            return;
        }
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                            QPainter *painter, const QWidget *widget = nullptr) const override
    {
        if (control == QStyle::CC_TitleBar && option && painter) {
            if (const auto *tb = qstyleoption_cast<const QStyleOptionTitleBar *>(option)) {
                const bool isActive = (tb->titleBarState & QStyle::State_Active) != 0;
                const QColor bg = isActive ? QColor(0xEF, 0xF6, 0xFF) : QColor(0xF1, 0xF5, 0xF9);
                const QColor fg = isActive ? QColor(0x0F, 0x17, 0x2A) : QColor(0x47, 0x55, 0x69);
                const QColor glyphCol = isActive ? QColor(0x1E, 0x29, 0x3B) : QColor(0x47, 0x55, 0x69);
                const QColor hoverBg = isActive ? QColor(0xDB, 0xEA, 0xFE) : QColor(0xE2, 0xE8, 0xF0);
                const QColor pressBg = isActive ? QColor(0xBF, 0xDB, 0xFE) : QColor(0xCB, 0xD5, 0xE1);

                painter->save();
                // 1. Clean modern light header background + top active accent + bottom hairline
                painter->fillRect(tb->rect, bg);
                if (isActive) {
                    painter->fillRect(QRect(tb->rect.left(), tb->rect.top(), tb->rect.width(), 2),
                                      QColor(0x3B, 0x82, 0xF6));
                }
                painter->setPen(QPen(isActive ? QColor(0xBF, 0xDB, 0xFE) : QColor(0xE2, 0xE8, 0xF0), 1));
                painter->drawLine(tb->rect.bottomLeft(), tb->rect.bottomRight());

                // 2. Window icon
                if ((tb->subControls & QStyle::SC_TitleBarSysMenu) && !tb->icon.isNull()) {
                    const QRect iconRect =
                            subControlRect(QStyle::CC_TitleBar, tb, QStyle::SC_TitleBarSysMenu, widget);
                    if (iconRect.isValid()) {
                        const int sz = qMin(16, qMin(iconRect.width(), iconRect.height()));
                        const QRect dst(iconRect.center().x() - sz / 2,
                                        iconRect.center().y() - sz / 2, sz, sz);
                        tb->icon.paint(painter, dst, Qt::AlignCenter,
                                       isActive ? QIcon::Normal : QIcon::Disabled);
                    }
                }

                // 3. Elided title text
                if (tb->subControls & QStyle::SC_TitleBarLabel) {
                    const QRect lblRect =
                            subControlRect(QStyle::CC_TitleBar, tb, QStyle::SC_TitleBarLabel, widget);
                    if (lblRect.isValid()) {
                        QFont tf = painter->font();
                        tf.setWeight(isActive ? QFont::DemiBold : QFont::Normal);
                        painter->setFont(tf);
                        painter->setPen(fg);
                        const QString elided =
                                painter->fontMetrics().elidedText(tb->text, Qt::ElideRight,
                                                                  qMax(0, lblRect.width() - 4));
                        painter->drawText(lblRect.adjusted(2, 0, -2, 0),
                                          Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
                                          elided);
                    }
                }

                // 4. Crisp Fluent titlebar buttons (-, □, ❐, ×)
                auto drawTitleBtn = [&](QStyle::SubControl sc) {
                    if (!(tb->subControls & sc))
                        return;
                    const QRect r = subControlRect(QStyle::CC_TitleBar, tb, sc, widget);
                    if (!r.isValid() || r.isEmpty())
                        return;

                    const bool isClose = (sc == QStyle::SC_TitleBarCloseButton);
                    const bool isHover = (tb->activeSubControls & sc)
                            && (tb->state & QStyle::State_MouseOver);
                    const bool isSunk = (tb->activeSubControls & sc)
                            && (tb->state & QStyle::State_Sunken);

                    const QRect bgRect = r.adjusted(1, 2, -1, -2);
                    if (isClose && (isSunk || isHover)) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(isSunk ? QColor(0xDC, 0x26, 0x26)
                                                 : QColor(0xEF, 0x44, 0x44));
                        painter->drawRoundedRect(bgRect, 4, 4);
                    } else if (isSunk || isHover) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(isSunk ? pressBg : hoverBg);
                        painter->drawRoundedRect(bgRect, 4, 4);
                    }

                    const QColor stroke = (isClose && (isSunk || isHover)) ? QColor(Qt::white)
                                                                           : glyphCol;
                    painter->setRenderHint(QPainter::Antialiasing, true);
                    painter->setPen(QPen(stroke, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                    painter->setBrush(Qt::NoBrush);

                    const QPointF c = QRectF(r).center();
                    if (sc == QStyle::SC_TitleBarMinButton) {
                        painter->drawLine(QPointF(c.x() - 4.0, c.y() + 1.0),
                                          QPointF(c.x() + 4.0, c.y() + 1.0));
                    } else if (sc == QStyle::SC_TitleBarMaxButton) {
                        painter->drawRect(QRectF(c.x() - 4.0, c.y() - 4.0, 8.0, 8.0));
                    } else if (sc == QStyle::SC_TitleBarNormalButton) {
                        painter->drawRect(QRectF(c.x() - 4.5, c.y() - 2.0, 6.5, 6.5));
                        painter->drawLine(QPointF(c.x() - 2.0, c.y() - 4.5),
                                          QPointF(c.x() + 4.5, c.y() - 4.5));
                        painter->drawLine(QPointF(c.x() + 4.5, c.y() - 4.5),
                                          QPointF(c.x() + 4.5, c.y() + 2.0));
                    } else if (sc == QStyle::SC_TitleBarCloseButton) {
                        painter->drawLine(QPointF(c.x() - 3.8, c.y() - 3.8),
                                          QPointF(c.x() + 3.8, c.y() + 3.8));
                        painter->drawLine(QPointF(c.x() + 3.8, c.y() - 3.8),
                                          QPointF(c.x() - 3.8, c.y() + 3.8));
                    } else if (sc == QStyle::SC_TitleBarShadeButton) {
                        painter->drawLine(QPointF(c.x() - 3.5, c.y() + 1.5),
                                          QPointF(c.x(), c.y() - 2.0));
                        painter->drawLine(QPointF(c.x(), c.y() - 2.0),
                                          QPointF(c.x() + 3.5, c.y() + 1.5));
                    } else if (sc == QStyle::SC_TitleBarUnshadeButton) {
                        painter->drawLine(QPointF(c.x() - 3.5, c.y() - 1.5),
                                          QPointF(c.x(), c.y() + 2.0));
                        painter->drawLine(QPointF(c.x(), c.y() + 2.0),
                                          QPointF(c.x() + 3.5, c.y() - 1.5));
                    }
                    painter->setRenderHint(QPainter::Antialiasing, false);
                };

                drawTitleBtn(QStyle::SC_TitleBarMinButton);
                drawTitleBtn(QStyle::SC_TitleBarMaxButton);
                drawTitleBtn(QStyle::SC_TitleBarNormalButton);
                drawTitleBtn(QStyle::SC_TitleBarShadeButton);
                drawTitleBtn(QStyle::SC_TitleBarUnshadeButton);
                drawTitleBtn(QStyle::SC_TitleBarCloseButton);

                painter->restore();
                return;
            }
        }
        QProxyStyle::drawComplexControl(control, option, painter, widget);
    }

    void polish(QWidget *widget) override
    {
        QProxyStyle::polish(widget);
        if (!widget)
            return;
        if (qobject_cast<QPushButton *>(widget) || qobject_cast<QToolButton *>(widget)
            || qobject_cast<QComboBox *>(widget) || qobject_cast<QLineEdit *>(widget)
            || qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QTabBar *>(widget)) {
            widget->setAttribute(Qt::WA_Hover, true);
        }
        // H5: icon-only buttons have no text for screen readers; use the tooltip.
        if (auto *ab = qobject_cast<QAbstractButton *>(widget)) {
            if (ab->text().trimmed().isEmpty() && ab->accessibleName().isEmpty()
                && !ab->toolTip().isEmpty()) {
                QString name = ab->toolTip();
                name.remove(QRegularExpression(QStringLiteral("<[^>]*>")));
                ab->setAccessibleName(name.simplified());
            }
        }
        if ((qobject_cast<QComboBox *>(widget) || qobject_cast<QSpinBox *>(widget)
             || qobject_cast<QDoubleSpinBox *>(widget))
            && !widget->property("_tg_arrow_overlay").toBool()) {
            widget->setProperty("_tg_arrow_overlay", true);
            widget->installEventFilter(ComboSpinArrowPainter::instance());
        }
    }
};

static QString cleanLabelText(const QString &raw)
{
    QString s = raw;
    // Strip trailing "(...)" mnemonic hints such as "新規作成(&N)" -> "新規作成" only after keeping copy
    s.replace(QStringLiteral("&&"), QStringLiteral("\x01"));
    s.remove(QLatin1Char('&'));
    s.replace(QLatin1Char('\x01'), QLatin1Char('&'));
    while (s.endsWith(QStringLiteral("...")) || s.endsWith(QString::fromUtf8("…"))) {
        if (s.endsWith(QStringLiteral("...")))
            s.chop(3);
        else
            s.chop(1);
        s = s.trimmed();
    }
    return s.trimmed().toLower();
}

static QString stripJapaneseMnemonicSuffix(const QString &cleanLower)
{
    // Strip trailing "(x)" mnemonic markers common in Japanese menu translations
    static const QRegularExpression reMnemonic(QStringLiteral("\\s*\\([a-z0-9]\\)\\s*$"));
    QString out = cleanLower;
    out.remove(reMnemonic);
    return out.trimmed();
}

struct ExactMapEntry {
    const char *key;
    const char *semanticId;
};

static const ExactMapEntry kExactObjectNameMap[] = {
    { "actionnewproject", "file-new" },
    { "actionopen", "file-open" },
    { "actionsaveproject", "file-save" },
    { "actionsaveprojectas", "file-save-as" },
    { "actionopentemplate", "file-open" },
    { "actionsavetemplate", "file-save-as" },
    { "actionload", "import-csv" },
    { "actionimportopj", "import-opj" },
    { "actionexportgraph", "export-image" },
    { "actionexportallgraphs", "export-image" },
    { "actionexportpdf", "export-pdf" },
    { "actioncopygraphole", "copy-ole" },
    { "actioncopypageimage", "copy-page-image" },
    { "actionprint", "print" },
    { "actionprintallplots", "print" },
    { "actionprintwindow", "print" },
    { "actionundo", "undo" },
    { "actionredo", "redo" },
    { "actioncutselection", "cut" },
    { "actioncopyselection", "copy" },
    { "actionpasteselection", "paste" },
    { "actionclearselection", "delete" },
    { "actionremovecurve", "delete" },
    { "actiondeletelayer", "delete" },
    { "actioncopywindow", "duplicate" },
    { "actionshowconfiguredialog", "settings" },
    { "actioncheckupdates", "update-check" },
    { "actionshowhelp", "help" },
    { "actionabout", "help" },
    { "actionshowexplorer", "explorer" },
    { "actionshowlog", "log-window" },
    { "actionshowconsole", "python-console" },
    { "actionpythonconsole", "python-console" },
    { "actionopenpythonconsole", "python-console" },
    { "actionrunpythonscript", "python-run" },
    { "actionnoteexecute", "python-run" },
    { "actionnoteexecuteall", "python-run" },
    { "actionnoteevaluate", "python-run" },
    { "actiontabbedworkspaceview", "tab-view" },
    { "actionworkspacelayouttabbed", "tab-view" },
    { "actionworkspacelayoutsplit", "split-view" },
    { "actionworkspacelayoutmdi", "mdi-view" },
    { "explorertabbedviewbutton", "tab-view" },
    { "actionfocushud", "focus-hud" },
    { "actionfocusmode", "focus-hud" },
    { "actiontogglefocushudmode", "focus-hud" },
    { "actionexitfocushud", "exit-focus-hud" },
    { "actiontabledrawer", "table-drawer" },
    { "actioncommandpalette", "search-command" },
    { "actionnewtable", "new-table" },
    { "actionnewmatrix", "new-matrix" },
    { "actionnewgraph", "new-graph" },
    { "actionnewnote", "new-note" },
    { "actionnewfolder", "new-folder" },
    { "actionnewfunctionplot", "function-plot" },
    { "actionnewsurfaceplot", "plot-3d-surface" },
    { "actionshowcolstatistics", "col-stats" },
    { "actionshowrowstatistics", "row-stats" },
    { "actionrecalcauto", "recalc-auto" },
    { "actionrecalculatenow", "recalc-now" },
    { "actionplotl", "plot-line" },
    { "actionplotp", "plot-scatter" },
    { "actionplotlp", "plot-line-symbol" },
    { "actionplotspline", "plot-spline" },
    { "actionplotverticaldroplines", "plot-droplines" },
    { "actionplotvertsteps", "plot-steps" },
    { "actionplothorsteps", "plot-steps" },
    { "actionplotverticalbars", "plot-bars" },
    { "actionplothorizontalbars", "plot-hbar" },
    { "actionplotarea", "plot-area" },
    { "actionplotpie", "plot-pie" },
    { "actionboxplot", "plot-box" },
    { "actionplotviolin", "plot-violin" },
    { "actionplothistogram", "plot-histogram" },
    { "actionplotstackedhistograms", "plot-histogram" },
    { "actionplotpareto", "plot-pareto" },
    { "actionplotqq", "plot-qq" },
    { "actionplotdoubley", "plot-double-y" },
    { "actionplotyoffsetstack", "plot-waterfall" },
    { "actionwaterfallstackoffset", "plot-waterfall" },
    { "actionplotvectxyxy", "plot-vectors" },
    { "actionplotvectxyam", "plot-vectors" },
    { "actionplotternary", "plot-ternary" },
    { "actioncolormap", "plot-heatmap" },
    { "actiongraymap", "plot-heatmap" },
    { "actioncontourmap", "plot-contour" },
    { "actionplot3dwireframe", "plot-3d-wireframe" },
    { "actionplot3dhiddenline", "plot-3d-wireframe" },
    { "actionplot3dpolygons", "plot-3d-surface" },
    { "actionplot3dwiresurface", "plot-3d-surface" },
    { "actionplot3dbars", "plot-3d-bars" },
    { "actionplot3dscatter", "plot-3d-scatter" },
    { "actionplot3dribbon", "plot-3d-ribbon" },
    { "actionplot3dtrajectory", "plot-3d-ribbon" },
    { "actionpeakanalyzerdialog", "peak-analyzer" },
    { "actionpeakfitdialog", "peak-analyzer" },
    { "actionmultipeakgauss", "peak-gauss" },
    { "actionmultipeakgaussamp", "peak-gauss" },
    { "actionautopeakgauss", "peak-gauss" },
    { "actionfitgauss", "peak-gauss" },
    { "actionmultipeaklorentz", "peak-lorentz" },
    { "actionautopeaklorentz", "peak-lorentz" },
    { "actionfitlorentz", "peak-lorentz" },
    { "actionmultipeakpseudovoigt", "peak-voigt" },
    { "actionautopeakpseudovoigt", "peak-voigt" },
    { "actionmultipeakpearsonvii", "peak-voigt" },
    { "actionbatchpeakanalysis", "peak-batch" },
    { "action2dpeakanalysis", "peak-2d" },
    { "actionfitlinear", "fit-linear" },
    { "actionfitapparentlinear", "fit-linear" },
    { "actionrepeatlinearfitall", "fit-linear" },
    { "actionfityorklinear", "fit-york" },
    { "actionshowfitpolynomdialog", "fit-poly" },
    { "actionshowfitdialog", "fit-nonlinear" },
    { "actionglobalcurvefit", "fit-global" },
    { "actionpiecewiselinearfit", "fit-piecewise" },
    { "actionrobustlinearfit", "fit-robust" },
    { "actionimplicitodrfit", "fit-odr" },
    { "actionlogisticregression", "fit-logistic" },
    { "actionmultiplelinearregression", "fit-mlr" },
    { "actionsurfacefit3d", "fit-surface-3d" },
    { "actionmodelcomparison", "fit-compare" },
    { "actioncomparedatasetsfit", "fit-compare" },
    { "btnpointer", "pointer" },
    { "btnzoomin", "zoom-in" },
    { "btnzoomout", "zoom-out" },
    { "actionunzoom", "zoom-auto" },
    { "btncursor", "data-reader" },
    { "btnpicker", "screen-reader" },
    { "btnselect", "range-selector" },
    { "btnmovepoints", "move-points" },
    { "btnremovepoints", "remove-points" },
    { "btnmaskpoints", "mask-points" },
    { "actionmaskregionroi", "mask-points" },
    { "btnunmaskpoints", "unmask-points" },
    { "actionunmaskregionroi", "unmask-points" },
    { "actionclearallmasks", "clear-masks" },
    { "actiongadgetregionstats", "gadget-roi" },
    { "actiongadgetverticalcursor", "gadget-cursor" },
    { "actiongadgetdeltacursor", "gadget-delta" },
    { "actiongadgetquickfit", "gadget-quick-fit" },
    { "actiongadgetquicksigmoidal", "gadget-quick-fit" },
    { "actiongadgetquickpeaks", "gadget-quick-peaks" },
    { "actiongadgetintegrate", "gadget-integrate" },
    { "actiongadgetintegrate2d", "gadget-integrate" },
    { "actiongadgetdifferentiate", "gadget-diff" },
    { "actiongadgetintersect", "gadget-intersect" },
    { "actiongadgetfft", "gadget-fft" },
    { "actiongadgetrisetime", "gadget-risetime" },
    { "actiongadgetimageprofile", "gadget-profile-2d" },
    { "actionpixellineprofile", "gadget-profile-2d" },
    { "actiongadgetcluster", "gadget-cluster" },
    { "actiongadgetclear", "gadget-clear" },
    { "actionsmoothsavgol", "signal-savgol" },
    { "actionsavgolsmoothderivative", "signal-savgol" },
    { "actionsmoothaverage", "signal-smooth" },
    { "actionsmoothmedian", "signal-smooth" },
    { "actionsmoothlowess", "signal-lowess" },
    { "actionsmoothwhittaker", "signal-whittaker" },
    { "actionfft", "signal-fft" },
    { "actionsmoothfft", "signal-filter" },
    { "actionlowpassfilter", "signal-filter" },
    { "actionhighpassfilter", "signal-filter" },
    { "actionbandpassfilter", "signal-filter" },
    { "actionbandblockfilter", "signal-filter" },
    { "actioniirdigitalfilter", "signal-iir" },
    { "actionstftspectrogram", "signal-stft" },
    { "actionwavelettransform", "signal-wavelet" },
    { "actionwelchpsdcoherence", "signal-psd" },
    { "actionhilbertenvelope", "signal-envelope" },
    { "actionpeakenvelope", "signal-envelope" },
    { "actiondecimateresample", "signal-decimate" },
    { "actionfourierselfdeconvolve", "signal-fsd" },
    { "actionmatrixfft2d", "matrix-fft2d" },
    { "actionmatrixifft2d", "matrix-fft2d" },
    { "actionmatrixfilter2d", "matrix-filter2d" },
    { "actionsimplecurvemath", "curvemath" },
    { "actionaveragemultiplecurves", "curve-average" },
    { "actionnormalizecurves", "curve-normalize" },
    { "actionrepeatnormalizeall", "curve-normalize" },
    { "actionfillbetweencurves", "curve-fill" },
    { "actionarrheniusplot", "arrhenius" },
    { "actiontaucplot", "tauc" },
    { "actioneisanalyzer", "eis-nyquist" },
    { "actiongroupcomparison", "stats-anova" },
    { "actiontwowayanova", "stats-twoway" },
    { "actionnonparametrictests", "stats-nonparam" },
    { "actionnormalitytest", "stats-normality" },
    { "actionoutliertest", "stats-outlier" },
    { "actionpairedcomparisonplot", "stats-paired" },
    { "actionblandaltman", "stats-bland-altman" },
    { "actionpassingbablokdeming", "stats-bablok" },
    { "actionweibullplot", "stats-weibull" },
    { "actionkaplanmeier", "stats-survival" },
    { "actionroccurve", "stats-roc" },
    { "actionpcabiplot", "stats-pca" },
    { "actioncorrelationmatrix", "stats-corr" },
    { "actionclustering", "stats-cluster" },
    { "actionconfidenceellipse", "stats-ellipse" },
    { "actiongraphdigitizer", "digitizer" },
    { "actionstyleisij", "preset-isij" },
    { "actionstyleslide", "preset-slide" },
    { "actionstylenature", "preset-nature" },
    { "actioncopygraphformat", "format-copy" },
    { "actionpastegraphformat", "format-paste" },
    { "actionmergegraphs", "split-view" },
    { "actionbatchplot", "batch-plot" },
    { "actionpagesetup", "page-setup" },
    { "actionfitpagetolayers", "fit-page" },
    { "actionfitlayerstopage", "fit-page" },
    { "actionfitframe", "fit-page" },
    { "actionaddlayer", "layer-add" },
    { "actionaddlinkedlayer", "layer-linked" },
    { "actionaddinsetlayer", "layer-inset" },
    { "actionshowlayerdialog", "layer-arrange" },
    { "actionautomaticlayout", "layer-arrange" },
    { "actionframeboxinward", "frame-box-inward" },
    { "actionframeboxplain", "frame-box-plain" },
    { "actionframeopenl", "frame-open-l" },
    { "actionframezerocrossed", "frame-zero" },
    { "actionshowscaledialog", "axis-scale" },
    { "actionshowaxisdialog", "axis-ticks-in" },
    { "actionshowgriddialog", "axis-grid" },
    { "actioncolorpaletteeditor", "color-palette" },
    { "actionnewlegend", "legend" },
    { "actionaddtext", "text-label" },
    { "actionshowtitledialog", "text-label" },
    { "btnarrow", "line-arrow" },
    { "btnline", "plot-line" },
    { "actionadderrorbars", "error-bars" },
    { "locktoolbar", "lock" },
    { "actionnextwindow", "go-next" },
    { "actionprevwindow", "go-previous" },
    { "actionclosewindow", "close" },
    { "actioncloseallwindows", "close" }
};

static QString matchByObjectName(const QString &objName)
{
    if (objName.isEmpty())
        return QString();

    const QString clean = objName.trimmed();
    const QStringList registered = IconLoader::registeredIconIds();
    if (registered.contains(clean))
        return clean;

    QString normalized = clean.toLower();
    normalized.remove(QLatin1Char('_'));
    normalized.remove(QLatin1Char('-'));

    static QHash<QString, QString> table;
    if (table.isEmpty()) {
        const int n = static_cast<int>(sizeof(kExactObjectNameMap) / sizeof(kExactObjectNameMap[0]));
        table.reserve(n * 2);
        for (int i = 0; i < n; ++i) {
            table.insert(QString::fromLatin1(kExactObjectNameMap[i].key),
                         QString::fromLatin1(kExactObjectNameMap[i].semanticId));
        }
    }
    const auto hit = table.constFind(normalized);
    if (hit != table.cend())
        return hit.value();

    // Check direct alias resolution if objectName uses hyphen/underscore semantic form
    QString dashed = clean.toLower();
    dashed.replace(QLatin1Char('_'), QLatin1Char('-'));
    if (registered.contains(dashed))
        return dashed;

    return QString();
}

static QString matchByTextAndToolTip(const QString &rawText, const QString &rawToolTip,
                                     const QString &shortcutStr)
{
    const QString text = stripJapaneseMnemonicSuffix(cleanLabelText(rawText));
    const QString tip = stripJapaneseMnemonicSuffix(cleanLabelText(rawToolTip));
    const QString combined = text + QLatin1Char(' ') + tip;
    const QString sc = shortcutStr.trimmed().toLower();

    auto has = [&](const char *utf8Sub) -> bool {
        return combined.contains(QString::fromUtf8(utf8Sub));
    };
    auto textIs = [&](const char *utf8Exact) -> bool {
        return text == QString::fromUtf8(utf8Exact);
    };
    auto tipIs = [&](const char *utf8Exact) -> bool {
        return tip == QString::fromUtf8(utf8Exact);
    };

    // 1. High-specificity scientific / ToriGraph Phase 1-6 actions first
    if (has("copy graph as ole") || has("ole object") || has("oleオブジェクト") || has("ole オブジェクト"))
        return QStringLiteral("copy-ole");
    if (has("copy page as image") || has("high-dpi png") || has("300 dpi png")
        || has("画像としてページをコピー") || has("ページを画像としてコピー"))
        return QStringLiteral("copy-page-image");
    if (has("copy format") || has("書式をコピー") || has("フォーマットをコピー")
        || sc == QStringLiteral("ctrl+shift+c"))
        return QStringLiteral("format-copy");
    if (has("paste format") || has("書式を貼り付け") || has("フォーマットを貼り付け")
        || sc == QStringLiteral("ctrl+shift+v"))
        return QStringLiteral("format-paste");
    if (has("batch plotting") || has("batch-clone") || has("一括プロット") || has("バッチプロット"))
        return QStringLiteral("batch-plot");

    // Style & Frame Presets
    if (has("isij"))
        return QStringLiteral("preset-isij");
    if (has("presentation slide") || has("28 pt arial") || has("プレゼンスライド") || has("発表スライド"))
        return QStringLiteral("preset-slide");
    if (has("nature / science") || has("89 mm") || has("nature/science"))
        return QStringLiteral("preset-nature");
    if (has("box frame (inward") || has("内向き目盛") || (has("box frame") && has("inward")))
        return QStringLiteral("frame-box-inward");
    if (has("plain top/right") || has("ボックス枠 (上・右目盛なし)") || has("目盛なし"))
        return QStringLiteral("frame-box-plain");
    if (has("open l-frame") || has("l字枠") || has("オープンl"))
        return QStringLiteral("frame-open-l");
    if (has("zero-crossed") || has("原点交差"))
        return QStringLiteral("frame-zero");

    // Focus Mode, Ribbon, Command Palette, Updater, Language
    if (has("exit focus") || has("フォーカスモード終了") || has("全画面終了"))
        return QStringLiteral("exit-focus-hud");
    if (has("focus hud") || has("focus mode") || has("focus canvas") || has("フォーカスhud")
        || has("フォーカスモード") || sc == QStringLiteral("f11"))
        return QStringLiteral("focus-hud");
    if (has("worksheet drawer") || has("table drawer") || has("データドロワー") || has("テーブルドロワー"))
        return QStringLiteral("table-drawer");
    if (has("command palette") || has("search command") || has("コマンド検索") || has("コマンドパレット"))
        return QStringLiteral("search-command");
    if (has("search for updates") || has("check for updates") || has("アップデートを確認")
        || has("更新を確認") || has("ソフトウェア更新"))
        return QStringLiteral("update-check");
    if (has("tabbed workspace") || has("tabbed view") || has("タブ表示") || has("タブワークスペース"))
        return QStringLiteral("tab-view");
    if (has("split view") || has("分割表示"))
        return QStringLiteral("split-view");
    if (has("free mdi") || has("mdi windows") || has("mdi view") || has("subwindow")
        || has("フリーmdi") || has("mdiウィンドウ") || has("サブウィンドウ"))
        return QStringLiteral("mdi-view");
    if (textIs("language") || textIs("言語") || has("switch language") || has("言語切替"))
        return QStringLiteral("language-ja-en");

    // Origin OPJ / Digitizer / MatSci
    if (has(".opj") || has("origin project") || has("origin プロジェクト") || has("opj"))
        return QStringLiteral("import-opj");
    if (has("digitizer") || has("デジタイザ") || has("画像からデータ抽出"))
        return QStringLiteral("digitizer");
    if (has("arrhenius") || has("アレニウス"))
        return QStringLiteral("arrhenius");
    if (has("tauc") || has("タウク") || has("光学バンドギャップ"))
        return QStringLiteral("tauc");
    if (has("eis nyquist") || has("nyquist") || has("インピーダンス"))
        return QStringLiteral("eis-nyquist");

    // Interactive Gadgets & Data Masks
    if (has("delta cursor") || has("c1-c2") || has("デルタカーソル"))
        return QStringLiteral("gadget-delta");
    if (has("vertical cursor") || has("垂直カーソル"))
        return QStringLiteral("gadget-cursor");
    if (has("cross-section profile") || has("pixel line profile") || has("断面プロファイル")
        || has("ピクセルライン"))
        return QStringLiteral("gadget-profile-2d");
    if (has("region statistics") || has("領域統計"))
        return QStringLiteral("gadget-roi");
    if (has("quick fit") || has("quick sigmoidal") || has("クイックフィット")
        || has("クイックシグモイド"))
        return QStringLiteral("gadget-quick-fit");
    if (has("quick peaks") || has("クイックピーク"))
        return QStringLiteral("gadget-quick-peaks");
    if (has("integrate (roi)") || has("2d integration gadget") || has("積分 (roi)")
        || has("2d積分ガジェット"))
        return QStringLiteral("gadget-integrate");
    if (has("differentiate gadget") || has("微分ガジェット"))
        return QStringLiteral("gadget-diff");
    if (has("intersect gadget") || has("curve intersections") || has("交点ガジェット")
        || has("曲線の交点"))
        return QStringLiteral("gadget-intersect");
    if (has("fft preview (roi)") || has("fftプレビュー"))
        return QStringLiteral("gadget-fft");
    if (has("rise time") || has("立ち上がり時間"))
        return QStringLiteral("gadget-risetime");
    if (has("cluster selector") || has("クラスタ選択"))
        return QStringLiteral("gadget-cluster");
    if (has("clear active gadget") || has("ガジェットをクリア") || has("ガジェット解除"))
        return QStringLiteral("gadget-clear");

    if (has("clear all masks") || textIs("clear masks") || has("マスクをすべて解除")
        || has("マスク解除 (全"))
        return QStringLiteral("clear-masks");
    if (has("unmask") || has("マスク解除"))
        return QStringLiteral("unmask-points");
    if (has("mask data points") || has("mask region") || textIs("mask") || has("データ点をマスク")
        || has("領域マスク") || textIs("マスク"))
        return QStringLiteral("mask-points");

    // Graph Interactive Cursor / Zoom Tools
    if (tipIs("pointer") || has("disable tools") || has("ツール解除") || textIs("ポインタ"))
        return QStringLiteral("pointer");
    if (has("zoom in") || has("ズームイン") || has("拡大"))
        return QStringLiteral("zoom-in");
    if (has("zoom out") || has("ズームアウト") || has("縮小"))
        return QStringLiteral("zoom-out");
    if (has("rescale to show all") || tipIs("best fit") || has("全体を表示") || has("自動スケール"))
        return QStringLiteral("zoom-auto");
    if (has("data reader") || has("データリーダー"))
        return QStringLiteral("data-reader");
    if (has("screen reader") || has("スクリーンリーダー"))
        return QStringLiteral("screen-reader");
    if (has("select data range") || has("データ範囲選択"))
        return QStringLiteral("range-selector");
    if (has("move data points") || has("データ点の移動"))
        return QStringLiteral("move-points");
    if (has("remove bad data") || has("remove data points") || has("不良データ点の除去")
        || has("データ点の削除"))
        return QStringLiteral("remove-points");

    // Peaks & Curve Fitting
    if (has("peak analyzer") || has("peak / multi-peak") || has("ピークアナライザ")
        || has("マルチピーク"))
        return QStringLiteral("peak-analyzer");
    if (has("batch peak") || has("一括ピーク"))
        return QStringLiteral("peak-batch");
    if (has("2d peak") || has("2dピーク"))
        return QStringLiteral("peak-2d");
    if (has("pseudo-voigt") || has("pearson vii") || has("擬フォークト") || has("voigt"))
        return QStringLiteral("peak-voigt");
    if (has("lorentzian") || has("ローレンツ"))
        return QStringLiteral("peak-lorentz");
    if (has("gaussian") || has("ガウス"))
        return QStringLiteral("peak-gauss");

    if (has("york") || has("両軸誤差"))
        return QStringLiteral("fit-york");
    if (has("global curve fit") || has("グローバルフィット") || has("共有パラメータ"))
        return QStringLiteral("fit-global");
    if (has("piecewise") || has("折れ線回帰") || has("区分線形"))
        return QStringLiteral("fit-piecewise");
    if (has("robust") || has("quantile linear") || has("ロバスト"))
        return QStringLiteral("fit-robust");
    if (has("implicit / odr") || has("odr") || has("陰関数"))
        return QStringLiteral("fit-odr");
    if (has("logistic regression") || has("ロジスティック回帰"))
        return QStringLiteral("fit-logistic");
    if (has("multiple linear regression") || has("(mlr)") || has("重回帰"))
        return QStringLiteral("fit-mlr");
    if (has("3d surface fit") || has("3d曲面フィット"))
        return QStringLiteral("fit-surface-3d");
    if (has("compare & rank fit") || has("compare datasets") || has("aic/bic")
        || has("モデル比較") || has("データセット比較"))
        return QStringLiteral("fit-compare");
    if (has("fit polynomial") || has("多項式フィット"))
        return QStringLiteral("fit-poly");
    if (has("fit linear") || has("apparent linear fit") || has("repeat linear fit")
        || has("線形フィット") || has("直線フィット"))
        return QStringLiteral("fit-linear");
    if (has("fit wizard") || has("boltzmann") || has("exponential growth") || has("first order")
        || has("second order") || has("third order") || has("フィットウィザード")
        || has("指数減衰") || has("シグモイド"))
        return QStringLiteral("fit-nonlinear");

    // Signal Processing & Matrix 2D
    if (has("savitzky-golay") || has("サビツキー"))
        return QStringLiteral("signal-savgol");
    if (has("lowess"))
        return QStringLiteral("signal-lowess");
    if (has("whittaker"))
        return QStringLiteral("signal-whittaker");
    if (has("iir digital filter") || has("butterworth") || has("iirデジタル"))
        return QStringLiteral("signal-iir");
    if (has("short-time fourier") || has("(stft)") || has("短時間フーリエ"))
        return QStringLiteral("signal-stft");
    if (has("wavelet") || has("ウェーブレット"))
        return QStringLiteral("signal-wavelet");
    if (has("welch psd") || has("cross-coherence") || has("パワースペクトル密度"))
        return QStringLiteral("signal-psd");
    if (has("hilbert") || has("peak envelope") || has("包絡線"))
        return QStringLiteral("signal-envelope");
    if (has("decimate") || has("anti-alias resample") || has("間引き") || has("リサンプリング"))
        return QStringLiteral("signal-decimate");
    if (has("fourier self-deconvolution") || has("自己デコンボリューション"))
        return QStringLiteral("signal-fsd");
    if (has("2d fft") || has("2d ifft") || has("2次元fft") || has("2次元逆fft"))
        return QStringLiteral("matrix-fft2d");
    if (has("2d spatial filter") || has("plane leveling") || has("2d空間フィルタ")
        || has("平面レベリング"))
        return QStringLiteral("matrix-filter2d");
    if (has("low pass") || has("high pass") || has("band pass") || has("band block")
        || has("fft filter") || has("ローパス") || has("ハイパス") || has("バンドパス")
        || has("バンドストップ"))
        return QStringLiteral("signal-filter");
    if (textIs("fft") || has("フーリエ変換"))
        return QStringLiteral("signal-fft");
    if (has("moving window average") || has("median filter") || has("移動平均")
        || has("メディアンフィルタ") || textIs("smooth") || textIs("平滑化"))
        return QStringLiteral("signal-smooth");

    // Curve Math & Calculus
    if (has("simple curve math") || has("曲線演算") || has("カーブ演算"))
        return QStringLiteral("curvemath");
    if (has("average multiple curves") || has("複数曲線の平均"))
        return QStringLiteral("curve-average");
    if (has("normalize") || has("正規化"))
        return QStringLiteral("curve-normalize");
    if (has("fill area between") || has("polygon / loop area") || has("曲線間の塗りつぶし")
        || has("ループ面積"))
        return QStringLiteral("curve-fill");
    if (textIs("integrate") || has("積分"))
        return QStringLiteral("gadget-integrate");
    if (textIs("differentiate") || has("微分"))
        return QStringLiteral("gadget-diff");
    if (textIs("interpolate") || has("補間") || has("subtract straight line")
        || has("直線差し引き") || has("correlate") || has("convolute") || has("deconvolute"))
        return QStringLiteral("curvemath");

    // Statistics Suite
    if (has("two-way anova") || has("二元配置分散分析"))
        return QStringLiteral("stats-twoway");
    if (has("one-way anova") || has("t-test") || has("一元配置分散分析") || has("t検定"))
        return QStringLiteral("stats-anova");
    if (has("nonparametric") || has("mann-whitney") || has("ノンパラメトリック"))
        return QStringLiteral("stats-nonparam");
    if (has("normality test") || has("shapiro-wilk") || has("正規性検定"))
        return QStringLiteral("stats-normality");
    if (has("outlier test") || has("grubbs") || has("外れ値検定"))
        return QStringLiteral("stats-outlier");
    if (has("paired comparison") || has("before-after") || has("対応のある比較"))
        return QStringLiteral("stats-paired");
    if (has("bland-altman") || has("ブランド・アルトマン"))
        return QStringLiteral("stats-bland-altman");
    if (has("passing-bablok") || has("deming"))
        return QStringLiteral("stats-bablok");
    if (has("weibull") || has("ワイブル"))
        return QStringLiteral("stats-weibull");
    if (has("kaplan-meier") || has("survival") || has("カプラン・マイヤー") || has("生存時間"))
        return QStringLiteral("stats-survival");
    if (has("roc curve") || has("(auc)") || has("roc曲線"))
        return QStringLiteral("stats-roc");
    if (has("principal component") || has("pca biplot") || has("主成分分析"))
        return QStringLiteral("stats-pca");
    if (has("correlation matrix") || has("相関行列"))
        return QStringLiteral("stats-corr");
    if (has("clustering") || has("k-means") || has("クラスタリング"))
        return QStringLiteral("stats-cluster");
    if (has("confidence / prediction ellipse") || has("信頼楕円"))
        return QStringLiteral("stats-ellipse");

    // 2D / 3D Plot Types
    if (has("double-y") || has("2軸プロット") || has("ダブルy"))
        return QStringLiteral("plot-double-y");
    if (has("waterfall") || has("y-offset stack") || has("stack offset") || has("ウォーターフォール"))
        return QStringLiteral("plot-waterfall");
    if (has("violin plot") || has("バイオリン"))
        return QStringLiteral("plot-violin");
    if (has("pareto") || has("パレート"))
        return QStringLiteral("plot-pareto");
    if (has("q-q plot") || has("quantile-quantile") || has("q-qプロット"))
        return QStringLiteral("plot-qq");
    if (has("ternary") || has("三角図") || has("三元系"))
        return QStringLiteral("plot-ternary");
    if (has("box plot") || has("box and whiskers") || has("箱ひげ図"))
        return QStringLiteral("plot-box");
    if (has("histogram") || has("ヒストグラム"))
        return QStringLiteral("plot-histogram");
    if (has("vectors xy") || has("ベクトル"))
        return QStringLiteral("plot-vectors");
    if (has("contour + color") || has("gray scale map") || has("カラーマップ")
        || has("グレースケールマップ"))
        return QStringLiteral("plot-heatmap");
    if (has("contour lines") || has("floor isolines") || has("等高線"))
        return QStringLiteral("plot-contour");
    if (has("3d wire frame") || textIs("wireframe") || has("hidden line") || has("ワイヤーフレーム")
        || has("陰線処理"))
        return QStringLiteral("plot-3d-wireframe");
    if (has("3d surface") || has("3d polygons") || has("3d wire surface") || textIs("polygon only")
        || has("mesh & filled") || textIs("surface") || has("3d曲面") || has("ポリゴン"))
        return QStringLiteral("plot-3d-surface");
    if (has("plot 3d bars") || textIs("cones") || (textIs("bars") && has("3d")))
        return QStringLiteral("plot-3d-bars");
    if (has("plot 3d scatter") || textIs("dots") || textIs("crosshairs"))
        return QStringLiteral("plot-3d-scatter");
    if (has("plot 3d ribbon") || has("3d trajectory") || textIs("ribbon") || textIs("trajectory")
        || has("リボン") || has("軌跡"))
        return QStringLiteral("plot-3d-ribbon");
    if (has("vertical bars") || has("縦棒"))
        return QStringLiteral("plot-bars");
    if (has("horizontal bars") || has("横棒"))
        return QStringLiteral("plot-hbar");
    if (textIs("area") || has("plot area") || textIs("面グラフ") || textIs("エリア"))
        return QStringLiteral("plot-area");
    if (textIs("pie") || has("plot pie") || has("円グラフ"))
        return QStringLiteral("plot-pie");
    if (textIs("spline") || has("スプライン"))
        return QStringLiteral("plot-spline");
    if (has("drop lines") || has("ドロップライン"))
        return QStringLiteral("plot-droplines");
    if (has("vertical steps") || has("horizontal steps") || has("階段プロット") || has("ステップ"))
        return QStringLiteral("plot-steps");
    if (has("line + symbol") || has("lines and/or symbols") || has("add/remove curve")
        || has("add curves") || has("線 + シンボル") || has("曲線の追加"))
        return QStringLiteral("plot-line-symbol");
    if (textIs("scatter") || has("plot as symbols") || textIs("散布図"))
        return QStringLiteral("plot-scatter");
    if (textIs("line") || has("plot as line") || has("draw line") || textIs("折れ線")
        || has("直線を描画"))
        return QStringLiteral("plot-line");

    // Page, Layers, Axes & Graph Enrichments
    if (has("page setup") || has("ページ設定"))
        return QStringLiteral("page-setup");
    if (has("fit page to layers") || has("fit layers to page") || has("fit frame to window")
        || has("window geometry") || has("resize window") || has("ページをレイヤに合わせる")
        || has("レイヤをページに合わせる"))
        return QStringLiteral("fit-page");
    if (has("linked top-x") || has("リンクレイヤ"))
        return QStringLiteral("layer-linked");
    if (has("zoom inset") || has("インセットレイヤ") || has("拡大インセット"))
        return QStringLiteral("layer-inset");
    if (has("arrange layers") || has("automatic layout") || has("manage layers")
        || has("layer geometry") || has("2 layers") || has("4 layers") || has("stacked layers")
        || has("レイヤの整列") || has("自動レイアウト") || has("レイヤ管理"))
        return QStringLiteral("layer-arrange");
    if (has("add layer") || has("レイヤ追加") || has("レイヤを追加"))
        return QStringLiteral("layer-add");
    if (has("remove layer") || has("レイヤ削除") || has("レイヤを削除"))
        return QStringLiteral("delete");
    if (has("color palette") || has("カラーパレット"))
        return QStringLiteral("color-palette");
    if (has("reconstruct legend") || textIs("legend") || has("凡例"))
        return QStringLiteral("legend");
    if (has("error bars") || textIs("x error") || textIs("y error") || has("エラーバー"))
        return QStringLiteral("error-bars");
    if (has("draw arrow") || textIs("arrow") || has("矢印"))
        return QStringLiteral("line-arrow");
    if (has("add text") || textIs("title") || tipIs("enrichments") || has("テキスト追加")
        || textIs("タイトル"))
        return QStringLiteral("text-label");
    if (textIs("scales") || has("軸スケール") || textIs("スケール") || has("set coordinates")
        || has("座標"))
        return QStringLiteral("axis-scale");
    if (textIs("axes") || textIs("軸"))
        return QStringLiteral("axis-ticks-in");
    if (textIs("grid") || has("front grid") || has("back grid") || has("left grid")
        || has("right grid") || has("ceiling grid") || has("floor grid") || has("グリッド"))
        return QStringLiteral("axis-grid");
    if (textIs("box"))
        return QStringLiteral("frame-box-plain");
    if (textIs("frame"))
        return QStringLiteral("frame-open-l");
    if (textIs("no axes"))
        return QStringLiteral("clear");
    if (has("floor data projection"))
        return QStringLiteral("plot-heatmap");
    if (has("empty floor"))
        return QStringLiteral("frame-box-plain");
    if (has("enable perspective"))
        return QStringLiteral("plot-3d-wireframe");
    if (has("reset rotation"))
        return QStringLiteral("recalc-now");
    if (textIs("animation"))
        return QStringLiteral("python-run");

    // Worksheet, Matrix & Formula
    if (has("statistics on columns") || has("column statistics") || has("列の統計"))
        return QStringLiteral("col-stats");
    if (has("statistics on rows") || has("row statistics") || has("行の統計"))
        return QStringLiteral("row-stats");
    if (has("row numbers") || has("行番号"))
        return QStringLiteral("row-numbers");
    if (has("random values") || has("random numbers") || has("乱数"))
        return QStringLiteral("random-numbers");
    if (textIs("dimensions") || has("table size") || has("matrix size") || has("行数・列数")
        || has("サイズ変更") || textIs("次元"))
        return QStringLiteral("table-dimensions");
    if (textIs("transpose") || has("mirror horizontally") || has("mirror vertically")
        || textIs("転置") || has("左右反転") || has("上下反転"))
        return QStringLiteral("matrix-transpose");
    if (textIs("select all") || has("すべて選択"))
        return QStringLiteral("table-drawer");
    if (has("show/hide comments") || has("column description") || has("コメントの表示")
        || has("列の説明"))
        return QStringLiteral("text-label");
    if (has("show/hide controls") || has("change type") || has("set display format")
        || has("コントロールの表示") || has("型と") || has("表示形式"))
        return QStringLiteral("settings");
    if (has("sort table") || has("sort columns") || has("並べ替え") || has("ソート"))
        return QStringLiteral("sort-table");
    if (has("add column") || has("insert empty column") || has("列の追加") || has("列を追加")
        || has("空の列を挿入"))
        return QStringLiteral("add-column");
    if (has("assign formula") || has("formula edit") || has("column labels & formula")
        || has("edit function") || has("add function") || has("new function plot") || has("数式")
        || has("関数プロット"))
        return QStringLiteral("formula");
    if (has("auto recalculate") || has("自動再計算"))
        return QStringLiteral("recalc-auto");
    if (has("recalculate") || has("再計算"))
        return QStringLiteral("recalc-now");
    if (has("new folder") || has("新しいフォルダ") || has("フォルダ作成"))
        return QStringLiteral("new-folder");
    if (has("new table") || textIs("worksheet") || has("convert to table") || has("intensity table")
        || has("insert empty rows") || has("add rows") || has("新しいテーブル")
        || has("新規テーブル") || has("新規ワークシート") || has("新しいワークシート")
        || has("ワークシートへ変換") || has("行を追加") || has("空の行を挿入"))
        return QStringLiteral("new-table");
    if (has("new matrix") || has("convert to matrix") || has("新しい行列") || has("新規マトリクス"))
        return QStringLiteral("new-matrix");
    if (has("new graph") || has("empty 2d plot") || has("plot wizard") || textIs("plot details")
        || textIs("plot") || has("新しいグラフ") || has("新規グラフ"))
        return QStringLiteral("new-graph");
    if (has("new note") || has("save note") || has("新しいノート") || has("スクリプトウィンドウ"))
        return QStringLiteral("new-note");

    // File, Edit, Workspace, Scripting & Help
    if (has("new project") || tipIs("new aspect") || has("新しいプロジェクト") || has("新規プロジェクト"))
        return QStringLiteral("file-new");
    if (has("import ascii") || has("import data") || has("import image") || has("open image")
        || has("asciiインポート") || has("インポート"))
        return QStringLiteral("import-csv");
    if (textIs("open") || has("open project") || has("open template") || has("プロジェクトを開く")
        || has("テンプレートを開く") || textIs("開く"))
        return QStringLiteral("file-open");
    if (has("save project as") || has("save as template") || has("名前を付けてプロジェクトを保存")
        || has("テンプレートとして保存"))
        return QStringLiteral("file-save-as");
    if (has("save project") || textIs("save") || has("プロジェクトを保存") || textIs("保存"))
        return QStringLiteral("file-save");
    if (has("export pdf") || has("export to pdf") || has("export to tex") || has("pdfエクスポート")
        || has("pdf出力") || has("tex"))
        return QStringLiteral("export-pdf");
    if (has("export current graph") || has("export all graphs") || has("export ascii")
        || has("add image") || has("グラフをエクスポート") || has("エクスポート"))
        return QStringLiteral("export-image");
    if (textIs("print") || has("print window") || has("print all plots") || has("印刷"))
        return QStringLiteral("print");

    if (textIs("undo") || has("undo changes") || has("undo/redo history") || has("元に戻す"))
        return QStringLiteral("undo");
    if (textIs("redo") || has("redo changes") || has("やり直し"))
        return QStringLiteral("redo");
    if (textIs("cut") || has("cut selection") || has("切り取り"))
        return QStringLiteral("cut");
    if (textIs("copy") || has("copy selection") || has("copy status bar") || textIs("コピー")
        || has("選択範囲をコピー"))
        return QStringLiteral("copy");
    if (textIs("paste") || has("paste selection") || has("paste transposed") || has("貼り付け"))
        return QStringLiteral("paste");
    if (textIs("duplicate") || has("duplicate window") || textIs("複製"))
        return QStringLiteral("duplicate");
    if (has("delete selection") || textIs("delete") || has("delete fit tables")
        || has("remove columns") || has("remove rows") || textIs("削除") || has("列を削除")
        || has("行を削除"))
        return QStringLiteral("delete");
    if (textIs("clear") || has("clear table") || has("clear matrix") || has("clear log")
        || has("clear columns") || has("clear rows") || textIs("クリア") || has("をクリア"))
        return QStringLiteral("clear");
    if (has("go to cell") || textIs("find") || has("セルへ移動") || has("検索"))
        return QStringLiteral("find");

    if (has("project explorer") || has("プロジェクトエクスプローラ"))
        return QStringLiteral("explorer");
    if (has("results log") || has("time stamp") || has("結果ログ"))
        return QStringLiteral("log-window");
    if (has("lock toolbars") || has("ツールバーを固定") || has("ツールバーのロック"))
        return QStringLiteral("lock");
    if (textIs("preferences") || has("manage dialog themes") || textIs("properties")
        || has("設定") || has("環境設定"))
        return QStringLiteral("settings");
    if (textIs("console") || has("scripting console") || has("scripting language")
        || has("python") || has("コンソール"))
        return QStringLiteral("python-console");
    if (textIs("execute") || has("execute all") || has("evaluate expression")
        || has("restart scripting") || has("実行") || has("式を評価"))
        return QStringLiteral("python-run");

    if (textIs("next") || has("次のウィンドウ"))
        return QStringLiteral("go-next");
    if (textIs("previous") || has("前のウィンドウ"))
        return QStringLiteral("go-previous");
    if (textIs("quit") || has("close window") || textIs("終了") || has("ウィンドウを閉じる"))
        return QStringLiteral("close");
    if (textIs("help") || has("about torigraph") || has("torigraph homepage")
        || has("torigraph forums") || has("report a bug") || has("download manual")
        || has("choose help folder") || has("ヘルプ") || has("torigraph について"))
        return QStringLiteral("help");

    return QString();
}

static bool hasSkippedAncestor(const QWidget *w)
{
    for (const QWidget *p = w ? w->parentWidget() : nullptr; p != nullptr; p = p->parentWidget()) {
        const QString cls = QString::fromLatin1(p->metaObject()->className());
        const QString obj = p->objectName();
        if (cls == QStringLiteral("MultiLayer") || cls == QStringLiteral("Graph")
            || cls == QStringLiteral("Plot") || cls == QStringLiteral("GraphMiniToolbar")
            || cls == QStringLiteral("ModernRibbonBar")
            || cls == QStringLiteral("GraphInspectorDock")
            || cls == QStringLiteral("FocusCanvasHudOverlay")
            || cls == QStringLiteral("FocusTableDrawer") || cls == QStringLiteral("QTabBar")
            || cls == QStringLiteral("QLineEdit") || qobject_cast<const QAbstractSpinBox *>(p)
            || obj == QStringLiteral("ModernRibbonToolBar")
            || obj == QStringLiteral("GraphInspectorDock")
            || obj == QStringLiteral("WorksheetQuickBar")
            || obj == QStringLiteral("MultiLayerPillContainer")) {
            return true;
        }
    }
    return false;
}

static bool isPureCheckmarkOnlyAction(const QAction *action)
{
    if (!action || !action->isCheckable() || !action->icon().isNull())
        return false;

    for (QWidget *w : action->associatedWidgets()) {
        if (qobject_cast<QToolBar *>(w) || qobject_cast<QToolButton *>(w))
            return false;
    }

    const QString text = cleanLabelText(action->text());
    // Language & Aspect Ratio preset radio items keep crisp native [✓] checkmarks
    if (text.contains(QStringLiteral("1:1")) || text.contains(QStringLiteral("4:3"))
        || text.contains(QStringLiteral("16:9")) || text.contains(QStringLiteral("3:2"))
        || text.contains(QStringLiteral("aspect ratio"))
        || text.contains(QString::fromUtf8("アスペクト比"))
        || text == QStringLiteral("english")
        || text.contains(QString::fromUtf8("日本語"))) {
        return true;
    }
    return false;
}

static void collectMenuActionsRecursive(QMenu *menu, QSet<QAction *> &out)
{
    if (!menu)
        return;
    if (QAction *ma = menu->menuAction())
        out.insert(ma);
    const QList<QAction *> acts = menu->actions();
    for (QAction *a : acts) {
        if (!a)
            continue;
        out.insert(a);
        if (QMenu *sub = a->menu()) {
            if (sub != menu)
                collectMenuActionsRecursive(sub, out);
        }
    }
}

} // namespace

void ModernUiTheme::applyModernTheme(QApplication *app)
{
    if (!app)
        return;

    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
    app->setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    IconLoader::init();
    IconLoader::lumen_ = 248;

    QStyle *fusionBase = QStyleFactory::create(QStringLiteral("Fusion"));
    app->setStyle(new ModernFluentProxyStyle(fusionBase));

    // Upgrade legacy 1998 monochrome bitmap Japanese UI fonts (MS UI Gothic / MS PGothic) to crisp ClearType
    {
        QFont uiFont = app->font();
        const QString fam = uiFont.family().trimmed();
        if (fam.isEmpty() || fam.compare(QLatin1String("MS UI Gothic"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS PGothic"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS Gothic"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS Shell Dlg"), Qt::CaseInsensitive) == 0
            || fam.compare(QLatin1String("MS Shell Dlg 2"), Qt::CaseInsensitive) == 0) {
            const QStringList families = QFontDatabase().families();
            if (families.contains(QStringLiteral("Yu Gothic UI"), Qt::CaseInsensitive))
                uiFont.setFamily(QStringLiteral("Yu Gothic UI"));
            else if (families.contains(QStringLiteral("Segoe UI"), Qt::CaseInsensitive))
                uiFont.setFamily(QStringLiteral("Segoe UI"));
        }
        if (uiFont.pointSize() <= 0 || uiFont.pointSize() < 12)
            uiFont.setPointSize(12);
        uiFont.setStyleStrategy(QFont::PreferAntialias);
        app->setFont(uiFont);
    }

    // Keep WindowText and Text pure black (#000000) and Base pure white (#FFFFFF) so legacy
    // QwtPlot / QwtScaleWidget / Graph COLORNAME() serialization and white export canvases stay
    // 100% unchanged; chrome widgets get Slate-900 (#0F172A) via scoped QSS rules below.
    QPalette pal = app->style()->standardPalette();
    pal.setColor(QPalette::Window, QColor(0xF8, 0xFA, 0xFC));          // #F8FAFC chrome
    pal.setColor(QPalette::WindowText, QColor(0x00, 0x00, 0x00));      // #000000 plot/axis safe
    pal.setColor(QPalette::Base, QColor(0xFF, 0xFF, 0xFF));            // #FFFFFF pure white base
    pal.setColor(QPalette::AlternateBase, QColor(0xF8, 0xFA, 0xFC));   // #F8FAFC alternating rows
    pal.setColor(QPalette::ToolTipBase, QColor(0x0F, 0x17, 0x2A));     // #0F172A dark tooltip
    pal.setColor(QPalette::ToolTipText, QColor(0xF8, 0xFA, 0xFC));     // #F8FAFC tooltip text
    pal.setColor(QPalette::Text, QColor(0x00, 0x00, 0x00));            // #000000 axis numbers safe
    pal.setColor(QPalette::Button, QColor(0xFF, 0xFF, 0xFF));          // #FFFFFF buttons
    pal.setColor(QPalette::ButtonText, QColor(0x1E, 0x29, 0x3B));      // #1E293B button text
    pal.setColor(QPalette::BrightText, QColor(0xE1, 0x1D, 0x48));      // #E11D48 accent alert
    pal.setColor(QPalette::Link, QColor(0x25, 0x63, 0xEB));            // #2563EB primary accent
    pal.setColor(QPalette::LinkVisited, QColor(0x1D, 0x4E, 0xD8));     // #1D4ED8
    pal.setColor(QPalette::Highlight, QColor(0x25, 0x63, 0xEB));       // #2563EB selection accent
    pal.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF)); // #FFFFFF
    pal.setColor(QPalette::Light, QColor(0xFF, 0xFF, 0xFF));
    pal.setColor(QPalette::Midlight, QColor(0xF1, 0xF5, 0xF9));
    pal.setColor(QPalette::Mid, QColor(0xE2, 0xE8, 0xF0));
    pal.setColor(QPalette::Dark, QColor(0x94, 0xA3, 0xB8));
    pal.setColor(QPalette::Shadow, QColor(0x64, 0x74, 0x8B));

    // H4: disabled text and placeholders need about 4.5:1 contrast to stay readable;
    // #94A3B8 was only 2.6:1 on white. #5B6B80 is >= 4.6:1 on every chrome background
    // (#FFFFFF, #F8FAFC, #F1F5F9, #E8EDF4) and still clearly lighter than enabled text.
    pal.setColor(QPalette::Disabled, QPalette::WindowText, ModernUiTheme::disabledTextColor());
    pal.setColor(QPalette::Disabled, QPalette::Text, ModernUiTheme::disabledTextColor());
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, ModernUiTheme::disabledTextColor());
    pal.setColor(QPalette::Disabled, QPalette::Base, QColor(0xF1, 0xF5, 0xF9));
    pal.setColor(QPalette::Disabled, QPalette::Button, QColor(0xF8, 0xFA, 0xFC));
    pal.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0xCB, 0xD5, 0xE1));
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x64, 0x74, 0x8B));
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    pal.setColor(QPalette::PlaceholderText, ModernUiTheme::placeholderTextColor());
#endif

    app->setPalette(pal);
    app->setStyleSheet(modernStyleSheet());
    app->installEventFilter(KeyboardFocusTracker::instance());
}

bool ModernUiTheme::keyboardFocusRingTargets(const QWidget *w)
{
    return w && KeyboardFocusTracker::instance()->ringTarget() == w;
}

QString ModernUiTheme::modernStyleSheet()
{
    // Strictly scoped to UI chrome widgets ONLY.
    // Never touches Qwt*/Graph/MultiLayer canvases or global QWidget font-family ("Arial" safe).
    return QStringLiteral(
            "QMainWindow, QDialog {"
            "  background-color: #F8FAFC;"
            "}"
            "QMainWindow::separator {"
            "  background-color: #E2E8F0;"
            "  width: 5px;"
            "  height: 5px;"
            "}"
            "QMainWindow::separator:hover {"
            "  background-color: #60A5FA;"
            "}"
            "QSplitter::handle {"
            "  background-color: #E2E8F0;"
            "}"
            "QSplitter::handle:hover {"
            "  background-color: #60A5FA;"
            "}"
            "QMdiArea {"
            "  background-color: #E8EDF4;"
            "  border: none;"
            "}"
            "QMenuBar {"
            "  background-color: #FFFFFF;"
            "  color: #0F172A;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  border-bottom: 1px solid #E2E8F0;"
            "  padding: 2px 6px;"
            "}"
            "QMenuBar::item {"
            "  background-color: transparent;"
            "  color: #1E293B;"
            "  padding: 4px 10px;"
            "  margin: 1px 1px;"
            "  border-radius: 5px;"
            "}"
            "QMenuBar::item:selected, QMenuBar::item:pressed {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "}"
            "QMenu {"
            "  background-color: #FFFFFF;"
            "  color: #0F172A;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 8px;"
            "  padding: 4px;"
            "}"
            "QMenu::item {"
            "  padding: 5px 26px 5px 10px;"
            "  margin: 1px 2px;"
            "  border-radius: 5px;"
            "  color: #0F172A;"
            "}"
            "QMenu::item:selected {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "}"
            "QMenu::item:disabled {"
            "  color: #5B6B80;"
            "  background-color: transparent;"
            "}"
            "QMenu::separator {"
            "  height: 1px;"
            "  background-color: #E2E8F0;"
            "  margin: 4px 8px;"
            "}"
            "QMenu::icon {"
            "  padding-left: 4px;"
            "}"
            "QToolBar {"
            "  background-color: #F8FAFC;"
            "  border-top: none;"
            "  border-left: none;"
            "  border-right: none;"
            "  border-bottom: 1px solid #E2E8F0;"
            "  spacing: 3px;"
            "  padding: 2px 5px;"
            "}"
            "QToolBar::separator {"
            "  background-color: #E2E8F0;"
            "  width: 1px;"
            "  height: 18px;"
            "  margin: 3px 4px;"
            "}"
            "QToolBar::handle {"
            "  background-color: transparent;"
            "  width: 6px;"
            "}"
            "QToolBar#ModernRibbonToolBar {"
            "  background: #F8FAFC;"
            "  border: none;"
            "  padding: 0px;"
            "  margin: 0px;"
            "  spacing: 0px;"
            "}"
            "QToolBar#ModernRibbonToolBar::handle {"
            "  width: 0px;"
            "  image: none;"
            "}"
            "QToolButton {"
            "  background-color: transparent;"
            "  color: #1E293B;"
            "  font-size: 11pt;"
            "  border: 1px solid transparent;"
            "  border-radius: 6px;"
            "  padding: 3px 5px;"
            "}"
            "QToolButton:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #BFDBFE;"
            "}"
            "QToolButton:pressed {"
            "  background-color: #DBEAFE;"
            "  border: 1px solid #93C5FD;"
            "}"
            "QToolButton:checked {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QToolButton:disabled {"
            "  color: #5B6B80;"
            "  background-color: transparent;"
            "  border-color: transparent;"
            "}"
            "QPushButton {"
            "  background-color: #FFFFFF;"
            "  color: #1E293B;"
            "  font-size: 11pt;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 6px;"
            "  padding: 4px 10px;"
            "}"
            "QPushButton[compactBtn=\"true\"] {"
            "  padding: 2px 4px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QPushButton:pressed {"
            "  background-color: #DBEAFE;"
            "  border: 1px solid #1D4ED8;"
            "}"
            "QPushButton:checked {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border: 1px solid #2563EB;"
            "}"
            "QPushButton:default {"
            "  background-color: #2563EB;"
            "  color: #FFFFFF;"
            "  border: 1px solid #1D4ED8;"
            "}"
            "QPushButton:default:hover {"
            "  background-color: #1D4ED8;"
            "  color: #FFFFFF;"
            "  border: 1px solid #1E40AF;"
            "}"
            "QPushButton:disabled {"
            "  background-color: #F8FAFC;"
            "  color: #5B6B80;"
            "  border: 1px solid #E2E8F0;"
            "}"
            "ColorButton QPushButton {"
            "  padding: 2px 6px;"
            "}"
            "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
            "  background-color: #F8FAFC;"
            "  color: #0F172A;"
            "  font-size: 10pt;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 5px;"
            "  padding: 3px 7px;"
            "  selection-background-color: #2563EB;"
            "  selection-color: #FFFFFF;"
            "}"
            "QComboBox {"
            "  padding-right: 24px;"
            "}"
            "QSpinBox, QDoubleSpinBox {"
            "  padding-right: 22px;"
            "}"
            "QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #94A3B8;"
            "}"
            "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {"
            "  border: 1px solid #2563EB;"
            "  background-color: #FFFFFF;"
            "}"
            "QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {"
            "  background-color: #F1F5F9;"
            "  color: #5B6B80;"
            "  border: 1px solid #E2E8F0;"
            "}"
            "QComboBox::drop-down {"
            "  subcontrol-origin: padding;"
            "  subcontrol-position: top right;"
            "  width: 20px;"
            "  border-left: 1px solid #E2E8F0;"
            "  border-top-right-radius: 5px;"
            "  border-bottom-right-radius: 5px;"
            "  background-color: transparent;"
            "}"
            "QComboBox::drop-down:hover {"
            "  background-color: #F1F5F9;"
            "}"
            "QComboBox::drop-down:on {"
            "  background-color: #EFF6FF;"
            "}"
            "QComboBox::down-arrow {"
            "  image: none;"
            "}"
            "QSpinBox::up-button, QDoubleSpinBox::up-button {"
            "  subcontrol-origin: border;"
            "  subcontrol-position: top right;"
            "  width: 18px;"
            "  border-left: 1px solid #E2E8F0;"
            "  border-bottom: 1px solid #E2E8F0;"
            "  border-top-right-radius: 5px;"
            "  background-color: transparent;"
            "}"
            "QSpinBox::down-button, QDoubleSpinBox::down-button {"
            "  subcontrol-origin: border;"
            "  subcontrol-position: bottom right;"
            "  width: 18px;"
            "  border-left: 1px solid #E2E8F0;"
            "  border-bottom-right-radius: 5px;"
            "  background-color: transparent;"
            "}"
            "QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,"
            "QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {"
            "  background-color: #F1F5F9;"
            "}"
            "QSpinBox::up-button:pressed, QDoubleSpinBox::up-button:pressed,"
            "QSpinBox::down-button:pressed, QDoubleSpinBox::down-button:pressed {"
            "  background-color: #DBEAFE;"
            "}"
            "QSpinBox::up-arrow, QDoubleSpinBox::up-arrow,"
            "QSpinBox::down-arrow, QDoubleSpinBox::down-arrow {"
            "  image: none;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background-color: #FFFFFF;"
            "  color: #0F172A;"
            "  border: 1px solid #E2E8F0;"
            "  selection-background-color: #EFF6FF;"
            "  selection-color: #1D4ED8;"
            "  outline: 0;"
            "}"
            "QTabWidget::pane {"
            "  background-color: #FFFFFF;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 8px;"
            "  top: -1px;"
            "}"
            "QTabBar {"
            "  background-color: transparent;"
            "}"
            "QTabBar::tab {"
            "  background-color: #F8FAFC;"
            "  color: #475569;"
            "  font-size: 11pt;"
            "  border: 1px solid #E2E8F0;"
            "  border-bottom: none;"
            "  border-top-left-radius: 6px;"
            "  border-top-right-radius: 6px;"
            "  padding: 5px 12px;"
            "  margin-right: 2px;"
            "}"
            "QTabBar::tab:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "}"
            "QTabBar::tab:selected {"
            "  background-color: #FFFFFF;"
            "  color: #2563EB;"
            "  border-color: #E2E8F0;"
            "  border-top: 2px solid #2563EB;"
            "}"
            "QDockWidget {"
            "  background-color: #F8FAFC;"
            "  color: #0F172A;"
            "  font-size: 11pt;"
            "  border: 1px solid #CBD5E1;"
            "  titlebar-close-icon: none;"
            "}"
            "QDockWidget::title {"
            "  background-color: #F1F5F9;"
            "  color: #1E293B;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "  border-bottom: 1px solid #CBD5E1;"
            "  padding: 6px 10px;"
            "  text-align: left;"
            "}"
            "QTableView, QTreeView, QTreeWidget {"
            "  background-color: #FFFFFF;"
            "  alternate-background-color: #F8FAFC;"
            "  color: #0F172A;"
            "  font-size: 11pt;"
            "  gridline-color: #E2E8F0;"
            "  border: 1px solid #CBD5E1;"
            "  selection-background-color: #DBEAFE;"
            "  selection-color: #0F172A;"
            "  outline: 0;"
            "}"
            "QTableView::item:selected, QTreeView::item:selected, QTreeWidget::item:selected {"
            "  background-color: #DBEAFE;"
            "  color: #0F172A;"
            "}"
            "QTreeView::item:hover, QTreeWidget::item:hover {"
            "  background-color: #EFF6FF;"
            "}"
            "QListWidget, QListView {"
            "  background-color: #FFFFFF;"
            "  alternate-background-color: #F8FAFC;"
            "  color: #0F172A;"
            "  font-size: 10pt;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 8px;"
            "  padding: 4px;"
            "  outline: 0;"
            "}"
            "QListWidget::item, QListView::item {"
            "  border-radius: 6px;"
            "  padding: 4px 8px;"
            "  margin: 1px 0px;"
            "  color: #1E293B;"
            "  border-left: 2px solid transparent;"
            "}"
            "QListWidget::item:hover, QListView::item:hover {"
            "  background-color: #F1F5F9;"
            "  color: #0F172A;"
            "}"
            "QListWidget::item:selected, QListView::item:selected,"
            "QListWidget::item:selected:active, QListView::item:selected:active {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "  border-left: 2px solid #2563EB;"
            "}"
            "QHeaderView {"
            "  background-color: #F1F5F9;"
            "  border: none;"
            "}"
            "QHeaderView::section {"
            "  background-color: #F1F5F9;"
            "  color: #1E293B;"
            "  font-family: 'Yu Gothic UI', 'Segoe UI', 'Meiryo UI', sans-serif;"
            "  font-size: 11pt;"
            "  font-weight: 600;"
            "  border-top: none;"
            "  border-left: none;"
            "  border-right: 1px solid #E2E8F0;"
            "  border-bottom: 1px solid #CBD5E1;"
            "  padding: 5px 8px;"
            "}"
            "QHeaderView::section:hover {"
            "  background-color: #EFF6FF;"
            "  color: #1D4ED8;"
            "}"
            "QTableCornerButton::section {"
            "  background-color: #F1F5F9;"
            "  border-right: 1px solid #E2E8F0;"
            "  border-bottom: 1px solid #CBD5E1;"
            "}"
            "QScrollBar:vertical {"
            "  background-color: #F8FAFC;"
            "  width: 10px;"
            "  margin: 0px;"
            "  border: none;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background-color: #CBD5E1;"
            "  min-height: 24px;"
            "  margin: 2px;"
            "  border-radius: 3px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "  background-color: #94A3B8;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
            "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
            "  background: none;"
            "  height: 0px;"
            "}"
            "QScrollBar:horizontal {"
            "  background-color: #F8FAFC;"
            "  height: 10px;"
            "  margin: 0px;"
            "  border: none;"
            "}"
            "QScrollBar::handle:horizontal {"
            "  background-color: #CBD5E1;"
            "  min-width: 24px;"
            "  margin: 2px;"
            "  border-radius: 3px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
            "  background-color: #94A3B8;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,"
            "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
            "  background: none;"
            "  width: 0px;"
            "}"
            "QGroupBox {"
            "  background-color: #FFFFFF;"
            "  font-size: 10pt;"
            "  border: 1px solid #E2E8F0;"
            "  border-radius: 8px;"
            "  margin-top: 12px;"
            "  padding-top: 10px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  subcontrol-position: top left;"
            "  left: 10px;"
            "  padding: 0px 6px;"
            "  font-size: 9pt;"
            "  font-weight: 600;"
            "  color: #475569;"
            "}"
            "QStatusBar {"
            "  background-color: #F8FAFC;"
            "  color: #334155;"
            "  font-size: 11pt;"
            "  border-top: 1px solid #E2E8F0;"
            "}"
            "QStatusBar::item {"
            "  border: none;"
            "}"
            "QProgressBar {"
            "  background-color: #E2E8F0;"
            "  color: #0F172A;"
            "  border: 1px solid #CBD5E1;"
            "  border-radius: 5px;"
            "  text-align: center;"
            "  min-height: 14px;"
            "}"
            "QProgressBar::chunk {"
            "  background-color: #2563EB;"
            "  border-radius: 4px;"
            "}"
            "QToolTip {"
            "  background-color: #0F172A;"
            "  color: #F8FAFC;"
            "  font-size: 11pt;"
            "  border: 1px solid #334155;"
            "  border-radius: 5px;"
            "  padding: 4px 8px;"
            "}");
}

QString ModernUiTheme::semanticIconForAction(const QAction *action)
{
    if (!action || action->isSeparator())
        return QString();

    const QString propId = action->property("_tg_duotone_id").toString();
    if (!propId.isEmpty())
        return propId;

    const QString byObj = matchByObjectName(action->objectName());
    if (!byObj.isEmpty())
        return byObj;

    // Skip numbered MRU recent files / window list items ("1. foo.sciprj")
    const QString cleanTxt = cleanLabelText(action->text());
    static const QRegularExpression reNumberedItem(QStringLiteral("^\\d+[\\.\\s:]"));
    if (reNumberedItem.match(cleanTxt).hasMatch())
        return QString();

    if (isPureCheckmarkOnlyAction(action))
        return QString();

    const QString byText = matchByTextAndToolTip(action->text(), action->toolTip(),
                                                 action->shortcut().toString(QKeySequence::PortableText));
    if (!byText.isEmpty())
        return byText;

    // Submenu header fallback: match submenu title or inherit first meaningful child action's domain
    if (QMenu *sub = action->menu()) {
        const QString byMenuTitle = matchByTextAndToolTip(sub->title(), action->toolTip(), QString());
        if (!byMenuTitle.isEmpty())
            return byMenuTitle;
    }

    return QString();
}

QString ModernUiTheme::semanticIconForToolButton(const QToolButton *button)
{
    if (!button)
        return QString();

    const QString propId = button->property("_tg_duotone_id").toString();
    if (!propId.isEmpty())
        return propId;

    if (const QAction *defAct = button->defaultAction()) {
        const QString actId = semanticIconForAction(defAct);
        if (!actId.isEmpty())
            return actId;
    }

    const QString byObj = matchByObjectName(button->objectName());
    if (!byObj.isEmpty())
        return byObj;

    const QString byText = matchByTextAndToolTip(button->text(), button->toolTip(), QString());
    if (!byText.isEmpty())
        return byText;

    if (const QMenu *popupMenu = button->menu()) {
        const QList<QAction *> acts = popupMenu->actions();
        for (const QAction *a : acts) {
            if (a && !a->isSeparator()) {
                const QString subId = semanticIconForAction(a);
                if (!subId.isEmpty())
                    return subId;
            }
        }
    }

    return QString();
}

static void upgradeSingleActionIcon(QAction *action, const QSet<QString> &registeredSet)
{
    if (!action || action->isSeparator())
        return;
    const QString existingIconName = action->icon().name();
    if (existingIconName.endsWith(QStringLiteral(":dark")))
        return;

    const QString semId = ModernUiTheme::semanticIconForAction(action);
    if (!semId.isEmpty()) {
        action->setProperty("_tg_duotone_id", semId);
        if (existingIconName != semId)
            action->setIcon(IconLoader::duotoneIcon(semId, 20, false));
    } else if (!action->icon().isNull() && !registeredSet.contains(existingIconName)) {
        // Guaranteed fallback: replace any remaining legacy .xpm / qrc raster icon with a 2-color Duotone SVG
        const QString fallbackId =
                existingIconName.isEmpty() ? QStringLiteral("new-graph") : existingIconName;
        const QString resolved = IconLoader::resolveIconId(fallbackId);
        action->setProperty("_tg_duotone_id", resolved);
        action->setIcon(IconLoader::duotoneIcon(resolved, 20, false));
    }
}

void ModernUiTheme::upgradeMenuIcons(QMenu *menu)
{
    if (!menu)
        return;

    IconLoader::init();
    const QStringList regList = IconLoader::registeredIconIds();
    const QSet<QString> registeredSet(regList.cbegin(), regList.cend());

    QSet<QAction *> menuActions;
    const QList<QAction *> topActs = menu->actions();
    for (QAction *a : topActs) {
        if (!a)
            continue;
        menuActions.insert(a);
        if (QMenu *sub = a->menu()) {
            if (sub != menu)
                collectMenuActionsRecursive(sub, menuActions);
        }
    }

    for (QAction *action : menuActions)
        upgradeSingleActionIcon(action, registeredSet);
}

void ModernUiTheme::upgradeAllWindowIcons(QWidget *rootWindow)
{
    if (!rootWindow)
        return;

    IconLoader::init();
    const QStringList regList = IconLoader::registeredIconIds();
    const QSet<QString> registeredSet(regList.cbegin(), regList.cend());

    // 1. Standardize classic QToolBar icon sizes to crisp 20x20 px
    const QList<QToolBar *> toolBars = rootWindow->findChildren<QToolBar *>();
    for (QToolBar *tb : toolBars) {
        if (!tb || tb->objectName() == QStringLiteral("ModernRibbonToolBar")
            || hasSkippedAncestor(tb))
            continue;
        if (tb->iconSize().width() != 20)
            tb->setIconSize(QSize(20, 20));
    }

    // 1b. Mark narrow QPushButtons (<= 48 px max width, e.g. "<<", "+", "-") as compactBtn
    const QList<QPushButton *> pushButtons = rootWindow->findChildren<QPushButton *>();
    for (QPushButton *pb : pushButtons) {
        if (!pb || hasSkippedAncestor(pb))
            continue;
        if (pb->maximumWidth() > 0 && pb->maximumWidth() <= 48
            && !pb->property("compactBtn").toBool()) {
            pb->setProperty("compactBtn", true);
            if (QStyle *st = pb->style()) {
                st->unpolish(pb);
                st->polish(pb);
            }
        }
    }

    // 2. Collect all QActions across children, rootWindow actions, and menuBar trees
    QSet<QAction *> allActions;
    const QList<QAction *> childActions = rootWindow->findChildren<QAction *>();
    for (QAction *a : childActions)
        allActions.insert(a);
    const QList<QAction *> directActions = rootWindow->actions();
    for (QAction *a : directActions)
        allActions.insert(a);
    const QList<QMenuBar *> menuBars = rootWindow->findChildren<QMenuBar *>();
    const QList<QMenu *> directTopMenus =
            rootWindow->findChildren<QMenu *>(QString(), Qt::FindDirectChildrenOnly);
    for (QMenu *m : directTopMenus) {
        if (!m)
            continue;
        for (QAction *sa : m->actions()) {
            if (!sa)
                continue;
            allActions.insert(sa);
            if (QMenu *sub2 = sa->menu())
                collectMenuActionsRecursive(sub2, allActions);
        }
    }
    for (QMenuBar *mb : menuBars) {
        if (!mb)
            continue;
        for (QAction *topAct : mb->actions()) {
            if (!topAct)
                continue;
            // Keep top-level horizontal QMenuBar items text-only for clean desktop menu bar layout,
            // while upgrading every action inside their dropdown QMenus!
            if (QMenu *m = topAct->menu()) {
                const QList<QAction *> subActs = m->actions();
                for (QAction *sa : subActs) {
                    if (!sa)
                        continue;
                    allActions.insert(sa);
                    if (QMenu *sub2 = sa->menu())
                        collectMenuActionsRecursive(sub2, allActions);
                }
            }
        }
    }
    // Remove top-level QMenuBar bar-item actions and dynamic top-level QMenus (e.g. plot2D / 作図(P))
    // from icon assignment and clear any prior icon so horizontal menubar labels always render as text!
    for (QMenu *m : directTopMenus) {
        if (!m)
            continue;
        if (QAction *ma = m->menuAction()) {
            allActions.remove(ma);
            if (!ma->icon().isNull())
                ma->setIcon(QIcon());
        }
    }
    for (QMenuBar *mb : menuBars) {
        if (!mb)
            continue;
        for (QAction *topAct : mb->actions()) {
            if (!topAct)
                continue;
            allActions.remove(topAct);
            if (!topAct->icon().isNull())
                topAct->setIcon(QIcon());
        }
    }

    for (QAction *action : allActions)
        upgradeSingleActionIcon(action, registeredSet);

    // 3. Upgrade standalone & popup QToolButtons (Explorer toolbar, Plot/Layer popup buttons, etc.)
    const QList<QToolButton *> toolButtons = rootWindow->findChildren<QToolButton *>();
    for (QToolButton *btn : toolButtons) {
        if (!btn)
            continue;
        const QString obj = btn->objectName();
        if (obj.startsWith(QStringLiteral("qt_")) || obj.startsWith(QStringLiteral("_q_"))
            || obj == QStringLiteral("MultiLayerStatusPill")) {
            continue;
        }
        if (hasSkippedAncestor(btn))
            continue;
        if (btn->defaultAction() != nullptr)
            continue;

        const QString existingIconName = btn->icon().name();
        if (existingIconName.endsWith(QStringLiteral(":dark")))
            continue;

        if (btn->maximumWidth() > 0 && btn->maximumWidth() <= 48 && btn->icon().isNull()
            && !btn->text().isEmpty()) {
            continue;
        }

        const QString semId = semanticIconForToolButton(btn);
        if (!semId.isEmpty()) {
            btn->setProperty("_tg_duotone_id", semId);
            if (existingIconName != semId)
                btn->setIcon(IconLoader::duotoneIcon(semId, 20, false));
            if (!btn->text().isEmpty() && btn->toolButtonStyle() == Qt::ToolButtonIconOnly)
                btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        } else if (!btn->icon().isNull() && !registeredSet.contains(existingIconName)) {
            const QString fallbackId =
                    existingIconName.isEmpty() ? QStringLiteral("new-graph") : existingIconName;
            const QString resolved = IconLoader::resolveIconId(fallbackId);
            btn->setProperty("_tg_duotone_id", resolved);
            btn->setIcon(IconLoader::duotoneIcon(resolved, 20, false));
        }
    }

    // 4. Upgrade MDI SubWindow / Workbook / Graph / Matrix / Note window icons
    const QList<QMdiSubWindow *> subWindows = rootWindow->findChildren<QMdiSubWindow *>();
    for (QMdiSubWindow *sub : subWindows) {
        if (!sub)
            continue;
        const QString cls = QString::fromLatin1(sub->metaObject()->className());
        QString subIconId;
        if (cls == QStringLiteral("Table"))
            subIconId = QStringLiteral("new-table");
        else if (cls == QStringLiteral("Matrix"))
            subIconId = QStringLiteral("new-matrix");
        else if (cls == QStringLiteral("MultiLayer"))
            subIconId = QStringLiteral("new-graph");
        else if (cls == QStringLiteral("Graph3D"))
            subIconId = QStringLiteral("plot-3d-surface");
        else if (cls == QStringLiteral("Note"))
            subIconId = QStringLiteral("new-note");

        if (!subIconId.isEmpty() && sub->windowIcon().name() != subIconId)
            sub->setWindowIcon(IconLoader::duotoneIcon(subIconId, 18, false));
    }

    // 5. Upgrade Project Explorer folder & window tree item icons (replaces legacy folder/table .xpm)
    const QList<QTreeWidget *> trees = rootWindow->findChildren<QTreeWidget *>();
    for (QTreeWidget *tree : trees) {
        if (!tree)
            continue;
        const QString treeName = tree->objectName();
        if (treeName == QStringLiteral("folders")) {
            QTreeWidgetItemIterator it(tree);
            while (*it) {
                QTreeWidgetItem *item = *it;
                const QString fid = item->isExpanded() ? QStringLiteral("folder-open")
                                                       : QStringLiteral("folder-closed");
                if (item->icon(0).name() != fid)
                    item->setIcon(0, IconLoader::duotoneIcon(fid, 18, false));
                ++it;
            }
        } else if (treeName == QStringLiteral("lv")) {
            QTreeWidgetItemIterator it(tree);
            while (*it) {
                QTreeWidgetItem *item = *it;
                const QString typeStr = item->text(1).toLower();
                QString itemId = QStringLiteral("new-graph");
                // Decide by the window's class, not by the translated "Type" text: the Japanese
                // name of a table changed ("テーブル" -> "ワークシート") and tables got the
                // graph icon. The text is only a fallback for rows without a window.
                MyWidget *rowWindow = (item->type() == WindowListItem::WindowType)
                        ? static_cast<WindowListItem *>(item)->window()
                        : nullptr;
                if (rowWindow) {
                    if (rowWindow->inherits("Table"))
                        itemId = QStringLiteral("new-table");
                    else if (rowWindow->inherits("Matrix"))
                        itemId = QStringLiteral("new-matrix");
                    else if (rowWindow->inherits("Note"))
                        itemId = QStringLiteral("new-note");
                } else if (typeStr.contains(QStringLiteral("table"))
                    || typeStr.contains(QString::fromUtf8("テーブル"))
                    || typeStr.contains(QString::fromUtf8("ワークシート")))
                    itemId = QStringLiteral("new-table");
                else if (typeStr.contains(QStringLiteral("matrix"))
                         || typeStr.contains(QString::fromUtf8("行列")))
                    itemId = QStringLiteral("new-matrix");
                else if (typeStr.contains(QStringLiteral("note"))
                         || typeStr.contains(QString::fromUtf8("ノート")))
                    itemId = QStringLiteral("new-note");
                else if (typeStr.contains(QStringLiteral("folder"))
                         || typeStr.contains(QString::fromUtf8("フォルダ")))
                    itemId = QStringLiteral("folder-closed");
                if (item->icon(0).name() != itemId)
                    item->setIcon(0, IconLoader::duotoneIcon(itemId, 18, false));
                ++it;
            }
        }
    }
}

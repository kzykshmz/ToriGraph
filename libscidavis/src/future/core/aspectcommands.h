/***************************************************************************
    File                 : aspectcommands.h
    Project              : SciDAVis
    --------------------------------------------------------------------
    Copyright            : (C) 2007-2009 by Knut Franke, Tilman Benkert
    Email (use @ for *)  : knut.franke*gmx.de, thzs*gmx.net
    Description          : Undo commands used by AbstractAspect.
                           Only meant to be used within AbstractAspect.cpp

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

#include "AspectPrivate.h"

#include <QPointer>
#include <QUndoCommand>
#include <QVariant>

class AspectNameChangeCmd : public QUndoCommand
{
public:
    AspectNameChangeCmd(AbstractAspect::Private *target, const QString &new_name)
        : d_target(target)
        , d_target_aspect(target ? target->owner() : nullptr)
        , d_other_name(new_name)
    {
        if (d_target)
            setText(QObject::tr("%1: rename to %2").arg(d_target->name()).arg(new_name));
    }

    virtual void redo()
    {
        if (!d_target_aspect || !d_target)
            return;
        QString tmp = d_target->name();
        d_target->setName(d_other_name);
        d_other_name = tmp;
    }

    virtual void undo() { redo(); }

private:
    AbstractAspect::Private *d_target;
    QPointer<AbstractAspect> d_target_aspect;
    QString d_other_name;
};

class AspectCommentChangeCmd : public QUndoCommand
{
public:
    AspectCommentChangeCmd(AbstractAspect::Private *target, const QString &new_comment)
        : d_target(target)
        , d_target_aspect(target ? target->owner() : nullptr)
        , d_other_comment(new_comment)
    {
        if (d_target)
            setText(QObject::tr("%1: change comment").arg(d_target->name()));
    }

    virtual void redo()
    {
        if (!d_target_aspect || !d_target)
            return;
        QString tmp = d_target->comment();
        d_target->setComment(d_other_comment);
        d_other_comment = tmp;
    }

    virtual void undo() { redo(); }

private:
    AbstractAspect::Private *d_target;
    QPointer<AbstractAspect> d_target_aspect;
    QString d_other_comment;
};

class AspectCaptionSpecChangeCmd : public QUndoCommand
{
public:
    AspectCaptionSpecChangeCmd(AbstractAspect::Private *target, const QString &new_caption_spec)
        : d_target(target)
        , d_target_aspect(target ? target->owner() : nullptr)
        , d_other_caption_spec(new_caption_spec)
    {
        if (d_target)
            setText(QObject::tr("%1: change caption").arg(d_target->name()));
    }

    virtual void redo()
    {
        if (!d_target_aspect || !d_target)
            return;
        QString tmp = d_target->captionSpec();
        d_target->setCaptionSpec(d_other_caption_spec);
        d_other_caption_spec = tmp;
    }

    virtual void undo() { redo(); }

private:
    AbstractAspect::Private *d_target;
    QPointer<AbstractAspect> d_target_aspect;
    QString d_other_caption_spec;
};

class AspectCreationTimeChangeCmd : public QUndoCommand
{
public:
    AspectCreationTimeChangeCmd(AbstractAspect::Private *target, const QDateTime &new_creation_time)
        : d_target(target)
        , d_target_aspect(target ? target->owner() : nullptr)
        , d_other_creation_time(new_creation_time)
    {
        if (d_target)
            setText(QObject::tr("%1: set creation time").arg(d_target->name()));
    }

    virtual void redo()
    {
        if (!d_target_aspect || !d_target)
            return;
        QDateTime tmp = d_target->creationTime();
        d_target->setCreationTime(d_other_creation_time);
        d_other_creation_time = tmp;
    }

    virtual void undo() { redo(); }

private:
    AbstractAspect::Private *d_target;
    QPointer<AbstractAspect> d_target_aspect;
    QDateTime d_other_creation_time;
};

class AspectChildRemoveCmd : public QUndoCommand
{
public:
    AspectChildRemoveCmd(AbstractAspect::Private *target, AbstractAspect *child, bool detach)
        : d_target(target)
        , d_target_aspect(target ? target->owner() : nullptr)
        , d_child(child)
        , d_index(-1)
        , d_removed(false)
        , d_detach(detach)
    {
        if (d_target && d_child)
            setText(QObject::tr("%1: remove %2").arg(d_target->name()).arg(d_child->name()));
    }
    ~AspectChildRemoveCmd()
    {
        if (d_removed && !d_detach && d_child) {
            d_child->setProperty("_tg_owned_by_undo_cmd", QVariant());
            delete d_child.data();
        }
    }

    // calling redo transfers ownership of d_child to the undo command
    virtual void redo()
    {
        if (!d_target_aspect || !d_target || !d_child)
            return;
        if (d_target->indexOfChild(d_child.data()) < 0)
            return;
        d_index = d_target->removeChild(d_child.data());
        d_removed = true;
        if (!d_detach && d_child)
            d_child->setProperty("_tg_owned_by_undo_cmd", true);
    }

    // calling undo transfers ownership of d_child back to its parent aspect
    // (also invoked by AspectChildAddCmd::redo() when d_removed is false)
    virtual void undo()
    {
        if (!d_target_aspect || !d_target || !d_child || d_index < 0)
            return;
        if (d_child->property("_tg_suppressed_removed").toBool()
            || d_child->parentAspect() != nullptr)
            return;
        d_child->setProperty("_tg_owned_by_undo_cmd", QVariant());
        d_target->insertChild(qBound(0, d_index, d_target->childCount()), d_child.data());
        d_removed = false;
    }

protected:
    AbstractAspect::Private *d_target;
    QPointer<AbstractAspect> d_target_aspect;
    QPointer<AbstractAspect> d_child;
    int d_index;
    bool d_removed, d_detach;
};

class AspectChildAddCmd : public AspectChildRemoveCmd
{
public:
    AspectChildAddCmd(AbstractAspect::Private *target, AbstractAspect *child, int index)
        : AspectChildRemoveCmd(target, child, false)
    {
        setText(QObject::tr("%1: add %2").arg(d_target->name()).arg(d_child->name()));
        d_index = index;
    }

    virtual void redo() { AspectChildRemoveCmd::undo(); }

    virtual void undo() { AspectChildRemoveCmd::redo(); }
};

class AspectChildMoveCmd : public QUndoCommand
{
public:
    AspectChildMoveCmd(AbstractAspect::Private *target, int from, int to)
        : d_target(target)
        , d_target_aspect(target ? target->owner() : nullptr)
        , d_from(from)
        , d_to(to)
    {
        if (d_target) {
            setText(QObject::tr("%1: move child from position %2 to %3.")
                            .arg(d_target->name())
                            .arg(d_from + 1)
                            .arg(d_to + 1));
        }
    }

    virtual void redo()
    {
        if (!d_target_aspect || !d_target)
            return;
        const int count = d_target->childCount();
        if (d_from < 0 || d_from >= count || d_to < 0 || d_to >= count)
            return;
        // Moving in one go confuses QTreeView, so we would need another two signals
        // to be mapped to QAbstractItemModel::layoutAboutToBeChanged() and ::layoutChanged().
        AbstractAspect *child = d_target->child(d_from);
        if (!child)
            return;
        d_target->removeChild(child);
        d_target->insertChild(qBound(0, d_to, d_target->childCount()), child);
    }

    virtual void undo()
    {
        if (!d_target_aspect || !d_target)
            return;
        const int count = d_target->childCount();
        if (d_from < 0 || d_from >= count || d_to < 0 || d_to >= count)
            return;
        AbstractAspect *child = d_target->child(d_to);
        if (!child)
            return;
        d_target->removeChild(child);
        d_target->insertChild(qBound(0, d_from, d_target->childCount()), child);
    }

private:
    AbstractAspect::Private *d_target;
    QPointer<AbstractAspect> d_target_aspect;
    int d_from, d_to;
};

class AspectChildReparentCmd : public QUndoCommand
{
public:
    AspectChildReparentCmd(AbstractAspect::Private *target, AbstractAspect::Private *new_parent,
                           AbstractAspect *child, int new_index)
        : d_target(target),
          d_target_aspect(target ? target->owner() : nullptr),
          d_new_parent(new_parent),
          d_new_parent_aspect(new_parent ? new_parent->owner() : nullptr),
          d_child(child),
          d_index(-1),
          d_new_index(new_index)
    {
        if (d_target && d_child && d_new_parent) {
            setText(QObject::tr("%1: move %2 to %3.")
                            .arg(d_target->name())
                            .arg(d_child->name())
                            .arg(d_new_parent->name()));
        }
    }
    ~AspectChildReparentCmd() { }

    // calling redo transfers ownership of d_child to the new parent aspect
    virtual void redo()
    {
        if (!d_target_aspect || !d_new_parent_aspect || !d_target || !d_new_parent || !d_child)
            return;
        if (d_target->indexOfChild(d_child.data()) < 0)
            return;
        d_index = d_target->removeChild(d_child.data());
        d_new_parent->insertChild(qBound(0, d_new_index, d_new_parent->childCount()), d_child.data());
    }

    // calling undo transfers ownership of d_child back to its previous parent aspect
    virtual void undo()
    {
        if (!d_target_aspect || !d_new_parent_aspect || !d_target || !d_new_parent || !d_child
            || d_index < 0)
            return;
        if (d_new_parent->indexOfChild(d_child.data()) < 0)
            return;
        d_new_parent->removeChild(d_child.data());
        d_target->insertChild(qBound(0, d_index, d_target->childCount()), d_child.data());
    }

protected:
    AbstractAspect::Private *d_target;
    QPointer<AbstractAspect> d_target_aspect;
    AbstractAspect::Private *d_new_parent;
    QPointer<AbstractAspect> d_new_parent_aspect;
    QPointer<AbstractAspect> d_child;
    int d_index;
    int d_new_index;
};

/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlnavigatorplacesselector_p.h"

#include <KProtocolInfo>
#include <KUrlMimeData>
#include <kfileplacesmodel.h>

#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QStyle>

namespace KDEPrivate
{
KUrlNavigatorPlacesSelector::KUrlNavigatorPlacesSelector(KUrlNavigator *parent, KFilePlacesModel *placesModel)
    : KUrlNavigatorButtonBase(parent)
    , m_selectedItem(-1)
    , m_placesModel(placesModel)
{
    setFocusPolicy(Qt::NoFocus);

    connect(m_placesModel, &KFilePlacesModel::reloaded, this, [this] {
        updateSelection(m_selectedUrl);
    });

    m_placesMenu = new QMenu(this);
    m_placesMenu->installEventFilter(this);
    connect(m_placesMenu, &QMenu::aboutToShow, this, &KUrlNavigatorPlacesSelector::updateMenu);
    connect(m_placesMenu, &QMenu::triggered, this, [this](QAction *action) {
        activatePlace(action, &KUrlNavigatorPlacesSelector::placeActivated);
    });

    setMenu(m_placesMenu);

    setAcceptDrops(true);
}

KUrlNavigatorPlacesSelector::~KUrlNavigatorPlacesSelector()
{
}

void KUrlNavigatorPlacesSelector::updateMenu()
{
    m_placesMenu->clear();

    // Submenus have to be deleted explicitly (QTBUG-11070)
    for (QObject *obj : QObjectList(m_placesMenu->children())) {
        delete qobject_cast<QMenu *>(obj); // Noop for nullptr
    }

    QString previousGroup;
    QMenu *subMenu = nullptr;

    const int rowCount = m_placesModel->rowCount();
    for (int i = 0; i < rowCount; ++i) {
        QModelIndex index = m_placesModel->index(i, 0);
        if (m_placesModel->isHidden(index)) {
            continue;
        }

        QAction *placeAction = new QAction(m_placesModel->icon(index), m_placesModel->text(index), m_placesMenu);
        placeAction->setData(i);

        const QString &groupName = index.data(KFilePlacesModel::GroupRole).toString();
        if (previousGroup.isEmpty()) { // Skip first group heading.
            previousGroup = groupName;
        }

        // Put all subsequent categories into a submenu.
        if (previousGroup != groupName) {
            QAction *subMenuAction = new QAction(groupName, m_placesMenu);
            subMenu = new QMenu(m_placesMenu);
            subMenu->installEventFilter(this);
            subMenuAction->setMenu(subMenu);

            m_placesMenu->addAction(subMenuAction);

            previousGroup = groupName;
        }

        if (subMenu) {
            subMenu->addAction(placeAction);
        } else {
            m_placesMenu->addAction(placeAction);
        }

        if (i == m_selectedItem) {
            setIcon(m_placesModel->icon(index));
        }
    }

    const QModelIndex index = m_placesModel->index(m_selectedItem, 0);
    if (QAction *teardown = m_placesModel->teardownActionForIndex(index)) {
        m_placesMenu->addSeparator();

        teardown->setParent(m_placesMenu);
        m_placesMenu->addAction(teardown);
    }
}

void KUrlNavigatorPlacesSelector::updateSelection(const QUrl &url)
{
    const QModelIndex index = m_placesModel->closestItem(url);
    if (index.isValid()) {
        m_selectedItem = index.row();
        m_selectedUrl = url;
        setIcon(m_placesModel->icon(index));
    } else {
        m_selectedItem = -1;
        // No bookmark has been found which matches to the given Url.
        // Show the protocol's icon as pixmap for indication, if available:
        QIcon icon;
        if (!url.scheme().isEmpty()) {
            if (const QString iconName = KProtocolInfo::icon(url.scheme()); !iconName.isEmpty()) {
                icon = QIcon::fromTheme(iconName);
            }
        }
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("folder"));
        }
        setIcon(icon);
    }
}

QUrl KUrlNavigatorPlacesSelector::selectedPlaceUrl() const
{
    const QModelIndex index = m_placesModel->index(m_selectedItem, 0);
    return index.isValid() ? m_placesModel->url(index) : QUrl();
}

QString KUrlNavigatorPlacesSelector::selectedPlaceText() const
{
    const QModelIndex index = m_placesModel->index(m_selectedItem, 0);
    return index.isValid() ? m_placesModel->text(index) : QString();
}

QSize KUrlNavigatorPlacesSelector::sizeHint() const
{
    const int height = KUrlNavigatorButtonBase::sizeHint().height();
    return QSize(height, height);
}

void KUrlNavigatorPlacesSelector::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    drawHoverBackground(&painter);

    // draw icon
    const QPixmap pixmap = icon().pixmap(QSize(22, 22).expandedTo(iconSize()), QIcon::Normal);
    style()->drawItemPixmap(&painter, rect(), Qt::AlignCenter, pixmap);
}

void KUrlNavigatorPlacesSelector::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        setDisplayHintEnabled(DraggedHint, true);
        event->acceptProposedAction();

        update();
    }
}

void KUrlNavigatorPlacesSelector::dragLeaveEvent(QDragLeaveEvent *event)
{
    KUrlNavigatorButtonBase::dragLeaveEvent(event);

    setDisplayHintEnabled(DraggedHint, false);
    update();
}

void KUrlNavigatorPlacesSelector::dropEvent(QDropEvent *event)
{
    setDisplayHintEnabled(DraggedHint, false);
    update();

    QMimeDatabase db;
    const QList<QUrl> urlList = KUrlMimeData::urlsFromMimeData(event->mimeData());
    for (const QUrl &url : urlList) {
        QMimeType mimetype = db.mimeTypeForUrl(url);
        if (mimetype.inherits(QStringLiteral("inode/directory"))) {
            m_placesModel->addPlace(url.fileName(), url);
        }
    }
}

void KUrlNavigatorPlacesSelector::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton && geometry().contains(event->pos())) {
        Q_EMIT tabRequested(KFilePlacesModel::convertedUrl(m_placesModel->url(m_placesModel->index(m_selectedItem, 0))));
        event->accept();
        return;
    }

    KUrlNavigatorButtonBase::mouseReleaseEvent(event);
}

void KUrlNavigatorPlacesSelector::activatePlace(QAction *action, ActivationSignal activationSignal)
{
    Q_ASSERT(action != nullptr);
    if (action->data().toString() == QLatin1String("teardownAction")) {
        QModelIndex index = m_placesModel->index(m_selectedItem, 0);
        m_placesModel->requestTeardown(index);
        return;
    }

    QModelIndex index = m_placesModel->index(action->data().toInt(), 0);

    m_lastClickedIndex = QPersistentModelIndex();
    m_lastActivationSignal = nullptr;

    if (m_placesModel->setupNeeded(index)) {
        connect(m_placesModel, &KFilePlacesModel::setupDone, this, &KUrlNavigatorPlacesSelector::onStorageSetupDone);

        m_lastClickedIndex = index;
        m_lastActivationSignal = activationSignal;
        m_placesModel->requestSetup(index);
        return;
    } else if (index.isValid()) {
        if (activationSignal == &KUrlNavigatorPlacesSelector::placeActivated) {
            m_selectedItem = index.row();
            setIcon(m_placesModel->icon(index));
        }

        const QUrl url = KFilePlacesModel::convertedUrl(m_placesModel->url(index));
        /*Q_EMIT*/ std::invoke(activationSignal, this, url);
    }
}

void KUrlNavigatorPlacesSelector::onStorageSetupDone(const QModelIndex &index, bool success)
{
    disconnect(m_placesModel, &KFilePlacesModel::setupDone, this, &KUrlNavigatorPlacesSelector::onStorageSetupDone);

    if (m_lastClickedIndex == index) {
        if (success) {
            if (m_lastActivationSignal == &KUrlNavigatorPlacesSelector::placeActivated) {
                m_selectedItem = index.row();
                setIcon(m_placesModel->icon(index));
            }

            const QUrl url = KFilePlacesModel::convertedUrl(m_placesModel->url(index));
            /*Q_EMIT*/ std::invoke(m_lastActivationSignal, this, url);
        }
        m_lastClickedIndex = QPersistentModelIndex();
        m_lastActivationSignal = nullptr;
    }
}

bool KUrlNavigatorPlacesSelector::eventFilter(QObject *watched, QEvent *event)
{
    if (auto *menu = qobject_cast<QMenu *>(watched)) {
        if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::MiddleButton) {
                if (QAction *action = menu->activeAction()) {
                    m_placesMenu->close(); // always close top menu

                    activatePlace(action, &KUrlNavigatorPlacesSelector::tabRequested);
                    return true;
                }
            }
        }
    }

    return KUrlNavigatorButtonBase::eventFilter(watched, event);
}

} // namespace KDEPrivate

#include "moc_kurlnavigatorplacesselector_p.cpp"

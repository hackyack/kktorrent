/***************************************************************************
*   Copyright (C) 2009 by Joris Guisson                                   *
*   joris.guisson@gmail.com                                               *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
***************************************************************************/
#include <QAction>
#include <KIcon>
#include <KConfigGroup>
#include "centralwidget.h"
#include <interfaces/activity.h>



namespace kt
{
    CentralWidget::CentralWidget(QWidget* parent) : QStackedWidget(parent)
    {
        activity_switching_group = new QActionGroup(this);
        connect(activity_switching_group, SIGNAL(triggered(QAction*)), this, SLOT(switchActivity(QAction*)));
    }

    CentralWidget::~CentralWidget()
    {
    }

    void CentralWidget::loadState(KSharedConfigPtr cfg)
    {
        KConfigGroup g = cfg->group("MainWindow");
        int idx = g.readEntry("current_activity", 0);
        Activity* act = (Activity*)widget(idx);
        if (act)
            setCurrentActivity(act);

        QList<QAction*> actions = activity_switching_group->actions();
        foreach (QAction* a, actions)
        {
            if (a->data().value<QObject*>() == act || act == 0)
            {
                a->setChecked(true);
                break;
            }
        }
    }

    void CentralWidget::saveState(KSharedConfigPtr cfg)
    {
        KConfigGroup g = cfg->group("MainWindow");
        g.writeEntry("current_activity", currentIndex());
    }

    KAction* CentralWidget::addActivity(Activity* act)
    {
        KAction* a = new KAction(KIcon(act->icon()), act->name(), this);
        activity_switching_group->addAction(a);
        a->setCheckable(true);
        a->setToolTip(act->toolTip());
        a->setData(qVariantFromValue<QObject*>(act));
        addWidget(act);
        return a;
    }

    void CentralWidget::removeActivity(Activity* act)
    {
        QList<QAction*> actions = activity_switching_group->actions();
        foreach (QAction* a, actions)
        {
            if (a->data().value<QObject*>() == act)
            {
                activity_switching_group->removeAction(a);
                a->deleteLater();
                break;
            }
        }
        removeWidget(act);
    }

    void CentralWidget::setCurrentActivity(Activity* act)
    {
        setCurrentWidget(act);
    }

    Activity* CentralWidget::currentActivity()
    {
        return (Activity*)currentWidget();
    }

    QList< QAction* > CentralWidget::activitySwitchingActions()
    {
        return activity_switching_group->actions();
    }

    void CentralWidget::switchActivity(QAction* action)
    {
        for (int i = 0; i < count(); i++)
        {
            Activity* act = (Activity*)widget(i);
            if (action->data().value<QObject*>() == act)
            {
                changeActivity(act);
                break;
            }
        }
    }

}


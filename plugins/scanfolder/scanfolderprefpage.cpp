/***************************************************************************
 *   Copyright (C) 2006 by Ivan Vasić                                     *
 *   ivasic@gmail.com                                                      *
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
#include "scanfolderplugin.h"
#include "scanfolderprefpage.h"

#include <klocale.h>
#include <kglobal.h>
#include <kfiledialog.h>
#include <util/functions.h>
#include <groups/groupmanager.h>
#include <interfaces/coreinterface.h>
#include "scanfolderpluginsettings.h"



namespace kt
{

    ScanFolderPrefPage::ScanFolderPrefPage(ScanFolderPlugin* plugin, QWidget* parent)
        : PrefPageInterface(ScanFolderPluginSettings::self(), i18nc("plugin name", "Scan Folder"), "folder-open", parent), m_plugin(plugin)
    {
        setupUi(this);
        connect(kcfg_actionDelete, SIGNAL(toggled(bool)), kcfg_actionMove, SLOT(setDisabled(bool)));
        connect(m_add, SIGNAL(clicked()), this, SLOT(addPressed()));
        connect(m_remove, SIGNAL(clicked()), this, SLOT(removePressed()));
        connect(m_folders, SIGNAL(itemSelectionChanged()), this, SLOT(selectionChanged()));
        connect(m_group, SIGNAL(currentIndexChanged(int)), this, SLOT(currentGroupChanged(int)));
    }


    ScanFolderPrefPage::~ScanFolderPrefPage()
    {}

    void ScanFolderPrefPage::loadSettings()
    {
        kcfg_actionMove->setEnabled(!ScanFolderPluginSettings::actionDelete());

        m_group->clear();

        GroupManager* gman = m_plugin->getCore()->getGroupManager();
        QStringList grps;
        GroupManager::Itr it = gman->begin();
        int current = 0;
        int cnt = 0;
        //now custom ones
        while (it != gman->end())
        {
            if (it->second->groupFlags() & Group::CUSTOM_GROUP)
            {
                grps << it->first;
                if (it->first == ScanFolderPluginSettings::group())
                    current = cnt;
                cnt++;
            }
            ++it;
        }
        m_group->addItems(grps);
        m_group->setEnabled(ScanFolderPluginSettings::addToGroup() && grps.count() > 0);
        m_group->setCurrentIndex(current);
        kcfg_addToGroup->setEnabled(grps.count() > 0);

        m_folders->clear();
        folders = ScanFolderPluginSettings::folders();
        foreach (const QString& f, folders)
        {
            m_folders->addItem(new QListWidgetItem(KIcon("folder"), f));
        }
        selectionChanged();
    }

    void ScanFolderPrefPage::loadDefaults()
    {
        kcfg_actionMove->setEnabled(!ScanFolderPluginSettings::actionDelete());

        m_folders->clear();
        folders.clear();
    }

    void ScanFolderPrefPage::updateSettings()
    {
        if (kcfg_addToGroup->isChecked() && kcfg_addToGroup->isEnabled())
            ScanFolderPluginSettings::setGroup(m_group->currentText());
        else
            ScanFolderPluginSettings::setGroup(QString());

        ScanFolderPluginSettings::setFolders(folders);
        ScanFolderPluginSettings::self()->writeConfig();
        m_plugin->updateScanFolders();
    }

    void ScanFolderPrefPage::addPressed()
    {
        KUrl dir = KFileDialog::getExistingDirectoryUrl(KUrl("kfiledialog:///openTorrent"), this);
        if (dir.isValid())
        {
            QString p = dir.toLocalFile();
            if (!p.endsWith(bt::DirSeparator()))
                p += bt::DirSeparator();
            m_folders->addItem(new QListWidgetItem(KIcon("folder"), p));
            folders.append(p);
        }

        updateButtons();
    }

    void ScanFolderPrefPage::removePressed()
    {
        QList<QListWidgetItem*> sel = m_folders->selectedItems();
        foreach (QListWidgetItem* i, sel)
        {
            folders.removeAll(i->text());
            delete i;
        }

        updateButtons();
    }

    void ScanFolderPrefPage::selectionChanged()
    {
        m_remove->setEnabled(m_folders->selectedItems().count() > 0);
    }

    void ScanFolderPrefPage::currentGroupChanged(int idx)
    {
        Q_UNUSED(idx);
        updateButtons();
    }

    bool ScanFolderPrefPage::customWidgetsChanged()
    {
        return ScanFolderPluginSettings::group() != m_group->currentText() ||
               folders != ScanFolderPluginSettings::folders();
    }
}

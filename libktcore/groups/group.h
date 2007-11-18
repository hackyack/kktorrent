/***************************************************************************
 *   Copyright (C) 2007 by Joris Guisson                                   *
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
#ifndef KTGROUP_H
#define KTGROUP_H

#include <qstring.h>
#include <qicon.h>
#include <ktcore_export.h>

namespace bt
{
	class BEncoder;
	class BDictNode;
	class TorrentInterface;
}

namespace kt
{
	using bt::TorrentInterface;
	
	/**
	 * @author Joris Guisson <joris.guisson@gmail.com>
	 * 
	 * Base class for all groups. Subclasses should only implement the
	 * isMember function, but can also provide save and load
	 * functionality.
	 */
	class KTCORE_EXPORT Group
	{
	protected:
		QString name;
		QIcon icon;
		QString icon_name;
		int flags;
	public:
		enum Properties
		{
			UPLOADS_ONLY_GROUP = 1,
			DOWNLOADS_ONLY_GROUP = 2,
			MIXED_GROUP = 3,
			CUSTOM_GROUP = 4
		};
		/**
		 * Create a new group.
		 * @param name The name of the group
		 * @param flags Properties of the group
		 */
		Group(const QString & name,int flags);
		virtual ~Group();
		
		/// See if this is a standard group.
		bool isStandardGroup() const {return !(flags & CUSTOM_GROUP);}
		
		/// Get the group flags
		int groupFlags() const {return flags;}
		
		/**
		 * Rename the group.
		 * @param nn The new name
		 */
		void rename(const QString & nn);
		
		/**
		 * Set the group icon by name.
		 * @param in The icon name
		 */
		void setIconByName(const QString & in);
		
		/// Get the name of the group
		const QString & groupName() const {return name;}
		
		/// Get the icon of the group
		const QIcon & groupIcon() const {return icon;}
		
		/// Name of the group icon
		const QString & groupIconName() const {return icon_name;}
		
		/**
		 * Save the torrents.The torrents should be save in a bencoded file.
		 * @param enc The BEncoder
		 */
		virtual void save(bt::BEncoder* enc);
		
		
		/**
		 * Load the torrents of the group from a BDictNode.
		 * @param n The BDictNode
		 */
		virtual void load(bt::BDictNode* n);
		
		
		/**
		 * Test if a torrent is a member of this group.
		 * @param tor The torrent
		 */
		virtual bool isMember(TorrentInterface* tor) = 0; 

		/**
		 * The torrent has been removed and is about to be deleted.
		 * Subclasses should make sure that they don't have dangling
		 * pointers to this torrent.
		 * @param tor The torrent
		 */
		virtual void torrentRemoved(TorrentInterface* tor);
		
		/**
		 * Subclasses should implement this, if they want to have torrents added to them.
		 * @param tor The torrent
		 */
		virtual void addTorrent(TorrentInterface* tor);
		
		/**
		 * Subclasses should implement this, if they want to have torrents removed from them.
		 * @param tor The torrent
		 */
		virtual void removeTorrent(TorrentInterface* tor);
	};

}

#endif
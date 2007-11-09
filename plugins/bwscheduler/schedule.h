/***************************************************************************
 *   Copyright (C) 2007 by Joris Guisson and Ivan Vasic                    *
 *   joris.guisson@gmail.com                                               *
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
#ifndef KTSCHEDULE_H
#define KTSCHEDULE_H

#include <QTime>
#include <QList>
#include <util/constants.h>

namespace bt
{
	class BDictNode;
}

namespace kt
{
	struct ScheduleItem
	{
		int day;
		QTime start;
		QTime end;
		bt::Uint32 upload_limit;
		bt::Uint32 download_limit;
		bool paused;
		
		ScheduleItem();
		ScheduleItem(const ScheduleItem & item);
		ScheduleItem(int day,const QTime & start,const QTime & end,bt::Uint32 upload_limit,
					 bt::Uint32 download_limit,	bool paused);
		
		bool isValid() const {return day >= 1 && day <= 7;}
		
		/**
		 * Check if this item conflicts with another
		 * @param other The other
		 * @return true If there is a conflict, false otherwise
		 */
		bool conflicts(const ScheduleItem & other) const;
		
		/**
		 * Assignment operator
		 * @param item The item to copy
		 * @return this
		 */
		ScheduleItem & operator = (const ScheduleItem & item);
		
		/**
		 * Comparison operator.
		 * @param item Item to compare
		 * @return true if the items are the same
		 */
		bool operator == (const ScheduleItem & item) const; 
	};

	/**
	 * Class which holds the schedule of one week.
	*/
	class Schedule : public QList<ScheduleItem>
	{
	public:
		Schedule();
		virtual ~Schedule();
		
		/**
		 * Load a schedule from a file.
		 * This will clear the current schedule.
		 * @param file The file to load from
		 * @throw Error When this fails
		 */
		void load(const QString & file);
		
		
		/**
		 * Save a schedule to a file.
		 * @param file The file to write to
		 * @throw Error When this fails
		 */
		void save(const QString & file);
		
		/**
		 * Add a ScheduleItem to the schedule
		 * @param item The ScheduleItem
		 * @return true upon succes, false otherwise (probably conflicts with other items)
		 */
		bool addItem(const ScheduleItem & item);
	
	private:
		bool parseItem(ScheduleItem* item,bt::BDictNode* dict);
	};

}

#endif
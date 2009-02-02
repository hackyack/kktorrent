/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#include "queuemanager.h"

#include <qstring.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <util/log.h>
#include <util/error.h>
#include <util/sha1hash.h>
#include <util/waitjob.h>
#include <util/fileops.h>
#include <util/functions.h>
#include <torrent/globals.h>
#include <torrent/torrent.h>
#include <torrent/torrentcontrol.h>
#include <interfaces/torrentinterface.h>
#include <interfaces/trackerslist.h>
#include <settings.h>

#include <climits>

using namespace bt;

namespace kt
{

	QueueManager::QueueManager() : QObject()
	{
		max_downloads = 0;
		max_seeds = 0; //for testing. Needs to be added to Settings::
		
		keep_seeding = true; //test. Will be passed from Core
		paused_state = false;
		exiting = false;
		ordering = false;
	}


	QueueManager::~QueueManager()
	{
		qDeleteAll(downloads);
	}

	void QueueManager::append(bt::TorrentInterface* tc)
	{
		downloads.append(tc);
		downloads.sort();
		connect(tc, SIGNAL(diskSpaceLow(bt::TorrentInterface*, bool)), this, SLOT(onLowDiskSpace(bt::TorrentInterface*, bool)));	
		connect(tc, SIGNAL(torrentStopped(bt::TorrentInterface*)), this, SLOT(torrentStopped(bt::TorrentInterface*)));
		connect(tc,SIGNAL(updateQueue()),this,SLOT(orderQueue()));
	}

	void QueueManager::remove(bt::TorrentInterface* tc)
	{
		paused_torrents.erase(tc);
		int index = downloads.indexOf(tc);
		if (index != -1)
			delete downloads.takeAt(index);
	}

	void QueueManager::clear()
	{
		Uint32 nd = downloads.count();
		paused_torrents.clear();
		
		// wait for a second to allow all http jobs to send the stopped event
		if (nd > 0)
			SynchronousWait(1000);
		
		qDeleteAll(downloads);
		downloads.clear();
	}
	
	TorrentStartResponse QueueManager::startInternal(bt::TorrentInterface* tc)
	{
		const TorrentStats & s = tc->getStats();
		
		if (!s.completed && !tc->checkDiskSpace(false)) //no need to check diskspace for seeding torrents
		{
			//we're short!
			switch (Settings::startDownloadsOnLowDiskSpace())
			{
			case 0: //don't start!
				return bt::NOT_ENOUGH_DISKSPACE;
			case 1: //ask user
				if (KMessageBox::questionYesNo(0, i18n("You don't have enough disk space to download this torrent. Are you sure you want to continue?"), i18n("Insufficient disk space for %1",s.torrent_name)) == KMessageBox::No)
					return bt::USER_CANCELED;
				else
					break;
			case 2: //force start
				break;
			}
		}

		Out(SYS_GEN | LOG_NOTICE) << "Starting download " << s.torrent_name << endl;
		bool max_ratio_reached = tc->overMaxRatio();
		bool max_seed_time_reached = tc->overMaxSeedTime();
		if (s.completed && (max_ratio_reached || max_seed_time_reached))
		{
			if (!enabled())
				return QM_LIMITS_REACHED;
			
			QString msg; 
			if (max_ratio_reached && max_seed_time_reached)
				msg = i18n("The torrent \"%1\" has reached it's maximum share ratio and it's maximum seed time. Ignore the limit and start seeding anyway?",s.torrent_name);
			else if (max_ratio_reached && !max_seed_time_reached)
				msg = i18n("The torrent \"%1\" has reached it's maximum share ratio. Ignore the limit and start seeding anyway?",s.torrent_name);
			else if (max_seed_time_reached && !max_ratio_reached)
				msg = i18n("The torrent \"%1\" has reached it's maximum seed time. Ignore the limit and start seeding anyway?",s.torrent_name);
				
			if (KMessageBox::questionYesNo(0, msg, i18n("Maximum share ratio limit reached.")) == KMessageBox::Yes)
			{
				if (max_ratio_reached)
					tc->setMaxShareRatio(0.00f);
				if (max_seed_time_reached)
					tc->setMaxSeedTime(0.0f);
				startSafely(tc);
			}
			else
				return USER_CANCELED;
		}
		else
			startSafely(tc);
		
		return START_OK;
	}

	TorrentStartResponse QueueManager::start(bt::TorrentInterface* tc)
	{
		if (!enabled)
		{
			return startInternal(tc);
		}
		else
		{
			tc->setAllowedToStart(true);
			bool start_tc = false;
			bool check_done = false;
			if (tc->isCheckingData(check_done) && !check_done)
				return BUSY_WITH_DATA_CHECK;
			
			const TorrentStats & s = tc->getStats();
			if (!s.completed && !tc->checkDiskSpace(false)) //no need to check diskspace for seeding torrents
			{
				//we're short!
				switch (Settings::startDownloadsOnLowDiskSpace())
				{
					case 0: //don't start!
						return bt::NOT_ENOUGH_DISKSPACE;
					case 1: //ask user
						if (KMessageBox::questionYesNo(0, i18n("You don't have enough disk space to download this torrent. Are you sure you want to continue?"), i18n("Insufficient disk space for %1",s.torrent_name)) == KMessageBox::No)
							return bt::USER_CANCELED;
						else
							break;
					case 2: //force start
						break;
				}
			}
			
			if (s.completed)
				start_tc = (max_seeds == 0 || getNumRunning(SEEDS) < max_seeds);
			else
				start_tc = (max_downloads == 0 || getNumRunning(DOWNLOADS) < max_downloads);

			if (start_tc)
				orderQueue();
			else
				return QM_LIMITS_REACHED;
			
			return START_OK;
		}
	}

	void QueueManager::stop(bt::TorrentInterface* tc)
	{
		bool check_done = false;
		if (tc->isCheckingData(check_done) && !check_done)
			return;
		
		const TorrentStats & s = tc->getStats();
		if (enabled())
			tc->setAllowedToStart(false);
		
		if (s.running)
			stopSafely(tc);
		else
			tc->updateStatus();
	}
	
	void QueueManager::stop(QList<bt::TorrentInterface*> & todo)
	{
		ordering = true;
		foreach (bt::TorrentInterface* tc,todo)
		{
			stop(tc);
		}
		ordering = false;
		if (enabled())
			orderQueue();
	}
	
	void QueueManager::checkDiskSpace(QList<bt::TorrentInterface*> & todo)
	{
		// first see if we need to ask the user to start torrents when diskspace is low
		if (Settings::startDownloadsOnLowDiskSpace() == 2)
		{
			QStringList names;
			QList<bt::TorrentInterface*> tmp;
			foreach (bt::TorrentInterface* tc,todo)
			{
				const TorrentStats & s = tc->getStats();
				if (!s.completed && !tc->checkDiskSpace(false))
				{
					names.append(s.torrent_name);
					tmp.append(tc);
				}
			}
			
			if (tmp.count() > 0)
			{
				if (KMessageBox::questionYesNoList(0,i18n("Not enough disk space for the following torrents. Do you want to start them anyway ?"),names) == KMessageBox::No)
				{
					foreach (bt::TorrentInterface* tc,tmp)
						todo.removeAll(tc);
				}
			}
		}
		// if the policy is to not start, remove torrents from todo list if diskspace is low
		else if (Settings::startDownloadsOnLowDiskSpace() == 0) 
		{
			QList<bt::TorrentInterface *>::iterator i = todo.begin();
			while (i != todo.end())		
			{
				bt::TorrentInterface* tc = *i;
				const TorrentStats & s = tc->getStats();
				if (!s.completed && !tc->checkDiskSpace(false))
					i = todo.erase(i);
				else
					i++;
			}
		}
	}
	
	void QueueManager::checkMaxSeedTime(QList<bt::TorrentInterface*> & todo)
	{
		QStringList names;
		QList<bt::TorrentInterface*> tmp;
		foreach (bt::TorrentInterface* tc,todo)
		{
			const TorrentStats & s = tc->getStats();
			if (tc->overMaxSeedTime())
			{
				names.append(s.torrent_name);
				tmp.append(tc);
			}
		}
			
		if (tmp.count() > 0)
		{
			if (KMessageBox::questionYesNoList(0,i18n("The following torrents have reached their maximum seed time. Do you want to start them anyway ?"),names) == KMessageBox::No)
			{
				foreach (bt::TorrentInterface* tc,tmp)
					todo.removeAll(tc);
			}
			else
			{
				foreach (bt::TorrentInterface* tc,tmp)
					tc->setMaxSeedTime(0.0f);
			}
		}
	}
	
	void QueueManager::checkMaxRatio(QList<bt::TorrentInterface*> & todo)
	{
		QStringList names;
		QList<bt::TorrentInterface*> tmp;
		foreach (bt::TorrentInterface* tc,todo)
		{
			const TorrentStats & s = tc->getStats();
			if (tc->overMaxRatio())
			{
				names.append(s.torrent_name);
				tmp.append(tc);
			}
		}
			
		if (tmp.count() > 0)
		{
			if (KMessageBox::questionYesNoList(0,i18n("The following torrents have reached their maximum share ratio. Do you want to start them anyway ?"),names) == KMessageBox::No)
			{
				foreach (bt::TorrentInterface* tc,tmp)
					todo.removeAll(tc);
			}
			else
			{
				foreach (bt::TorrentInterface* tc,tmp)
					tc->setMaxShareRatio(0.0f);
			}
		}
	}
	
	void QueueManager::start(QList<bt::TorrentInterface*> & todo)
	{
		if (todo.count() == 0)
			return;
		
		// check dispace stuff
		checkDiskSpace(todo);
		if (todo.count() == 0)
			return;
		
		checkMaxSeedTime(todo);
		if (todo.count() == 0)
			return;
		
		checkMaxRatio(todo);
		if (todo.count() == 0)
			return;
		
		// start what is left
		foreach (bt::TorrentInterface* tc,todo)
		{
			const TorrentStats & s = tc->getStats();
			if (s.running)
				continue;
			
			bool check_done = false;
			if (tc->isCheckingData(check_done) && !check_done)
				continue;
			
			if (enabled())
				tc->setAllowedToStart(true);
			else
				startSafely(tc);
		}
		
		if (enabled())
			orderQueue();
	}
	
	void QueueManager::startAll()
	{
		if (enabled())
		{
			foreach (bt::TorrentInterface* tc,downloads)
				tc->setAllowedToStart(true);
			
			orderQueue();
		}
		else
		{
			// first get the list of torrents which need to be started
			QList<bt::TorrentInterface*> todo;
			foreach (bt::TorrentInterface* tc,downloads)
			{
				const TorrentStats & s = tc->getStats();
				if (s.running)
					continue;
				
				bool check_done = false;
				if (tc->isCheckingData(check_done) && !check_done)
					continue;
				
				todo.append(tc);
			}
			
			start(todo);
		}
	}

	void QueueManager::stopAll()
	{
		stop(downloads);
	}
	
	void QueueManager::onExit(WaitJob* wjob)
	{
		exiting = true;
		QList<bt::TorrentInterface *>::iterator i = downloads.begin();
		while (i != downloads.end())
		{
			bt::TorrentInterface* tc = *i;
			if (tc->getStats().running)
			{
				stopSafely(tc,wjob);
			}
			i++;
		}
	}
	
	void QueueManager::startNext()
	{
		orderQueue();
	}

	int QueueManager::countDownloads()
	{
		return getNumRunning(DOWNLOADS);
	}

	int QueueManager::countSeeds()
	{
		return getNumRunning(SEEDS);
	}
	
	int QueueManager::getNumRunning(Flags flags)
	{
		int nr = 0;
		QList<TorrentInterface*>::const_iterator i = downloads.constBegin();
		while (i != downloads.constEnd())
		{
			const TorrentInterface* tc = *i;
			const TorrentStats & s = tc->getStats();
			
			if (s.running)
			{
				if (flags == ALL || (flags == DOWNLOADS && !s.completed) || (flags == SEEDS && s.completed))
					nr++;
			}
			i++;
		}
		return nr;
	}
	
	const bt::TorrentInterface* QueueManager::getTorrent(Uint32 idx) const
	{
		if (idx >= (Uint32)downloads.count())
			return 0;
		else
			return downloads[idx];
	}
	
	bt::TorrentInterface* QueueManager::getTorrent(bt::Uint32 idx)
	{
		if (idx >= (Uint32)downloads.count())
			return 0;
		else
			return downloads[idx];
	}

	QList<bt::TorrentInterface *>::iterator QueueManager::begin()
	{
		return downloads.begin();
	}

	QList<bt::TorrentInterface *>::iterator QueueManager::end()
	{
		return downloads.end();
	}

	void QueueManager::setMaxDownloads(int m)
	{
		max_downloads = m;
	}
	
	void QueueManager::onLowDiskSpace(bt::TorrentInterface* tc, bool toStop)
	{
		if (toStop)
		{
			stopSafely(tc);
			if (enabled())
			{
				tc->setAllowedToStart(false);
				orderQueue();
			}
		}
		
		//then emit the signal to inform trayicon to show passive popup
		emit lowDiskSpace(tc, toStop);
	}

	void QueueManager::setMaxSeeds(int m)
	{
		max_seeds = m;
	}
	
	void QueueManager::setKeepSeeding(bool ks)
	{
		keep_seeding = ks;
	}
	
	bool QueueManager::alreadyLoaded(const bt::SHA1Hash & ih) const
	{
		foreach (const bt::TorrentInterface* tor,downloads)
		{
			if (tor->getInfoHash() == ih)
				return true;
		}
		return false;
	}
	
	void QueueManager::mergeAnnounceList(const bt::SHA1Hash & ih,const TrackerTier* trk)
	{
		foreach (bt::TorrentInterface* tor,downloads)
		{
			if (tor->getInfoHash() == ih)
			{
				TrackersList* ta = tor->getTrackersList(); 
				ta->merge(trk);
				return;
			}
		}
	}
	
	void QueueManager::orderQueue()
	{
		if (Settings::manuallyControlTorrents())
			return;
		
		if (ordering || !downloads.count() || paused_state || exiting)
			return;
		
		ordering = true; // make sure that recursive entering of this function is not possible
		
		downloads.sort();
	
		QueuePtrList download_queue;
		QueuePtrList seed_queue;
		
		foreach (TorrentInterface* tc,downloads)
		{
			const TorrentStats & s = tc->getStats();
			bool dummy;
			if (tc->isAllowedToStart() && !tc->isMovingFiles() && !s.stopped_by_error && !tc->isCheckingData(dummy))
			{
				if (s.completed)
					seed_queue.append(tc);
				else
					download_queue.append(tc);
			}
		}
	
		int num_running = 0;
		foreach (bt::TorrentInterface* tc,download_queue)
		{
			const TorrentStats & s = tc->getStats();

			if (num_running < max_downloads || max_downloads == 0)
			{
				if (!s.running)
				{
					Out(SYS_GEN|LOG_DEBUG) << "QM Starting: " << s.torrent_name << endl;
					if (startInternal(tc) == bt::START_OK)
						num_running++;
				}
				else
					num_running++;
			}
			else
			{
				if (s.running)
				{
					Out(SYS_GEN|LOG_DEBUG) << "QM Stopping: " << s.torrent_name << endl;
					stopSafely(tc);
				}
				else
					tc->updateStatus();
			}
		}
		
		num_running = 0;
		foreach (bt::TorrentInterface* tc,seed_queue)
		{
			const TorrentStats & s = tc->getStats();

			if (num_running < max_seeds || max_seeds == 0)
			{
				if (!s.running)
				{
					Out(SYS_GEN|LOG_DEBUG) << "QM Starting: " << s.torrent_name << endl;
					if (startInternal(tc) == bt::START_OK)
						num_running++;
				}
				else
					num_running++;
			}
			else
			{
				if (s.running)
				{
					Out(SYS_GEN|LOG_DEBUG) << "QM Stopping: " << s.torrent_name << endl;
					stopSafely(tc);
				}
				else
					tc->updateStatus();
			}
		}
		
		ordering = false;
		emit queueOrdered();
	}
	
	void QueueManager::torrentFinished(bt::TorrentInterface* tc)
	{
		if (!keep_seeding)
		{
			if (enabled())
				tc->setAllowedToStart(false);
			
			stopSafely(tc);
		}
		
		orderQueue();
	}
	
	void QueueManager::torrentAdded(bt::TorrentInterface* tc,bool start_torrent)
	{
		if (enabled())
		{
			// new torrents have the lowest priority
			// so everybody else gets a higher priority
			foreach (TorrentInterface * otc,downloads)
			{
				int p = otc->getPriority();
				otc->setPriority(p+1);
			}
			tc->setAllowedToStart(start_torrent);
			tc->setPriority(0);
			rearrangeQueue();
			orderQueue();
		}
		else
		{
			 if (start_torrent)
				 start(tc);
		}	
	}
	
	void QueueManager::torrentRemoved(bt::TorrentInterface* tc)
	{
		remove(tc);
		rearrangeQueue();
		orderQueue();
	}
	
	void QueueManager::setPausedState(bool pause)
	{
		if (paused_state == pause)
			return;
		
		paused_state = pause;	
		if(!pause)
		{
			UpdateCurrentTime();
			std::set<bt::TorrentInterface*>::iterator it = paused_torrents.begin();
			while (it != paused_torrents.end())
			{
				TorrentInterface* tc = *it;
				startSafely(tc);
				it++;
			}
			
			paused_torrents.clear();
			orderQueue();
		}
		else
		{
			foreach (TorrentInterface* tc,downloads)
			{
				const TorrentStats & s = tc->getStats();
				if (s.running)
				{
					paused_torrents.insert(tc);
					stopSafely(tc,false);
				}
			}
		}
		emit pauseStateChanged(paused_state);
	}
	
	void QueueManager::rearrangeQueue()
	{
		downloads.sort();
		int prio = downloads.count();
		// make sure everybody has an unique priority
		foreach (bt::TorrentInterface* tc,downloads)
		{
			tc->setPriority(prio--);
		}
	}
	
	void QueueManager::startSafely(bt::TorrentInterface* tc)
	{
		try
		{
			tc->start();
		}
		catch (bt::Error & err)
		{
			const TorrentStats & s = tc->getStats();
			QString msg =
					i18n("Error starting torrent %1 : %2",
					s.torrent_name,err.toString());
			KMessageBox::error(0,msg,i18n("Error"));
		}
	}
	
	void QueueManager::stopSafely(bt::TorrentInterface* tc,WaitJob* wjob)
	{
		try
		{
			tc->stop(wjob);
		}
		catch (bt::Error & err)
		{
			const TorrentStats & s = tc->getStats();
			QString msg =
					i18n("Error stopping torrent %1 : %2",
					s.torrent_name,err.toString());
			KMessageBox::error(0,msg,i18n("Error"));
		}
	}
	
	void QueueManager::torrentStopped(bt::TorrentInterface* )
	{
		orderQueue();
	}
	
	static bool IsStalled(bt::TorrentInterface* tc,bt::TimeStamp now,bt::Uint32 min_stall_time)
	{
		bt::Int64 stalled_time = 0;
		if (tc->getStats().completed)
			stalled_time = (now - tc->getStats().last_upload_activity_time) / 1000;
		else
			stalled_time = (now - tc->getStats().last_download_activity_time) / 1000;
				
		return stalled_time > min_stall_time * 60 && tc->getStats().running;
	}
	
	void QueueManager::checkStalledTorrents(bt::TimeStamp now,bt::Uint32 min_stall_time)
	{
		if (!enabled())
			return;
		
		QueuePtrList newlist;
		QueuePtrList stalled;
		bool can_decrease = false;
		
		// find all stalled ones
		foreach (bt::TorrentInterface* tc,downloads)
		{
			if (IsStalled(tc,now,min_stall_time))
			{
				stalled.append(tc);
			}
			else
			{
				// decreasing makes only sense if there are QM torrents after the stalled ones
				can_decrease = stalled.count() > 0;
				newlist.append(tc);
			}
		}
		
		if (stalled.count() == 0 || stalled.count() == downloads.count() || !can_decrease)
			return;
		
		foreach (bt::TorrentInterface* tc,stalled)
			Out(SYS_GEN | LOG_NOTICE) << "The torrent " << tc->getStats().torrent_name << " has stalled longer then " << min_stall_time << " minutes, decreasing it's priority" << endl;
	
		downloads.clear();
		downloads += newlist;
		downloads += stalled;
		// redo priorities and then order the queue
		int prio = downloads.count();
		foreach (bt::TorrentInterface* tc,downloads)
		{
			tc->setPriority(prio--);
		}
		orderQueue();
	}
	/////////////////////////////////////////////////////////////////////////////////////////////

	
	QueuePtrList::QueuePtrList() : QList<bt::TorrentInterface *>()
	{}
	
	QueuePtrList::~QueuePtrList()
	{}
	
	void QueuePtrList::sort()
	{
		qSort(begin(),end(),QueuePtrList::biggerThan);
	}
	
	bool QueuePtrList::biggerThan(bt::TorrentInterface* tc1, bt::TorrentInterface* tc2)
	{
		return tc1->getPriority() > tc2->getPriority();
	}
	
}

#include "queuemanager.moc"

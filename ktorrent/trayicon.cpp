/***************************************************************************
 *   Copyright (C) 2005-2007 by                                            *
 *   Joris Guisson <joris.guisson@gmail.com>                               *
 *   Ivan Vasic <ivasic@gmail.com>                                         *
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
#include <QPainter>
#include <QtAlgorithms>
#include <klocale.h>
#include <kapplication.h>
#include <qtooltip.h>
#include <kpassivepopup.h>
#include <interfaces/torrentinterface.h>
#include <interfaces/functions.h>
#include <net/socketmonitor.h>
#include <util/log.h>
#include <settings.h>
#include <interfaces/torrentinterface.h>
//#include "trayhoverpopup.h"
#include "core.h"
#include "trayicon.h"


using namespace bt;

namespace kt
{

	TrayIcon::TrayIcon(Core* core, QWidget *parent)	: KSystemTrayIcon(parent),m_kt_pix("ktorrent")
	{
		m_core = core;
		setIcon(m_kt_pix);
		
		previousDownloadHeight=0;
		previousUploadHeight=0;
		

		connect(this,SIGNAL(quitSelected()),kapp,SLOT(quit()));
		connect(m_core, SIGNAL(finished(kt::TorrentInterface* )),
			this, SLOT(finished(kt::TorrentInterface* )));
		connect(m_core,SIGNAL(torrentStoppedByError(kt::TorrentInterface*, QString )),
			this,SLOT(torrentStoppedByError(kt::TorrentInterface*, QString )));
		connect(m_core,SIGNAL(maxShareRatioReached( kt::TorrentInterface* )),
				this,SLOT(maxShareRatioReached( kt::TorrentInterface* )));
		connect(m_core,SIGNAL(maxSeedTimeReached(kt::TorrentInterface*)),
				this, SLOT(maxSeedTimeReached(kt::TorrentInterface*)));
		connect(m_core,SIGNAL(corruptedData( kt::TorrentInterface* )),
				this,SLOT(corruptedData( kt::TorrentInterface* )));
		connect(m_core, SIGNAL(queuingNotPossible( kt::TorrentInterface* )),
				this, SLOT(queuingNotPossible( kt::TorrentInterface* )));
		connect(m_core,SIGNAL(canNotStart(kt::TorrentInterface*, kt::TorrentStartResponse)),
				this,SLOT(canNotStart(kt::TorrentInterface*, kt::TorrentStartResponse)));
		connect(m_core, SIGNAL(lowDiskSpace(kt::TorrentInterface*, bool)),
				this, SLOT(lowDiskSpace(kt::TorrentInterface*, bool)));
		
		KMenu* m = new KMenu(0);
		setContextMenu(m);
		max_upload_rate = new SetMaxRate(core,0,m);
		max_upload_rate->setTitle(i18n("Set max upload speed"));
		max_download_rate = new SetMaxRate(core,1,m);
		max_download_rate->setTitle(i18n("Set max download speed"));
		m->addMenu(max_download_rate);
		m->addMenu(max_upload_rate);
		m->addSeparator();
	}

	TrayIcon::~TrayIcon()
	{}

	void TrayIcon::updateStats(const CurrentStats stats, bool showBars,int downloadBandwidth, int uploadBandwidth )
	{
		QString tip = i18n("<table cellpadding='2' cellspacing='2' align='center'><tr><td><b>Speed:</b></td><td></td></tr><tr><td>Download: <font color='#1c9a1c'>%1</font></td><td>Upload: <font color='#990000'>%2</font></td></tr><tr><td><b>Transfer:</b></td><td></td></tr><tr><td>Download: <font color='#1c9a1c'>%3</font></td><td>Upload: <font color='#990000'>%4</font></td></tr></table>",
				KBytesPerSecToString((double)stats.download_speed/1024.0),
				KBytesPerSecToString((double)stats.upload_speed/1024.0),
				BytesToString(stats.bytes_downloaded),
				BytesToString(stats.bytes_uploaded));
		
		setToolTip(tip);	
		if(showBars)
			drawSpeedBar(stats.download_speed/1024,stats.upload_speed/1024, downloadBandwidth, uploadBandwidth);
	}

	void TrayIcon::drawSpeedBar(int downloadSpeed, int uploadSpeed, int downloadBandwidth, int uploadBandwidth )
	{
		/*
		//check if need repaint
		if (uploadBandwidth == 0)
			 uploadBandwidth = 1;
		if (downloadBandwidth == 0)
			downloadBandwidth = 1;
			
		QRect r = geometry();
		int DownloadHeight= (downloadSpeed * r.height()) / downloadBandwidth;
		int UploadHeight= (uploadSpeed * r.height()) / uploadBandwidth;
		if (previousDownloadHeight == DownloadHeight && previousUploadHeight == UploadHeight)
			return;
		
		repaint ();

		QBrush brushD(green);
		QBrush brushU(red);
		paint->fillRect(0,r.height(),2,-DownloadHeight,brushD);
		paint->fillRect(r.width()-2,r.height(),2,-UploadHeight,brushU);

		previousDownloadHeight=DownloadHeight;
		previousUploadHeight=UploadHeight;	
		*/
	}

	void TrayIcon::showPassivePopup(const QString & msg,const QString & title)
	{
		KPassivePopup* p = KPassivePopup::message(KPassivePopup::Boxed,title,msg,m_kt_pix.pixmap(100,QIcon::Normal,QIcon::On),this);
		p->setPalette(QToolTip::palette());
		p->setLineWidth(1);
	}


	void TrayIcon::finished(TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		double speed_up = (double)s.bytes_uploaded / 1024.0;
		double speed_down = (double)(s.bytes_downloaded - s.imported_bytes)/ 1024.0;

		QString msg = i18n("<b>%1</b> has completed downloading."
					"<br>Average speed: %2 DL / %3 UL.",
					s.torrent_name,
					KBytesPerSecToString(speed_down / tc->getRunningTimeDL()),
					KBytesPerSecToString(speed_up / tc->getRunningTimeUL()));

		showPassivePopup(msg,i18n("Download completed"));
	}

	void TrayIcon::maxShareRatioReached(kt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		KLocale* loc = KGlobal::locale();
		double speed_up = (double)s.bytes_uploaded / 1024.0;
		
		QString msg = i18n("<b>%1</b> has reached its maximum share ratio of %2 and has been stopped."
				"<br>Uploaded %3 at an average speed of %4.",
				s.torrent_name,
				loc->formatNumber(s.max_share_ratio,2),
				BytesToString(s.bytes_uploaded),
				KBytesPerSecToString(speed_up / tc->getRunningTimeUL()));
		
		showPassivePopup(msg,i18n("Seeding completed"));
	}

	void TrayIcon::maxSeedTimeReached(kt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		KLocale* loc = KGlobal::locale();
		double speed_up = (double)s.bytes_uploaded / 1024.0;
		
		QString msg = i18n("<b>%1</b> has reached its maximum seed time of %2 hours and has been stopped."
				"<br>Uploaded %3 at an average speed of %4.",
				s.torrent_name,
				loc->formatNumber(s.max_seed_time,2),
				BytesToString(s.bytes_uploaded),
				KBytesPerSecToString(speed_up / tc->getRunningTimeUL()));
		
		showPassivePopup(msg,i18n("Seeding completed"));
	}

	void TrayIcon::torrentStoppedByError(kt::TorrentInterface* tc, QString msg)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		QString err_msg = i18n("<b>%1</b> has been stopped with the following error: <br>%2",
					s.torrent_name,msg);
		
		showPassivePopup(err_msg,i18n("Error"));
	}

	void TrayIcon::corruptedData(kt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		QString err_msg = i18n("Corrupted data has been found in the torrent <b>%1</b>"
				"<br>It would be a good idea to do a data integrity check on the torrent.",s.torrent_name);
		showPassivePopup(err_msg,i18n("Error"));
	}

	void TrayIcon::queuingNotPossible(kt::TorrentInterface* tc)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
			
		QString msg;
		KLocale* loc = KGlobal::locale();
		 
		if (tc->overMaxRatio())
			msg = i18n("<b>%1</b> has reached its maximum share ratio of %2 and cannot be enqueued. "
				"Remove the limit manually if you want to continue seeding.",
				s.torrent_name,loc->formatNumber(s.max_share_ratio,2));
		else
			msg = i18n("<b>%1</b> has reached its maximum seed time of %2 hours and cannot be enqueued. "
				"Remove the limit manually if you want to continue seeding.",
				s.torrent_name,loc->formatNumber(s.max_seed_time,2));
		
		showPassivePopup(msg,i18n("Torrent cannot be enqueued."));
	}

	void TrayIcon::canNotStart(kt::TorrentInterface* tc,kt::TorrentStartResponse reason)
	{
		if (!Settings::showPopups())
			return;
		
		QString msg = i18n("Cannot start <b>%1</b> : <br>",tc->getStats().torrent_name);
		switch (reason)
		{
		case kt::QM_LIMITS_REACHED:
			if (tc->getStats().bytes_left_to_download == 0)
			{
				// is a seeder
				msg += i18np("Cannot seed more than 1 torrent. <br>",
							"Cannot seed more than %1 torrents. <br>",Settings::maxSeeds());
			}
			else
			{
				msg += i18np("Cannot download more than 1 torrent. <br>",
							"Cannot download more than %1 torrents. <br>",Settings::maxDownloads());
			}
			msg += i18n("Go to Settings -> Configure KTorrent, if you want to change the limits.");
			showPassivePopup(msg,i18n("Torrent cannot be started"));
			break;
		case kt::NOT_ENOUGH_DISKSPACE:
			msg += i18n("There is not enough diskspace available.");
			showPassivePopup(msg,i18n("Torrent cannot be started"));
			break;
		default:
			break;
		}
	}

	void TrayIcon::lowDiskSpace(kt::TorrentInterface * tc, bool stopped)
	{
		if (!Settings::showPopups())
			return;
		
		const TorrentStats & s = tc->getStats();
		
		QString msg = i18n("Your disk is running out of space.<br /><b>%1</b> is being downloaded to '%2'.",s.torrent_name,tc->getDataDir());
		
		if(stopped)
			msg.prepend(i18n("Torrent has been stopped.<br />"));
		
		showPassivePopup(msg,i18n("Device running out of space"));
	}
	
	void TrayIcon::updateMaxRateMenus()
	{
		max_upload_rate->update();
		max_download_rate->update();
	}



	SetMaxRate::SetMaxRate(Core* core, int t, QWidget *parent) : KMenu(parent)
	{
		m_core = core;
		type=t;
		makeMenu();
		connect(this,SIGNAL(triggered(QAction*)),this,SLOT(onTriggered(QAction* )));
	}
	SetMaxRate::~SetMaxRate()
	{}

	void SetMaxRate::makeMenu()
	{
		int rate=(type==0) ? Settings::maxUploadRate() : Settings::maxDownloadRate();
		int maxBandwidth=(rate > 0) ? rate : (type==0) ? 0 : 20 ;
		int delta = 0;
		int maxBandwidthRounded;

		addTitle(i18n("Speed limit in KB/s"));

		unlimited = addAction(i18n("Unlimited"));
		unlimited->setCheckable(true);
		unlimited->setChecked(rate == 0);

		if((maxBandwidth%5)>=3)
			maxBandwidthRounded=maxBandwidth + 5 - (maxBandwidth%5);
		else
			maxBandwidthRounded=maxBandwidth - (maxBandwidth%5);

		QList<int> values;
		for (int i = 0; i < 15; i++)
		{
			if (delta == 0)
				values.append(maxBandwidth);
			else
			{
				if ((maxBandwidth%5)!=0)
				{
					values.append(maxBandwidthRounded - delta);
					values.append(maxBandwidthRounded + delta);
				}
				else
				{
					values.append(maxBandwidth - delta);
					values.append(maxBandwidth + delta);
				}
			}

			delta += (delta >= 50) ? 50 : (delta >= 20) ? 10 : 5;
		}
		
		qSort(values);
		foreach(int v,values)
		{
			if (v >= 1)
			{
				QAction* act = addAction(QString("%1").arg(v));
				act->setCheckable(true);
				act->setChecked(rate == v);
			}
		}
	}

	void SetMaxRate::update()
	{
		clear();
		makeMenu();
	}

	void SetMaxRate::onTriggered(QAction* act)
	{
		int rate;
		if(act == unlimited)
			rate=0;
		else
			rate=act->text().toInt();

		if(type==0)
		{
			Settings::setMaxUploadRate(rate);
			net::SocketMonitor::setUploadCap( Settings::maxUploadRate() * 1024);
		}
		else
		{
			Settings::setMaxDownloadRate(rate);
			net::SocketMonitor::setDownloadCap(Settings::maxDownloadRate()*1024);
		}
		Settings::self()->writeConfig();

		update();
	}
}

#include "trayicon.moc"

/***************************************************************************
 *   Copyright (C) 2005 by Joris Guisson                                   *
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
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ***************************************************************************/
#include <stdlib.h>
#include <kurl.h>
#include <klocale.h>
#include <QTextStream>
#include <qfile.h>
#include <qlist.h>
#include <iostream>
#include <interfaces/logmonitorinterface.h>
#include <util/fileops.h>
#include <qdatetime.h>
#include <qmutex.h> 
#include "log.h"
#include "error.h"
#include "autorotatelogjob.h"


namespace bt
{
	const Uint32 MAX_LOG_FILE_SIZE = 10 * 1024 * 1024; // 10 MB
	
	class Log::Private
	{
	public:
		Log* parent;
		QTextStream* out;
		QFile* fptr;
		bool to_cout;
		QList<LogMonitorInterface *> monitors;
		QString tmp;
		QMutex mutex;
		unsigned int m_filter;
		AutoRotateLogJob* rotate_job;
	public:
		Private(Log* parent) : parent(parent),out(0),fptr(0),to_cout(false),rotate_job(0)
		{
		}

		~Private()
		{
			cleanup();
		}

		void cleanup()
		{
			if (out)
			{
				delete out;
				out = 0;
			}

			if (fptr)
			{
				delete fptr;
				fptr = 0;
			}
		}
		
		void setFilter(unsigned int filter)
		{
			m_filter = filter;
		}
		
		void rotateLogs(const QString & file)
		{
			if (bt::Exists(file + "-10.gz"))
				bt::Delete(file + "-10.gz",true);
			
			// move all log files one up
			for (Uint32 i = 10;i > 1;i--)
			{
				QString prev = QString("%1-%2.gz").arg(file).arg(i - 1);
				QString curr = QString("%1-%2.gz").arg(file).arg(i);
				if (bt::Exists(prev))
					bt::Move(prev,curr,true);
			}
			
			// move current log to 1 and zip it
			bt::Move(file,file + "-1",true);
			system(QString("gzip %1-1").arg(file).toLocal8Bit());
		}

		void setOutputFile(const QString & file,bool rotate)
		{
			cleanup();
			
			if (bt::Exists(file) && rotate)
				rotateLogs(file);

			fptr = new QFile(file);
			if (!fptr->open(QIODevice::WriteOnly))
			{
				QString err = fptr->errorString();
				cleanup();
				throw Error(i18n("Cannot open log file %1 : %2",file,err));
			}

			out = new QTextStream(fptr);
		}

		void write(const QString & line)
		{
			tmp += line;
		}
		
		void finishLine()
		{
			// only add stuff when we are not rotating the logs
			// this could result in the loss of some messages
			if (!rotate_job)
			{
				QString final = QDateTime::currentDateTime().toString() + ": " + tmp;
				*out << final << ::endl;
				fptr->flush();
				if (to_cout)
					std::cout << final.toLocal8Bit().constData() << std::endl;;
				
				if (monitors.count() > 0)
				{
					QList<LogMonitorInterface *>::iterator i = monitors.begin();
					while (i != monitors.end())
					{
						LogMonitorInterface* lmi = *i;
						lmi->message(final,m_filter);
						i++;
					}
				}
			}
			tmp = "";
		}

		void endline()
		{
			finishLine();
			if (fptr->size() > MAX_LOG_FILE_SIZE && !rotate_job)
			{
				tmp = "Log larger then 10 MB, rotating";
				finishLine();
				QString file = fptr->fileName();
				fptr->close(); // close the log file
				out->setDevice(0);
				// start the rotate job
				rotate_job = new AutoRotateLogJob(file,parent);
			}
		}

		void logRotateDone()
		{
			fptr->open(QIODevice::WriteOnly);
			out->setDevice(fptr);
			rotate_job = 0;
		}
	};
	
	Log::Log() 
	{
		priv = new Private(this);
	}
	
	
	Log::~Log()
	{
		delete priv;
	}
	
	
	void Log::setOutputFile(const QString & file,bool rotate)
	{
		priv->setOutputFile(file,rotate);
	}

	void Log::addMonitor(LogMonitorInterface* m)
	{
		priv->monitors.append(m);
	}

	void Log::removeMonitor(LogMonitorInterface* m)
	{
		int index = priv->monitors.indexOf(m);
    		if (index != -1)
         		delete priv->monitors.takeAt(index);		
	}

	void Log::setOutputToConsole(bool on)
	{
		priv->to_cout = on;
	}
	
	void Log::logRotateDone()
	{
		priv->logRotateDone();
	}

	Log & endl(Log & lg)
	{
		lg.priv->endline();
		lg.priv->mutex.unlock(); // unlock after end of line
		return lg;
	}

	Log & Log::operator << (const KUrl & url)
	{
		priv->write(url.prettyUrl());
		return *this;
	}

	Log & Log::operator << (const QString & s)
	{
		priv->write(s);
		return *this;
	}

	Log & Log::operator << (const char* s)
	{
		priv->write(s);
		return *this;
	}

	Log & Log::operator << (Uint64 v)
	{
		return operator << (QString::number(v));
	}

	Log & Log::operator << (Int64 v)
	{
		return operator << (QString::number(v));
	}

	void Log::setFilter(unsigned int filter)
	{
		priv->setFilter(filter);
	}

	void Log::lock()
	{
		priv->mutex.lock();
	}

	static Log global_log;
	
	Log & Out(unsigned int arg)
	{
		global_log.setFilter(arg);
		global_log.lock();
		return global_log;
	}

	void InitLog(const QString & file,bool rotate)
	{
		global_log.setOutputFile(file,rotate);
	}

	void AddLogMonitor(LogMonitorInterface* m)
	{
		global_log.addMonitor(m);
	}

	void RemoveLogMonitor(LogMonitorInterface* m)
	{
		global_log.removeMonitor(m);
	}
}
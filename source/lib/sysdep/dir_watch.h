/* Copyright (C) 2009 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * =========================================================================
 * File        : dir_watch.h
 * Project     : 0 A.D.
 * Description : portable directory change notification API.
 * =========================================================================
 */

#ifndef INCLUDED_DIR_WATCH
#define INCLUDED_DIR_WATCH

class DirWatch;
typedef shared_ptr<DirWatch> PDirWatch;

/**
 * start watching a single directory for changes.
 *
 * @param path native path of the directory to watch.
 * @param dirWatch receives a smart pointer to the watch object.
 *
 * note: the FAM backend can only watch single directories, so that is
 * all we can guarantee. the Win32 implementation watches entire trees;
 * adding a watch for subdirectories is a no-op there.
 **/
LIB_API LibError dir_watch_Add(const fs::wpath& path, PDirWatch& dirWatch);

/**
 * stop watching a directory.
 *
 * note: any previously queued notifications will still be returned.
 **/
LIB_API void dir_watch_Remove(PDirWatch& dirWatch);

class DirWatchNotification
{
public:
	enum Event
	{
		Created,
		Deleted,
		Changed
	};

	// (default ctor is required because DirWatchNotification is returned
	// via output parameter.)
	DirWatchNotification()
	{
	}

	DirWatchNotification(const fs::wpath& pathname, Event type)
		: m_pathname(pathname), m_type(type)
	{
	}

	const fs::wpath& Pathname() const
	{
		return m_pathname;
	}

	Event Type() const
	{
		return m_type;
	}

	static const char* EventString(Event type)
	{
		switch(type)
		{
		case Created:
			return "created";
		case Deleted:
			return "deleted";
		case Changed:
			return "changed";
		default:
			throw std::logic_error("invalid type");
		}
	}

private:
	fs::wpath m_pathname;
	Event m_type;
};

/**
 * check if a directory watch notification is pending.
 *
 * @param notification receives the first pending DirWatchNotification from
 * any of the watched directories. this notification is subsequently removed
 * from the internal queue.
 * @return INFO::OK if a notification was retrieved, ERR::AGAIN if none
 * are pending, or a negative error code.
 *
 * note: the run time of this function is independent of the number of
 * directory watches and number of files.
 *
 * rationale for a polling interface: users (e.g. the main game loop)
 * typically want to receive change notifications at a single point,
 * rather than deal with the complexity of asynchronous notifications.
 **/
LIB_API LibError dir_watch_Poll(DirWatchNotification& notification);

#endif	// #ifndef INCLUDED_DIR_WATCH

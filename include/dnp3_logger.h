#ifndef _LOGGER_DNP3_H
#define _LOGGER_DNP3_H

/*
 * Fledge DNP3 logger class
 *
 * Copyright (c) 2024 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */

#include <openpal/logging/ILogHandler.h>
#include <openpal/util/Uncopyable.h>

using namespace std;
using namespace std::chrono;

namespace asiodnp3
{

std::ostringstream& operator<<(std::ostringstream& ss, const openpal::LogFilters& filters);

/**
 * LogHandler that sends all log messages to Fledge logger system
 */
class Dnp3Logger final : public openpal::ILogHandler, private openpal::Uncopyable
{
	public:
		virtual void Log(const openpal::LogEntry& entry) override
		{
			ostringstream oss;
			// Get log level from passd object
			uint32_t logLevel = entry.filters.GetBitfield();

			oss << entry.loggerid;

			// Add file C info for debug/event and unknown levels
			if (m_printLocation &&
			    (logLevel != flags::ERR &&
			     logLevel != flags::WARN &&
			     logLevel != flags::INFO))
			{
				oss << " - " << entry.location;
			}

			oss << " - " << entry.message;

			switch (logLevel)
			{
				case flags::ERR:
					Logger::getLogger()->error(oss.str().c_str());
					break;
				case flags::WARN:
					Logger::getLogger()->warn(oss.str().c_str());
					break;
				case flags::INFO:
					Logger::getLogger()->info(oss.str().c_str());
					break;
				case flags::DBG:
				case flags::EVENT:
					Logger::getLogger()->debug(oss.str().c_str());
					break;
				default:
					// Unkwon level, log as debug
					Logger::getLogger()->debug("%s - %s",
							LogFlagToString(logLevel),
							oss.str().c_str());
					break;
			}
		};

		static std::shared_ptr<openpal::ILogHandler> Create(bool printLocation = false)
		{
			return std::make_shared<Dnp3Logger>(printLocation);
		};

		Dnp3Logger(bool printLocation) : m_printLocation(printLocation) {};

	private:
		bool m_printLocation;
};

} // namespace asiodnp3

#endif

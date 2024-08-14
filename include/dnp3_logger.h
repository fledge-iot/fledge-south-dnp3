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

// Outstation channel state listener override class
class DNP3ChannelListener : public IChannelListener
{
public:
	virtual void OnStateChange(opendnp3::ChannelState state) override
	{
		const char *s = opendnp3::ChannelStateToString(state);
		Logger::getLogger()->info("Outstation id %d: channel state change for %s:%d is '%s'",
					m_outstation->linkId,
					m_outstation->address.c_str(),
					m_outstation->port,
				s);
	}

	// Pass OutStationTCP pointer
	static std::shared_ptr<DNP3ChannelListener> Create(const DNP3::OutStationTCP *o)
	{
		Logger::getLogger()->debug("DNP3ChannelListener::Create() called");
		return std::make_shared<DNP3ChannelListener>(o);
	}

	DNP3ChannelListener(const DNP3::OutStationTCP *o)
	{
		m_outstation = (DNP3::OutStationTCP *)o;
	}
private:
	DNP3::OutStationTCP	*m_outstation;
};

// Master application override class
class DNP3MasterApplication : public IMasterApplication
{
public:
	DNP3MasterApplication() {} // Emoty constructor
	static std::shared_ptr<DNP3MasterApplication> Create() // Return default class
	{
		Logger::getLogger()->debug("DNP3MasterApplication::Create() called");
		return std::make_shared<DNP3MasterApplication>();
	}

	// Pass IMaster pointer and OutStationTCP pointer
	void AddMaster(std::shared_ptr<IMaster> mp, const DNP3::OutStationTCP *o)
	{
		m = mp;
		m_outstation = (DNP3::OutStationTCP *)o;
		Logger::getLogger()->debug("DNP3MasterApplication::AddMaster() called");
	}

   	// Set here methods overridden in DefaultMasterApplication (set to override final)
	// opendnp3 library
	// cpp/libs/src/asiodnp3/DefaultMasterApplication.cpp
	//
	// As in DefaultMasterApplication
	virtual bool AssignClassDuringStartup() override final
	{
		return false;
	}

	// As in DefaultMasterApplication
	virtual openpal::UTCTimestamp Now() override final
	{
		uint64_t time
		= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		return {time};
	}

	// Report master link change
	virtual void OnStateChange(opendnp3::LinkStatus value) override
	{
		const char *s = opendnp3::LinkStatusToString(value);
		Logger::getLogger()->debug("Master link state change: %s", s);
	}

	// Override OnKeepAliveFailure and close and open again connection to outstation
	// This covers scenario of disconnected network cable or powering off the network switch
	virtual void OnKeepAliveFailure() override
	{
		Logger::getLogger()->error("Master detected KeepAlive failure for " \
					"outstation %s:%d id %d, " \
					"restarting connection ...",
					m_outstation->address.c_str(),
					m_outstation->port,
					m_outstation->linkId);

		// Close connection to outstation
		m->Disable();

		// Open new connection to outstation
		// If outstation is still down or not reachable
		// the connection task will continue trying
		m->Enable();
	}
private:
	std::shared_ptr<IMaster> m;
	DNP3::OutStationTCP	*m_outstation;
};

} // namespace asiodnp3

#endif

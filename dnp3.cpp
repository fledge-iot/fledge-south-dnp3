/*
 * FogLAMP DNP3 class.
 *
 * Copyright (c) 2019 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */
#include <plugin_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <logger.h>
#include <plugin_exception.h>
#include <config_category.h>
#include <rapidjson/document.h>
#include <version.h>
#include <string>
#include <reading.h>
#include <logger.h>
#include <mutex>
#include <iostream>
#include <thread>

#include "south_dnp3.h"

using namespace std;
using namespace asiodnp3;
using namespace openpal;
using namespace asiopal;
using namespace opendnp3;

/**
 * Start the DNP3 master and connect to configured outstation
 *
 * @return	True on success, false otherwise
 */
bool DNP3::start()
{
	this->lockConfig();

	// Save configuration items
	int nThreads = m_outstations.size();
	uint16_t masterId = this->getMasterLinkId();
	bool pollEnabled = this->isPollEnabled();
	unsigned long applicationTimeout = this->getTimeout();
	unsigned long pollInterval = this->getOutstationPollInterval();
	// We currently handle one outstation only
	OutStationTCP outstation = m_outstations[0];

	// Create DNP3 manager object
	// Set threads and console logging
	asiodnp3::DNP3Manager* manager = 
		new asiodnp3::DNP3Manager(nThreads,
					  asiodnp3::ConsoleLogger::Create());
	m_manager = manager;

	this->unlockConfig();

	string remoteLabel = "remote_" + to_string(outstation.linkId);

	// Set log levels
	const auto logLevels = levels::NOTHING | flags::WARN | flags::ERR;

	// Create TCP channel for outstation
	std::shared_ptr<IChannel> channel =
		manager->AddTCPClient(m_serviceName + "_" + remoteLabel, // alias in log messages
				      logLevels, // filter what gets logged
				      ChannelRetry::Default(), // how connections will be retried
				      // host names or IP address of remote endpoint
				      // with port remote endpoint is listening on
				      {IPEndpoint(outstation.address, outstation.port)},
				      // adapter on which to attempt the connection (any adapter)
				      "0.0.0.0",
				      // optional listener interface for monitoring the channel
				      PrintingChannelListener::Create());

	if (!channel)
	{
		return false;
	}

	Logger::getLogger()->info("configured DNP3 TCP outstation is: %s:%d, Link Id %d",
				  outstation.address.c_str(),
				  outstation.port,
				  outstation.linkId);

	// This object contains static configuration for the master, and transport/link layers
	MasterStackConfig stackConfig;

	// you can optionally override these defaults like:
	// setting the application layer response timeout
	// or change behaviors on the master
	stackConfig.master.responseTimeout =
		TimeDuration::Seconds(applicationTimeout);

	// Don't poll outstation at startup
	stackConfig.master.startupIntegrityClassMask = ClassField::None(); ;

	// ... or you can override the default link layer settings
	stackConfig.link.LocalAddr = masterId;
	stackConfig.link.RemoteAddr = outstation.linkId;

	// Custom SOEHandler object
	std::shared_ptr<ISOEHandler> SOEHandle =
		std::make_shared<dnp3SOEHandler>(this, remoteLabel);
	if (!SOEHandle)
	{
		return false;
	}

	// Create a master bound to a particular channel
	std::shared_ptr<IMaster> master =
		channel->AddMaster("master_" + to_string(masterId), // alias for logging
				   SOEHandle,  // IOEHandler (interface)
				   asiodnp3::DefaultMasterApplication::Create(), // Application (interface)
				   stackConfig  // static stack configuration
        );

	if (!master)
	{
		return false;
	}

	// do an integrity poll (Class 3/2/1/0) once per specified seconds
	if (pollEnabled)
	{
		Logger::getLogger()->info("Outstation poll is enabled");
		master->AddClassScan(ClassField::AllClasses(),
				     TimeDuration::Seconds(pollInterval));
	}

	// Enable the DNP3 master and connect to outstation
	return master->Enable();
}

/**
 * Set plugin configuration
 *
 * @param	config	The configuration category
 * @return		True on success, false on errors
 */
bool DNP3::configure(ConfigCategory* config)
{
	this->lockConfig();

        m_outstations.clear();

	DNP3::OutStationTCP outstation;
                
	if (config->itemExists("asset"))
	{
		this->setAssetName(config->getValue("asset"));
	}
	else    
	{
		this->setAssetName(DEFAULT_ASSETNAME_PREFIX);
	}
        
	if (config->itemExists("master_id"))
	{
		this->setMasterLinkId((uint16_t)atoi(config->getValue("master_id").c_str()));
	}
	else    
	{       
		this->setMasterLinkId((uint16_t)atoi(DEFAULT_MASTER_LINK_ID));
	}
	if (config->itemExists("outstation_tcp_address"))
	{
		// Overrides address
		outstation.address = config->getValue("outstation_tcp_address");
	}
	if (config->itemExists("outstation_tcp_port"))
	{
		// Overrides port
		outstation.port = (unsigned short int)atoi(config->getValue("outstation_tcp_port").c_str());
	}

	bool enablePoll = config->itemExists("outstation_poll_enable") &&
			 (config->getValue("outstation_poll_enable").compare("true") == 0 ||
			  config->getValue("outstation_poll_enable").compare("True") == 0);

	this->enablePoll(enablePoll);

	if (config->itemExists("outstation_poll_interval"))
	{
		this->setOutstationPollInterval(atol(config->getValue("outstation_poll_interval").c_str()));
	}

	if (config->itemExists("data_fetch_timeout"))
	{
		this->setTimeout(atol(config->getValue("data_fetch_timeout").c_str()));
	}

	if (config->itemExists("outstation_id"))
	{
		// Overrides link id
		outstation.linkId = (uint16_t)atoi(config->getValue("outstation_id").c_str());
	}

	// Add this outstation to teh object
	this->addOutStationTCP(outstation);

	this->unlockConfig();

	return true;
}

/**
 * Data callback for solicited and usolicited messagess
 * from outstation
 *
 * For each element a routine is called which
 * ingest data into FogLAMP
 *
 * @param    info	HeaderInfo structure
 * @param    valueis	Indexed Object<T> values
 * @param    objectType	The object type
 */
template<class T> void
	dnp3SOEHandler::dnp3DataCallback(const HeaderInfo& info,
					 const ICollection<Indexed<T>>& values,
					 const string& objectType)
{       
	Logger::getLogger()->debug("Callback for outstation (%s) data: "
				   "object type '%s', # of elements",
				   m_label.c_str(),
				   objectType.c_str(),
				   values.Count());

	// Lambda function for data element
	auto processData = [&](const Indexed<T>& pair)
	{
		this->dataElement<T>(info,
				     pair.value,
				     pair.index,
				     objectType);
	};

	// Process all elements
	values.ForeachItem(processData);
}

/**
 * Process a data element from callback
 *
 * This routine ingests data in FogLAMP
 *
 * @param    info	HeaderInfo structure
 * @param    value	Object<T> value
 * @param    index	Index value of this data
 * @param    objectType	The object type
 */
template<class T> void
	dnp3SOEHandler::dataElement(const HeaderInfo& info,
				    const T& value,
				    uint16_t index,
				    const std::string& objectType)
{
	Logger::getLogger()->debug("callback for %s, object %s[%d], isEvent %d, "
				   "flagsValid %d, flags %d, value %s, time %lu",
				   m_label.c_str(),
				   objectType.c_str(),
				   index,
				   info.isEventVariation,
				   info.flagsValid,
				   static_cast<int>(value.flags.value),
				   ValueToString(value).c_str(),
				   value.time.value);

	if (m_dnp3)
	{
		bool event = info.isEventVariation == true;
		int flag = static_cast<int>(value.flags.value);
		bool isBinary = objectType.compare("Binary") == 0 ||
				objectType.compare("BinaryOutputStatus") == 0;

		// 0x01 means ONLINE for all Objects
		// STATE is checked for Binary and BinaryOutputStatus objects
		if (flag == ONLINE_FLAG_ALL_OBJECTS ||
		    (isBinary &&
		     (flag & static_cast<uint8_t>(BinaryQuality::STATE))))
		{
			std::vector<Datapoint *> points;
			DatapointValue dVal(ValueToString(value));
			// Datapoint name = objectType + index
			// Example: Counter0, Counter1
			std::string dataPointName = objectType + std::to_string(index);
			points.push_back(new Datapoint(dataPointName, dVal));

			// Asset name name = im_label + objectType + _ + index
			// Example: remote_20_Binary_0
			std::string assetName = m_label + "_" + \
						objectType + "_"  + \
						std::to_string(index);

			// Ingest data in FogLAMP
			m_dnp3->ingest(assetName, points);
		}
	}
}


/*
 * Fledge DNP3 class.
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
#include "rapidjson/error/error.h"
#include "rapidjson/error/en.h"
#include <version.h>
#include <string>
#include <reading.h>
#include <logger.h>
#include <mutex>
#include <iostream>
#include <thread>

#include "south_dnp3.h"
#include "dnp3_logger.h" // Include logging and application overrides

using namespace std;
using namespace asiodnp3;
using namespace openpal;
using namespace asiopal;
using namespace opendnp3;

using namespace rapidjson;

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
	bool scanEnabled = this->isScanEnabled();
	unsigned long applicationTimeout = this->getTimeout();
	unsigned long scanInterval = this->getOutstationScanInterval();
	uint32_t logLevels = this->getAppLogLevel();

	// Create DNP3 manager object
	// Set threads and console logging
	asiodnp3::DNP3Manager* manager = 
		new asiodnp3::DNP3Manager(nThreads,
					  asiodnp3::Dnp3Logger::Create(true)); // true for file an line reference in debug
	m_manager = manager;

	this->unlockConfig();

	Logger::getLogger()->info("Found %d DNP3 TCP outstation configured", m_outstations.size());

	// Iterate outstation array
	for (OutStationTCP *outstation : m_outstations)
	{
		string remoteLabel = "remote_" + to_string(outstation->linkId);

		// Connection retry timings: staring with 20 seconds, then up to 5 minutes
		auto retry = ChannelRetry(TimeDuration::Seconds(20), TimeDuration::Minutes(5));

		// Create TCP channel for outstation
		std::shared_ptr<IChannel> channel =
			manager->AddTCPClient(m_serviceName + "_" + remoteLabel, // alias in log messages
				      logLevels, // filter what gets logged
				      retry, // how connections will be retried
				      // host names or IP address of remote endpoint
				      outstation->address, 
				      // interface adapter on which to attempt the connection (any adapter)
				      "0.0.0.0",
				      // wich port the remote endpoint is listening on
				      outstation->port,
				      // optional listener interface for monitoring the channel of outstation
				      asiodnp3::DNP3ChannelListener::Create(outstation));

		if (!channel)
		{
			return false;
		}

		Logger::getLogger()->info("configured DNP3 TCP outstation is: %s:%d, Link Id %d",
					  outstation->address.c_str(),
					  outstation->port,
					  outstation->linkId);

		// This object contains static configuration for the master, and transport/link layers
		MasterStackConfig stackConfig;

		// you can optionally override these defaults like:
		// setting the application layer response timeout
		// or change behaviors on the master
		stackConfig.master.responseTimeout = TimeDuration::Seconds(applicationTimeout);

		// Don't perform Integrity Poll outstation at startup
		stackConfig.master.startupIntegrityClassMask = ClassField::None(); ;

		// Override the default link layer settings
		stackConfig.link.LocalAddr = masterId;  // Master id link
		stackConfig.link.RemoteAddr = outstation->linkId; // Outstation id link

		// Custom SOEHandler object for callback
		std::shared_ptr<ISOEHandler> SOEHandle =
			std::make_shared<dnp3SOEHandler>(this, remoteLabel);
		if (!SOEHandle)
		{
			return false;
		}

		// Create custom MasterApplication
		auto ma = DNP3MasterApplication::Create();

		// Create a master bound to a particular channel
		std::shared_ptr<IMaster> master =
				channel->AddMaster("master_" + to_string(masterId), // alias for logging
				SOEHandle,  // IOEHandler (interface)
				ma, // Application (interface)
				stackConfig); // static stack configuration

		// Check
		if (!master)
		{
			return false;
		}

		// Pass master and outstation to custom MasterApplication
		ma->AddMaster(master, outstation);

		// Do an integrity poll (Class 3/2/1/0) once per specified seconds
		if (scanEnabled)
		{
			Logger::getLogger()->info("Outstation id %d scan (Integrity Poll) is enabled",
						outstation->linkId);

			master->AddClassScan(ClassField::AllClasses(),
					     TimeDuration::Seconds(scanInterval));
		}

		// Enable the DNP3 master and connect to outstation
		if (!master->Enable())
		{
			return false;
		}
	}

	// Success
	return true;
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

	auto it = m_outstations.begin();
	while (it != m_outstations.end())
	{
		it = m_outstations.erase(it);
	}

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

	bool oneOutstation = true;
	if (config->itemExists("outstations") && config->isList("outstations"))
	{
		string outstations = config->getValue("outstations");
		Document document;
		if (document.Parse(outstations.c_str()).HasParseError())
		{
			Logger::getLogger()->error("Error while parsing '%s' type 'list' item: %s",
					"outstations",
					GetParseError_En(document.GetParseError()));
			return false;
		}
		if (!document.IsArray())
		{
			Logger::getLogger()->error("Error '%s' type 'list' item is not an array",
					"outstations");
			return false;
		}
		for (auto& o : document.GetArray())
		{
			if (!o.IsObject())
			{
				Logger::getLogger()->error("Error '%s' type 'list': array element is not an object",
						"outstations");
				return false;
			}
			DNP3::OutStationTCP *outstation = new DNP3::OutStationTCP();
			for (auto& v : o.GetObject())
                        {
				string key = v.name.GetString();
				string value = config->to_string(v.value);
				if (key == "address")
				{
					outstation->address = value;
				}
				if (key == "port")
				{
					outstation->port = (unsigned short int)atoi(value.c_str());
				}
				if (key == "linkid")
				{
					outstation->linkId = (uint16_t)atoi(value.c_str());
				}
                        }

			// Add this outstation to the array
			this->addOutStationTCP(outstation);

			oneOutstation = false;
		}
		if (!oneOutstation)
		{
			Logger::getLogger()->warn("Using configuration in 'outstations' list item, " \
						  "ignoring the single outstation configuration ('outstation_id', " \
						  "'outstation_tcp_address' and 'outstation_tcp_port')");
		}
	}

	if (oneOutstation)
	{
		DNP3::OutStationTCP *outstation = new DNP3::OutStationTCP();
		if (config->itemExists("outstation_id"))
		{
			// Overrides link id
			outstation->linkId = (uint16_t)atoi(config->getValue("outstation_id").c_str());
		}
		if (config->itemExists("outstation_tcp_address"))
		{
			// Overrides address
			outstation->address = config->getValue("outstation_tcp_address");
		}
		if (config->itemExists("outstation_tcp_port"))
		{
			// Overrides port
			outstation->port = (unsigned short int)atoi(config->getValue("outstation_tcp_port").c_str());
		}
		// Add this outstation to the array
		this->addOutStationTCP(outstation);
	}

	bool enableScan = config->itemExists("outstation_scan_enable") &&
			 (config->getValue("outstation_scan_enable").compare("true") == 0 ||
			  config->getValue("outstation_scan_enable").compare("True") == 0);

	this->enableScan(enableScan);

	if (config->itemExists("outstation_scan_interval"))
	{
		this->setOutstationScanInterval(atol(config->getValue("outstation_scan_interval").c_str()));
	}

	if (config->itemExists("data_fetch_timeout"))
	{
		this->setTimeout(atol(config->getValue("data_fetch_timeout").c_str()));
	}

	if (config->itemExists("appLogLevel"))
	{
	        int32_t logLevels = levels::NOTHING;

		string appLevel = config->getValue("appLogLevel");
		if (appLevel == "" || appLevel == "Normal")
		{
			logLevels = levels::NORMAL;
		}
		if (appLevel == "Data")
		{
			logLevels = levels::NORMAL | levels::ALL_APP_COMMS;
		}
		if (appLevel == "DataAndLink")
		{
			logLevels = levels::NORMAL | levels::ALL_COMMS;
		}
		if (appLevel == "All")
		{
			logLevels = levels::ALL;
		}
		this->setAppLogLevel(logLevels);
	}

	this->unlockConfig();

	return true;
}

/**
 * Data callback for solicited and usolicited messagess
 * from outstation
 *
 * For each element a routine is called which
 * ingest data into Fledge
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
				   "object type '%s', # of elements %d",
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
 * This routine ingests data in Fledge
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
			if (objectType.compare("Analog") == 0 ||
			    objectType.compare("AnalogOutput") == 0)
			{
				double v = strtod(ValueToString(value).c_str(), NULL);
				DatapointValue dVal(v);
				// Datapoint name = objectType + index
				// Example: Counter0, Counter1
				std::string dataPointName = objectType + std::to_string(index);
				points.push_back(new Datapoint(dataPointName, dVal));
			}
			else if (isBinary ||
				 objectType.compare("Counter") == 0)
			{
				long v = strtol(ValueToString(value).c_str(), NULL, 10);
				DatapointValue dVal(v);
				// Datapoint name = objectType + index
				// Example: Counter0, Counter1
				std::string dataPointName = objectType + std::to_string(index);
				points.push_back(new Datapoint(dataPointName, dVal));
			}

			if (points.size() > 0)
			{
				// Asset name name = im_label + objectType + _ + index
				// Example: remote_20_Binary_0
				std::string assetName = m_label + "_" + \
							objectType + "_"  + \
							std::to_string(index);

				// Ingest data in Fledge
				m_dnp3->ingest(assetName, points);
			}
		}
	}
}

/**
 * Data callback for DoubleBitBinary solicited and usolicited messagess
 * from outstation
 *
 * For each element a routine is called which
 * ingest data into Fledge
 *
 * @param    info	HeaderInfo structure
 * @param    valueis	Indexed Object<T> values
 * @param    objectType	The object type
 */
template<class T> void
	dnp3SOEHandler::dnp3DataCallbackDBB(const HeaderInfo& info,
					 const ICollection<Indexed<T>>& values,
					 const string& objectType)
{       
	Logger::getLogger()->debug("DoubleBitBinary Callback for outstation (%s) data: "
				   "object type '%s', # of elements %d",
				   m_label.c_str(),
				   objectType.c_str(),
				   values.Count());
	// Lambda function for data element
	auto processData = [&](const Indexed<T>& pair)
	{
		this->dataElementDBB<T>(info,
				     pair.value,
				     pair.index,
				     objectType);
	};

	// Process all elements
	values.ForeachItem(processData);
}

/**
 * Process a DoubleBitBinary data element from callback
 *
 * This routine ingests data in Fledge
 *
 * @param    info	HeaderInfo structure
 * @param    value	Object<T> value
 * @param    index	Index value of this data
 * @param    objectType	The object type
 */
template<class T> void
	dnp3SOEHandler::dataElementDBB(const HeaderInfo& info,
				    const T& value,
				    uint16_t index,
				    const std::string& objectType)
{

	Logger::getLogger()->debug("DoubleBitBinary callback for %s, object %s[%d], isEvent %d, "
				   "flagsValid %d, flags %d, value %s, time %lu",
				   m_label.c_str(),
				   objectType.c_str(),
				   index,
				   info.isEventVariation,
				   info.flagsValid,
				   static_cast<int>(value.flags.value),
				   ValueToStringDBB(value).c_str(),
				   value.time.value);


	if (m_dnp3)
	{
		bool event = info.isEventVariation == true;
		int flag = static_cast<int>(value.flags.value);

		// 0x01 means ONLINE for all Objects
		// STATE is checked for DoubleBitBinary object
		if (flag & ONLINE_FLAG_ALL_OBJECTS)
		{
			std::vector<Datapoint *> points;
			if (objectType.compare("DoubleBitBinary") == 0)
			{
				// Convert value to string
				string v = ValueToStringDBB(value);
				DatapointValue dVal(v);
				// Datapoint name = objectType + index
				// Example: Counter0, Counter1
				std::string dataPointName = objectType + std::to_string(index);
				points.push_back(new Datapoint(dataPointName, dVal));

				// Asset name name = im_label + objectType + _ + index
				// Example: remote_20_Binary_0
				std::string assetName = m_label + "_" + \
							objectType + "_"  + \
							std::to_string(index);

				// Ingest data in Fledge
				m_dnp3->ingest(assetName, points);
			}
		}
	}
}

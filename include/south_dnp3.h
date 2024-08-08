#ifndef _DNP3_H
#define _DNP3_H
/*
 * Fledge DNP3 class
 *
 * Copyright (c) 2019-2024 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Massimiliano Pinto
 */
#include <string>
#include <reading.h>
#include <logger.h>
#include <mutex>
#include <vector>

#include <asiodnp3/ConsoleLogger.h>
#include <asiodnp3/DNP3Manager.h>
#include <asiodnp3/PrintingChannelListener.h>
#include <asiodnp3/DefaultMasterApplication.h>
#include <asiodnp3/PrintingSOEHandler.h>
#include <opendnp3/LogLevels.h>
#include <opendnp3/outstation/IUpdateHandler.h>
#include <opendnp3/outstation/SimpleCommandHandler.h>
#include <opendnp3/master/ISOEHandler.h>

#define DEFAULT_APPLICATION_TIMEOUT 		"5" // seconds
#define DEFAULT_MASTER_LINK_ID 			"1"
#define DEFAULT_TCP_ADDR      			"127.0.0.1"
#define DEFAULT_TCP_PORT      			"20000"   
#define DEFAULT_OUTSTATION_ID			"10"
#define DEFAULT_OUTSTATION_SCAN_INTERVAL	"30" // seconds
#define DEFAULT_ASSETNAME_PREFIX		"dnp3_"

#define ONLINE_FLAG_ALL_OBJECTS			0x01
// DNP3 class for DNP3 Fledge South plugin
class DNP3
{
	public:
		// This class describes the TCP outstation (remote endpoint)
		class OutStationTCP
		{
			public:
				OutStationTCP()
				{
					// Set default values
					address = DEFAULT_TCP_ADDR;
					port = (short unsigned int)atoi(DEFAULT_TCP_PORT);
					linkId = (uint16_t)atoi(DEFAULT_OUTSTATION_ID);
				};
				std::string		address;
				short unsigned int	port;
				uint16_t		linkId;
		};

	public:
		DNP3(const std::string& name) : m_serviceName(name)
		{
			m_manager = NULL;     // configure() creates the object
			m_enableScan = false; // Scan outstation (Integrity Poll)
			// Default scan interval in seconds
			m_outstationScanInterval =
				(unsigned long)atol(DEFAULT_OUTSTATION_SCAN_INTERVAL);
			// Network timeout default
			m_applicationTimeout = 
				(unsigned long)DEFAULT_APPLICATION_TIMEOUT;
		};
		~DNP3()
		{
			if (m_manager)
			{
				delete m_manager;
			}
			auto it = m_outstations.begin();
			while (it != m_outstations.end())
			{
				it = m_outstations.erase(it);
			}
		};

		// Lock configuration items
		void	lockConfig() { m_configMutex.lock(); };
		// Unlock configuration items
		void	unlockConfig() { m_configMutex.unlock(); };

		// Ingest function
		void	ingest(std::string assetName,
				std::vector<Datapoint *>  points)
		{
			(*m_ingest)(m_data, Reading(m_asset + assetName, points));
		}
		// Register ingest function
		void	registerIngest(void *data, void (*cb)(void *, Reading))
		{
			m_ingest = cb;
			m_data = data;
		};
		void	setAssetName(const std::string& asset)
		{
			m_asset = asset;
		};
		void	setMasterLinkId(uint16_t id)
		{
			m_masterId = id;
		};
		uint16_t
			getMasterLinkId() { return m_masterId; };

		// Add outstation
		void	addOutStationTCP(OutStationTCP *outstation)
		{
			m_outstations.push_back(outstation);	
		};

		unsigned long
			getTimeout() const { return m_applicationTimeout; };
		void	setTimeout(unsigned long val)
		{
			m_applicationTimeout = val;
		};

		bool	start();

		// Stop master anc close outstation connection
		void	stop()
		{
			if (m_manager)
			{
				m_manager->Shutdown();
				delete m_manager;
				m_manager = NULL;
			}
		};
		bool	configure(ConfigCategory* config);
		void	enableScan(bool val) { m_enableScan = val; };
		bool	isScanEnabled() const { return m_enableScan; };
		unsigned long
			getOutstationScanInterval() const
		{
			return m_outstationScanInterval;
		};
		void 	setOutstationScanInterval(unsigned long val)
		{
			m_outstationScanInterval = val;
		};

		void	setAppLogLevel(uint32_t level)
		{
			m_appLogLevel = level;
		};
		uint32_t
			getAppLogLevel()
		{
			return m_appLogLevel;
		};

	private:
		std::string		m_serviceName;
		std::string		m_asset;
		uint16_t		m_masterId;
		asiodnp3::DNP3Manager* 	m_manager;
		bool			m_enableScan;
		unsigned long		m_outstationScanInterval;
		unsigned long		m_applicationTimeout;
		std::mutex		m_configMutex;;
		void			(*m_ingest)(void *, Reading);
		void			*m_data;
		// vector of Outstations
		std::vector<DNP3::OutStationTCP *>
					m_outstations;
		uint32_t		m_appLogLevel;
};

// Convert to string for most object types
template<class T> std::string ValueToString(const T& meas)
{
	return std::to_string(meas.value);
}

// Convert to string for DoubleBitBinary
template<class T> std::string ValueToStringDBB(const T& meas)
{
	return DoubleBitToString(meas.value);
}

using namespace opendnp3;

namespace asiodnp3
{
	// This class defines a custom SOE handler for data ingest in Fledge
	class dnp3SOEHandler : public opendnp3::ISOEHandler
	{
		public:
			dnp3SOEHandler(DNP3* dnp3, std::string& name)
			{
				m_dnp3 = dnp3;
				m_label = name;
			};

			// Data callbacks
			// We get data from these objects only 
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<Counter>>& values) override
			{
				return this->dnp3DataCallback(info,values, "Counter");
			};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<Binary>>& values) override
			{
				return this->dnp3DataCallback(info,values, "Binary");
			};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<BinaryOutputStatus>>& values) override
			{
				return this->dnp3DataCallback(info,values, "BinaryOutputStatus");
			};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<Analog>>& values) override
			{
				return this->dnp3DataCallback(info,values, "Analog");
			};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<AnalogOutputStatus>>& values) override
			{
				return this->dnp3DataCallback(info,values, "AnalogOutput");
			};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<DoubleBitBinary>>& values) //override {};
			{
				return this->dnp3DataCallbackDBB(info,values, "DoubleBitBinary");
			};

			// We don't get data from these
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<FrozenCounter>>& values) override {};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<OctetString>>& values) override {};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<TimeAndInterval>>& values) override {};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<BinaryCommandEvent>>& values) override {};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<AnalogCommandEvent>>& values) override {};
			void Process(const HeaderInfo& info,
				     const ICollection<Indexed<SecurityStat>>& values) override {};
			void Process(const HeaderInfo& info,
				     const ICollection<DNPTime>& values) override {};

		protected:
			void Start() {};
			void End() {};

			// Callback for data receiving:
			// solicited and unsolicited messages
			template<class T> void
				dnp3DataCallback(const HeaderInfo& info,
						 const ICollection<Indexed<T>>& values,
						 const std::string& objectType);
			// Callback for data receiving of DoubleBitBinary
			// solicited and unsolicited messages
			template<class T> void
				dnp3DataCallbackDBB(const HeaderInfo& info,
						 const ICollection<Indexed<T>>& values,
						 const std::string& objectType);

			// Process a data element from callback
			// and ingest data into Fledge
			template<class T> void dataElement(const opendnp3::HeaderInfo& info,
							   const T& value,
							   uint16_t index,
							   const std::string& objectName);
			// Process a data element from callback of DoubleBitBinary
			// and ingest data into Fledge
			template<class T> void dataElementDBB(const opendnp3::HeaderInfo& info,
							   const T& value,
							   uint16_t index,
							   const std::string& objectName);
		private:
			// assetName prefix
			std::string	m_label;
			DNP3*		m_dnp3;
	};

} // end namespace asiodnp3
#endif

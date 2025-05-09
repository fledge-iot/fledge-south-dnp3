/*
 * Fledge DNP3 south plugin.
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

typedef void (*INGEST_CB)(void *, Reading);


/**
 * Default configuration
 */
static const char *default_config = QUOTE({
	"plugin" : {
		"description" : "Simple DNP3 data change plugin",
		"type" : "string",
		"default" : "dnp3",
		"readonly" : "true"
		},
	"asset" : {
		"description" : "Asset name prefix for ingested readings data",
		"type" : "string",
		"default" : DEFAULT_ASSETNAME_PREFIX,
		"displayName" : "Asset Name prefix",
		"order" : "1"
		},
	"master_id" : {
		"description" : "Master Link ID",
		"type" : "integer",
		"default" : DEFAULT_MASTER_LINK_ID,
		"displayName" : "Master link ID",
		"order" : "2",
		"maximum" : "65519",
		"minimum" : "1"
		},
	"outstation_tcp_address" : {
		"description" : "Outstation TCP/IP address",
		"type" : "string",
		"default" : DEFAULT_TCP_ADDR,
		"displayName" : "Outstation address",
		"order" : "3",
		"mandatory": "true"
		},
	"outstation_tcp_port" : {
		"description" : "Outstation TCP/IP port",
		"type" : "integer",
		"default" : DEFAULT_TCP_PORT,
		"displayName" : "Outstation port",
		"order" : "4",
		"maximum" : "65000",
		"minimum" : "1"
		},
	"outstation_id" : {
		"description" : "Outstation Link ID",
		"type" : "integer",
		"default" : DEFAULT_OUTSTATION_ID,
		"displayName" : "Outstation link ID",
		"order" : "5",
		"maximum" : "65519",
		"minimum" : "1"
		},
	"outstation_scan_enable" : {
		"description" : "Enable outstation data scan (Integrity Poll for all Classes)",
		"type" : "boolean",
		"default" : "false",
		"displayName" : "Data scan",
		"order" : "6"
		},
	"outstation_scan_interval" : {
		"description" : "Outstation scan interval in seconds",
		"type" : "integer",
		"default" : DEFAULT_OUTSTATION_SCAN_INTERVAL,
		"displayName" : "Scan interval",
		"order" : "7",
		"minimum" : "1"
		},
	"data_fetch_timeout" : {
		"description" : "Timeout in seconds while fetching data",
		"type" : "integer",
		"default" : DEFAULT_APPLICATION_TIMEOUT,
		"displayName" : "Network timeout",
		"order" : "8",
		"minimum" : "1"
		},
	"outstations": {
		"description": "A list of DNP3 outstations to connect",
		"type": "list",
		"items" : "object",
		"default": "[]",
		"order" : "9",
		"displayName" : "Outstations",
		"properties" : {
				"linkid" : {
					"description" : "The outstation link ID",
					"displayName" : "Link ID",
					"type" : "integer",
					"maximum" : "65519",
					"minimum" : "1",
					"default" : "10"
				},
				"address" : {
					"description" : "The outstation TCP address or name ",
					"displayName" : "TCP Address",
					"type" : "string",
					"default" : "127.0.0.1",
					"mandatory": "true"
				},
				"port" : {
					"description" : "The outstation TCP port",
					"displayName" : "TCP Port",
					"type" : "integer",
					"default" : "20000",
					"maximum" : "65000",
					"minimum" : "1"
				}
#ifdef USE_TLS
				,
				"TLS": {
					"description" : "Outstation TLS setting",
					"displayName" : "Outstation TLS setting",
					"type": "enumeration",
					"default" : "Use local default",
					"options": [
						"Use local default",
						"Enable TLS",
						"Disable TLS"
					]
				},
				"TLSCAcertificate": {
					"description" : "Set a specific TLS CA certificate name (PEM)",
					"displayName" : "Specific TLS CA certificate",
					"type" : "string",
					"default" : ""
				},
				"TLScertificate": {
					"description" : "Set a specific TLS master certificate (PEM)",
					"displayName" : "Specific TLS master certificate",
					"type" : "string",
					"default" : ""
				}
#endif
			}
		},
		"appLogLevel": {
			"type": "enumeration",
			"default": "Normal",
			"options": [
				"Normal",
				"Data",
				"DataAndLink",
				"All"
			],
			"description": "DNP3 communication debug objects",
			"displayName": "DNP3 debug objects",
			"order" : "10"
		}
#ifdef USE_TLS
		,
		"enableTLS": {
			"description" : "Enable TLS encryption in outstation to master communication",
			"type" : "boolean",
			"default" : "false",
			"displayName" : "Enable TLS",
			"order" : "11",
			"group": "TLS"
		},
		"TLSCAcertificate": {
			"description": "TLS CA Certificate used for all outstations unless overridden in the outstation list",
			"type": "string",
			"default": "dnp3ca",
			"order": "12",
			"displayName": "TLS CA Certificate Name",
			"group": "TLS",
			"validity" : "enableTLS == \"true\""
		},
		"TLScertificate": {
			"description": "TLS Master Certificate used for all outstations unless overridden in the outstation list",
			"type": "string",
			"default": "master1",
			"order": "13",
			"displayName": "TLS Master Certificate Name",
			"group": "TLS",
			"validity" : "enableTLS == \"true\""
		}
#endif
	});

/**
 * The DNP3 plugin interface
 */
extern "C" {

/**
 * The plugin information structure
 */
static PLUGIN_INFORMATION info = {
	"dnp3",                  // Name
	VERSION,                  // Version
	SP_ASYNC, 		  // Flags
	PLUGIN_TYPE_SOUTH,        // Type
	"1.0.0",                  // Interface version
	default_config		  // Default configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	Logger::getLogger()->info("DNP3 south plugin configuration is %s",
				  info.config);
	return &info;
}

/**
 * Initialise the plugin, called to get the plugin handle
 */
PLUGIN_HANDLE plugin_init(ConfigCategory *config)
{
	DNP3* dnp3 = new DNP3(config->getName());

	if (!dnp3->configure(config))
	{
		delete dnp3;
		Logger::getLogger()->fatal("DNP3 south plugin 'plugin_init' failed");
		dnp3 = NULL;
	}

	// Return object
	return (PLUGIN_HANDLE)dnp3;
}

/**
 * Start the Async handling for the plugin
 */
void plugin_start(PLUGIN_HANDLE *handle)
{
	Logger::getLogger()->debug("DNP3 south plugin 'plugin_start' called");
	if (!handle)
	{
		throw runtime_error("DNP3 plugin handle is NULL "
				    "in 'plugin_start' call");
	}

	DNP3* dnp3 = (DNP3 *)handle;

	// Apply configuration, start DNP3 master and
	// connect to configured outstation
	if (!dnp3->start())
	{
		throw runtime_error("DNP3 plugin failed to instantiate DNP3 master "
				    "in 'plugin_start' call");
		return;
	}
}

/**
 * Register ingest callback
 */
void plugin_register_ingest(PLUGIN_HANDLE handle,
			    INGEST_CB cb,
			    void *data)
{
	Logger::getLogger()->debug("DNP3 south plugin 'plugin_register_ingest' called");

	if (!handle)
	{
		throw runtime_error("DNP3 plugin handle is NULL "
				    "in 'plugin_register_ingest' call");
	}
	DNP3* dnp3 = (DNP3 *)handle;
	dnp3->registerIngest(data, cb);
}

/**
 * Poll for a plugin reading
 */
Reading plugin_poll(PLUGIN_HANDLE handle)
{
	throw runtime_error("DNP3 is an async plugin, poll should not be called");
}

/**
 * Reconfigure the plugin
 *
 */
void plugin_reconfigure(PLUGIN_HANDLE *handle, string& newConfig)
{
	DNP3* dnp3 = (DNP3 *)*handle;
	ConfigCategory	config("new", newConfig);

	Logger::getLogger()->debug("DNP3 south 'plugin_reconfigure' called");

	if (dnp3)
	{
		// Shutdown DNP3 master and close outstation connection
		dnp3->stop();
		// Apply new configuration
		dnp3->configure(&config);
		// Start master and connect to outstation
		dnp3->start();
	}
}

/**
 * Shutdown the DNP3 plugin
 */
void plugin_shutdown(PLUGIN_HANDLE handle)
{
	DNP3* dnp3 = (DNP3 *)handle;
	Logger::getLogger()->debug("DNP3 south plugin 'plugin_shutdown' called");
	
	if (dnp3)
	{
		// Shutdown DNP3 master and close outstation connection
		dnp3->stop();
		delete dnp3;
		dnp3 = NULL;
	}
}

};

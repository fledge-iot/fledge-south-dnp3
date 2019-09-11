=====================
DNP3 C++ South plugin 
=====================

A simple asynchronous DNP3 protocol plugin that enables Fledge plugin to
be a DNP3  master, fetching data from DNP3 outstations.

NOTE:

This plugin assumes the opendnp3 library is available at a specified location
in the file system, see below.

Configuration
-------------

This configuration of this plugin requires 3 parameters to be set

asset
  An asset name prefix that is added to the DNP3 objects retrieved from the DNP3 outstations

master_id
  The master link id for DNP3 protocol

outstation_tcp_address
  The remote end (Outstation) TCP/IP address

outstation_tcp_port
  The remote end (Outstation) TCP/IP port

outstation_id
  The remote end (Outstation) link id

data_fetch_timeout
  Timeout for data fetch from outstation, in seconds

outstation_scan_enable
  Whether to scan all objects and values from the outstation.
  This is the Integrity Poll for all Classes

outstation_scan_interval
  Outstation scan interval in seconds
    
Whithin this basic configurations all object types in outstation are fetched.
The DNP3 plugin also handles unsolicited messages trasmitted by the outstation.


DNP3 Outstation tests
----------------------

It's possible to run an outstation demo application from the opendnp3 library
source tree:

.. code-block:: console

  $ host1:~/opendnp3/build$ ./outstation-demo

This demo application listens on any IP address, port 20001 and has link Id set to 10.
It also assumes master link Id is 1.

Once started il logs traffic and wait for use input for sending unsolicited messages:

.. code-block:: console

   Enter one or more measurement changes then press <enter>
   c = counter, b = binary, d = doublebit, a = analog, o = octet string, 'quit' = exit

Another option is the usage of a DNP3 Outstation simulator, as an example:


http://freyrscada.com/dnp3-ieee-1815-Client-Simulator.php#Download-DNP3-Development-Bundle

Once the boundle has been downloaded, the **DNPOutstationSimulator.exe** under "Simulator" folder
can be installed and run in Win32 platforms.


Building opendnp3
------------------

To build opendnp3 you must clone the opendnp3 repository to a directory of your choice.
The branch name is release-2.x

.. code-block:: console

  $ git clone --recursive -b release-2.x https://github.com/dnp3/opendnp3.git
  $ cd opendnp3
  $ export OPENDNP3_LIB_DIR=`pwd`
  $ mkdir build
  $ cd build
  $ cmake -DSTATICLIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DDNP3_DEMO=ON ..
  $ make

The opendnp3 library requires boost libraries that are not available in packaged form for the
Raspbery Pi. Therefore it can not be built for the Raspbery Pi without first building these boost
libraries.

Note: cmake 3.11 is required in order to build the opendnp3

Alternatively run the script requirements.sh to automate this and place a copy of the opendnp3
project in your home directory.

.. code-block:: console

  ./requirements.sh

If you require to place the opendnp3 code elsewhere you may pass the requirements.sh script an argument
of a directory name to use.

.. code-block:: console

  requirements.sh ~/projects

Build
-----

To build the dnp3 plugin run the commands:

.. code-block:: console

  $ mkdir build
  $ cd build
  $ cmake ..
  $ make

- By default the Fledge develop package header files and libraries
  are expected to be located in /usr/include/fledge and /usr/lib/fledge
- If **FLEDGE_ROOT** env var is set and no -D options are set,
  the header files and libraries paths are pulled from the ones under the
  FLEDGE_ROOT directory.
  Please note that you must first run 'make' in the FLEDGE_ROOT directory.

You may also pass one or more of the following options to cmake to override 
this default behaviour:

- **FLEDGE_SRC** sets the path of a Fledge source tree
- **FLEDGE_INCLUDE** sets the path to Fledge header files
- **FLEDGE_LIB sets** the path to Fledge libraries
- **FLEDGE_INSTALL** sets the installation path of Random plugin

NOTE:
 - The **FLEDGE_INCLUDE** option should point to a location where all the Fledge 
   header files have been installed in a single directory.
 - The **FLEDGE_LIB** option should point to a location where all the Fledge
   libraries have been installed in a single directory.
 - 'make install' target is defined only when **FLEDGE_INSTALL** is set

Examples:

- no options

  $ cmake ..

- no options and FLEDGE_ROOT set

  $ export FLEDGE_ROOT=/some_fledge_setup

  $ cmake ..

- set FLEDGE_SRC

  $ cmake -DFLEDGE_SRC=/home/source/develop/Fledge  ..

- set FLEDGE_INCLUDE

  $ cmake -DFLEDGE_INCLUDE=/dev-package/include ..
- set FLEDGE_LIB

  $ cmake -DFLEDGE_LIB=/home/dev/package/lib ..
- set FLEDGE_INSTALL

  $ cmake -DFLEDGE_INSTALL=/home/source/develop/Fledge ..

  $ cmake -DFLEDGE_INSTALL=/usr/local/fledge ..

=====================
DNP3 C++ South plugin 
=====================

A simple asynchronous DNP3 protocol plugin that enables FogLAMP plugin to
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

outstations
  An array of DNP3 outstations.
  
  Configuration examples:

.. code-block:: console

    {"outstations":["IP_1:PORT_1:ID_1", "IP_2:PORT_2:ID_2"]}

In the above examples
    AAAAA

Building opendnp3
------------------

To build opendnp3 you must clone the opendnp3 repository to a directory of your choice.

.. code-block:: console

  $ git clone https://github.com/dnp3/opendnp3.git
  $ cd opendnp3
  $ export OPENDNP3_LIB=`pwd`
  $ mkdir build
  $ cd build
  $ cmake -DDNP3_STATIC_LIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DDNP3_EXAMPLES=ON -DDNP3_TLS=OFF ..
  $ make

The opendnp3 library requires boost libraries that are not available in packaged form for the
Raspbery Pi. Therefore it can not be built for the Raspbery Pi without first building these boost
libraries.

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

- By default the FogLAMP develop package header files and libraries
  are expected to be located in /usr/include/foglamp and /usr/lib/foglamp
- If **FOGLAMP_ROOT** env var is set and no -D options are set,
  the header files and libraries paths are pulled from the ones under the
  FOGLAMP_ROOT directory.
  Please note that you must first run 'make' in the FOGLAMP_ROOT directory.

You may also pass one or more of the following options to cmake to override 
this default behaviour:

- **FOGLAMP_SRC** sets the path of a FogLAMP source tree
- **FOGLAMP_INCLUDE** sets the path to FogLAMP header files
- **FOGLAMP_LIB sets** the path to FogLAMP libraries
- **FOGLAMP_INSTALL** sets the installation path of Random plugin

NOTE:
 - The **FOGLAMP_INCLUDE** option should point to a location where all the FogLAMP 
   header files have been installed in a single directory.
 - The **FOGLAMP_LIB** option should point to a location where all the FogLAMP
   libraries have been installed in a single directory.
 - 'make install' target is defined only when **FOGLAMP_INSTALL** is set

Examples:

- no options

  $ cmake ..

- no options and FOGLAMP_ROOT set

  $ export FOGLAMP_ROOT=/some_foglamp_setup

  $ cmake ..

- set FOGLAMP_SRC

  $ cmake -DFOGLAMP_SRC=/home/source/develop/FogLAMP  ..

- set FOGLAMP_INCLUDE

  $ cmake -DFOGLAMP_INCLUDE=/dev-package/include ..
- set FOGLAMP_LIB

  $ cmake -DFOGLAMP_LIB=/home/dev/package/lib ..
- set FOGLAMP_INSTALL

  $ cmake -DFOGLAMP_INSTALL=/home/source/develop/FogLAMP ..

  $ cmake -DFOGLAMP_INSTALL=/usr/local/foglamp ..

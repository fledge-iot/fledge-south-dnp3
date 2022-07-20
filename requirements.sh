#!/usr/bin/env bash

##--------------------------------------------------------------------
## Copyright (c) 2019 Dianomic Systems
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##--------------------------------------------------------------------

##
## Author: Massimiliano Pinto
##

fledge_location=`pwd`
os_name=`(grep -o '^NAME=.*' /etc/os-release | cut -f2 -d\" | sed 's/"//g')`
os_version=`(grep -o '^VERSION_ID=.*' /etc/os-release | cut -f2 -d\" | sed 's/"//g')`
echo "Platform is ${os_name}, Version: ${os_version}"

if [[ ( $os_name == *"Red Hat"* || $os_name == *"CentOS"* ) &&  $os_version == *"7"* ]]; then
	echo Installing development tools 7 components
	sudo yum install -y yum-utils
	sudo yum-config-manager --enable rhel-server-rhscl-7-rpms
	sudo yum install -y devtoolset-7
	echo Installing boost components
	sudo yum install -y boost-filesystem
	sudo yum install -y boost-program-options
	sudo yum install -y llvm-toolset-7-clang
	sudo yum install -y asio-devel
	source scl_source enable devtoolset-7
	export CC=/opt/rh/devtoolset-7/root/usr/bin/gcc
	export CXX=/opt/rh/devtoolset-7/root/usr/bin/g++
	source scl_source enable llvm-toolset-7
elif apt --version 2>/dev/null; then
	echo Installing boost components
	sudo apt install -y libboost-filesystem-dev
	sudo apt install -y libboost-program-options-dev
	sudo apt install -y clang-format
	sudo apt install -y clang-tidy
	sudo apt install -y libasio-dev
else
	echo "Requirements cannot be automatically installed, please refer README.rst to install requirements manually"
fi

if [ $# -eq 1 ]; then
	directory=$1
	if [ ! -d $directory ]; then
		mkdir -p $directory
	fi
else
	directory=~
fi

if [ ! -d "${directory}/opendnp3" ]; then
	cd $directory
	echo "Fetching Open DNP3 library 'release' in ${directory} ..."
	git clone --recursive -b release https://github.com/dnp3/opendnp3.git
	cd opendnp3
	# Until we hve a newer libasio on all the platforms we support we will
	if [[ $os_name == *"Ubuntu"* && $os_version == *"20"* ]]; then
		# stick with release 2.3.0 of opendnp3
		git checkout tags/2.3.0
	else
		# stick with release 2.2.0 of opendnp3
		git checkout tags/2.2.0
	fi
	#sed -e "s/buffer = {0x00}/buffer = {{0x00}}/" < cpp/lib/include/opendnp3/app/OctetData.h \
	#	> cpp/lib/include/opendnp3/app/OctetData.h.$$ && \
	#	mv cpp/lib/include/opendnp3/app/OctetData.h cpp/lib/include/opendnp3/app/OctetData.h.orig && \
	#	mv cpp/lib/include/opendnp3/app/OctetData.h.$$ cpp/lib/include/opendnp3/app/OctetData.h

	# OpenDNP claims it needs 3.8 of cmake, but actually it doesn't
	sed -i CMakeLists.txt -e 's/VERSION 3.8/VERSION 2.8/'
	sed -i CMakeLists.txt -e 's/opendnp3 VERSION .*/opendnp3\)/'
	mkdir build
	cd build
	echo Building opendnp3 static library ...
	cmake -DSTATICLIBS=ON -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DDNP3_DEMO=ON ..
	make
	cd ..
	echo Set the environment variable OPENDNP3_LIB_DIR to `pwd`
	echo export OPENDNP3_LIB_DIR=`pwd`
fi

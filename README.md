<!--
# Copyright 2014-2020 Jetperch LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
-->

# Introduction

The Embedded C support library (embc) provides common utilities useful for 
embedded systems that are often not included in an RTOS or the standard
C library.  This library is intended for use by software developers producing 
firmware for embedded devices based upon microcontrollers.  The library can 
also be used on microprocessors as a lightweight supporting library.  
The EMBC library is available under the permissive Apache 2.0 license.

As of Oct 2020, the EMBC library API is not yet stable and subject to change
without notice.


## Installation

The embc library uses CMake to generate the project make files.  

Since EMBC is a library primarily targeted at embedded microcontrollers, you
will likely want to include the EMBC source code in your project.  Projects
using CMake can use ExternalProject_Add to include EMBC.  Other build systems
will likely need to include the source files directly.  Consider using
git subtrees or git submodules.  If not using cmake, you will need to
manually create the embc/config.h file.


### Ubuntu

First install the build tools for your system.

    sudo apt-get install build-essential cmake doxygen graphviz

Then compile the EMBC library:

    cd PROJECT_DIRECTORY
    mkdir build && cd $_
    cmake ../
    cmake --build .
    ctest


## Licenses

The EMBC library is available under the permissive Apache 2.0 license.
The embc library includes all third-party dependencies in the third-party
directory.  The dependencies are built along with the EMBC project.  
These third-party libraries are provided under their own 
[licenses](third-party/README.md)


## More information

The EMBC API is documented using doxygen. This documentation will eventually
be present on github.  For now, you will need to generate the documentation
locally.  

For a history of changes, see the [changelog](CHANGELOG.md).


## Alternatives

Several alternative libraries to embc exist that you can consider.

*   [microrl](https://github.com/Helius/microrl)
*   [Piconomic FW Library](http://piconomic.co.za/fwlib/index.html)
*   http://mbeddr.com/

Random number generation:

*   https://en.wikipedia.org/wiki/Xorshift
*   http://www.pcg-random.org/ [small](http://excamera.com/sphinx/article-xorshift.html)


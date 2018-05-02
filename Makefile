###############################################################################
#
# Copyright (c) 2015-2018, Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
###############################################################################

BUILD_DATE=$(shell date +%Y:%m:%d)
GIT_REV=$(shell git log --abbrev-commit --abbrev=16 2>/dev/null | head -1)
ifeq (${GIT_REV},)
	BUILD_VERSION="no_version_info"
else
	BUILD_VERSION=$(GIT_REV)
endif

CXX=clang++
CXXFLAGS=-std=c++14 -Wall -Wpedantic -Werror -MMD -MP -DBUILD_DATE="\"$(BUILD_DATE)\"" -DBUILD_VERSION="\"$(BUILD_VERSION)\""  -DBIT_MODE64=$(BIT_MODE64) -Iinclude
OPT=-O3
LDFLAGS=-L./ -std=c++14
BIT_MODE64=true

OBJ_DIR=objs/

LIBSOURCES=config_parser.cpp ir_value.cpp options.cpp type.cpp
LIBSOURCES_DIR=lib/
LIBSOURCES_SRC=$(addprefix $(LIBSOURCES_DIR), $(LIBSOURCES))
LIBOBJS=$(addprefix $(OBJ_DIR), $(LIBSOURCES:.cpp=.o))

SOURCES=main.cpp
SOURCES_DIR=src/
SOURCES_SRC=$(addprefix $(SOURCES_DIR), $(SOURCES))
OBJS=$(addprefix $(OBJ_DIR), $(SOURCES:.cpp=.o))


HEADERS=config_parser.hpp ir_value.hpp json.hpp options.hpp type_enums.hpp type.hpp utils.hpp
HEADERS_DIR=include/
HEADERS_SRC=$(addprefix $(HEADERS_DIR), $(HEADERS))

DEPS=$(OBJS:.o=.d) $(LIBOBJS:.o=.d)
EXECUTABLE=yarpgen

default: $(EXECUTABLE)

$(EXECUTABLE): dir $(LIBOBJS) $(OBJS) $(LIBSOURCES_SRC) $(SOURCES_SRC) $(HEADERS_SRC)
	$(CXX) $(OPT) $(LDFLAGS) -o $@ $(OBJS) $(LIBOBJS)

dir:
	/bin/mkdir -p $(OBJ_DIR)

clean:
	/bin/rm -rf $(OBJ_DIR) $(EXECUTABLE)

debug: $(EXECUTABLE)
debug: OPT=-O0 -g

ubsan: $(EXECUTABLE)
ubsan: CXXFLAGS+=-fsanitize=undefined

gcc: $(EXECUTABLE)
gcc: CXX=g++

gcov: $(EXECUTABLE) 
gcov: CXX=g++
gcov: OPT+=-fprofile-arcs -ftest-coverage -g

.SECONDEXPANSION:
$(LIBOBJS): $$(patsubst %.o,$(LIBSOURCES_DIR)%.cpp,$$(@F))
	$(CXX) $(OPT) $(CXXFLAGS) -o $@ -c $<

$(OBJS): $$(patsubst %.o,$(SOURCES_DIR)%.cpp,$$(@F))
	$(CXX) $(OPT) $(CXXFLAGS) -o $@ -c $<

-include $(DEPS)

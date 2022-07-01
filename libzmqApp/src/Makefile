TOP = ../..
include $(TOP)/configure/CONFIG

# extract visual studio major version number from path
# unfortunately cannot use %.% so using %.0 but may not match minor version number changes e.g. 10.1
ifneq ($(VisualStudioVersion),)
VS_VERS=$(filter %.0,$(subst \, ,$(VisualStudioVersion)))
else
VS_VERS=$(filter %.0,$(subst \, ,$(VSINSTALLDIR)))
endif
VS_MAJOR=$(word 1,$(subst ., ,$(VS_VERS)))

CMAKE ?= cmake

LIBZMQ_INSTALL=$(abspath $(TOP))/zmqinstall/$(EPICS_HOST_ARCH)
CMAKE_CONFIG_FLAGS=-DWITH_PERF_TOOL=ON -DZMQ_BUILD_TESTS=ON -DENABLE_CPACK=OFF -DENABLE_CURVE=OFF
CMAKE_BUILD_FLAGS=
ifneq ($(findstring linux,$(EPICS_HOST_ARCH)),)
CMAKE_GENERATOR=Unix Makefiles
else
ifneq ($(findstring windows,$(EPICS_HOST_ARCH)),)
PLATFORM=x64
else
PLATFORM=Win32
endif
VS_ARCH=$(VS_MAJOR)
CMAKE_GENERATOR=Visual Studio $(VS_ARCH)
CMAKE_PLATFORM=-A $(PLATFORM)
ifeq ($(findstring static,$(EPICS_HOST_ARCH)),)
CMAKE_CONFIG_FLAGS += -DBUILD_SHARED_LIBS=ON
WININSTLIB=lib
else
CMAKE_CONFIG_FLAGS += -DBUILD_SHARED_LIBS=OFF
WININSTLIB=staticlib
endif
endif
ifneq ($(findstring debug,$(EPICS_HOST_ARCH)),)
CMAKE_CONFIG_FLAGS += -DCMAKE_BUILD_TYPE:STRING=Debug
CMAKE_CONFIG=Debug
else
CMAKE_CONFIG_FLAGS += -DCMAKE_BUILD_TYPE:STRING=Release
CMAKE_CONFIG=Release
endif

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

ifdef T_A
install:
	$(CMAKE) $(TOP) -G "$(CMAKE_GENERATOR)" $(CMAKE_PLATFORM) -DCMAKE_INSTALL_PREFIX:PATH="$(LIBZMQ_INSTALL)" $(CMAKE_CONFIG_FLAGS)
	$(CMAKE) --build . --target install --config $(CMAKE_CONFIG) $(CMAKE_BUILD_FLAGS)
	-$(MKDIR) $(TOP)/bin
	-$(MKDIR) $(TOP)/bin/$(EPICS_HOST_ARCH)
	-$(MKDIR) $(TOP)/lib
	-$(MKDIR) $(TOP)/lib/$(EPICS_HOST_ARCH)
ifeq ($(findstring linux,$(EPICS_HOST_ARCH)),)
	$(CP) -f $(LIBZMQ_INSTALL)/bin/*.exe $(TOP)/bin/$(EPICS_HOST_ARCH)/
	$(CP) -f $(LIBZMQ_INSTALL)/bin/*.dll $(TOP)/bin/$(EPICS_HOST_ARCH)/
ifeq ($(findstring static,$(EPICS_HOST_ARCH)),)
	$(CP) -f $(LIBZMQ_INSTALL)/lib/libzmq-*-gd-*.lib $(TOP)/lib/$(EPICS_HOST_ARCH)/zmq.lib
else
	$(CP) -f $(LIBZMQ_INSTALL)/lib/libzmq-*-sgd-*.lib $(TOP)/lib/$(EPICS_HOST_ARCH)/zmq.lib
endif
#else
#	$(CP) -f $(OPENCV_INSTALL)/lib*/libopencv* $(TOP)/lib/$(EPICS_HOST_ARCH)/
endif
endif

#clean::
#	$(MAKE) -C mysql-connector-c -f Makefile.epics clean T_A=$(T_A) CMAKE_CONFIG=$(CMAKE_CONFIG) PLATFORM=$(PLATFORM) MYSQL_DIR="$(MYSQL_DIR)"
#	$(MAKE) -C mysql-connector-c++ -f Makefile.epics clean T_A=$(T_A) CMAKE_CONFIG=$(CMAKE_CONFIG) PLATFORM=$(PLATFORM) MYSQL_DIR="$(MYSQL_DIR)"
#
#uninstall:
#	$(RMDIR) $(MYSQL_DIR)
#

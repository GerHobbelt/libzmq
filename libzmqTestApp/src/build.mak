TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

### NOTE: there should only be one build.mak for a given IOC family and this should be located in the ###-IOC-01 directory

#=============================
# Build the IOC application libzmqTest
# We actually use $(APPNAME) below so this file can be included by multiple IOCs

PROD_IOC = $(APPNAME)
# libzmqTest.dbd will be created and installed
DBD += $(APPNAME).dbd

# libzmqTest.dbd will be made up from these files:
$(APPNAME)_DBD += base.dbd
## add other dbd here ##
#$(APPNAME)_DBD += xxx.dbd

# Add all the support libraries needed by this IOC

## Add additional libraries here ##
$(APPNAME)_LIBS += zmq

USR_CXXFLAGS += -I$(CPPZMQ)

ifneq ($(findstring static,$(EPICS_HOST_ARCH)),)
USR_CXXFLAGS += -DZMQ_STATIC
endif

$(APPNAME)_SRCS += zmqtest.cpp cppzmqtest.cpp

# libzmqTest_registerRecordDeviceDriver.cpp derives from libzmqTest.dbd
$(APPNAME)_SRCS += $(APPNAME)_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
$(APPNAME)_SRCS_DEFAULT += $(APPNAME)Main.cpp
$(APPNAME)_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#$(APPNAME)_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
$(APPNAME)_LIBS += $(EPICS_BASE_IOC_LIBS)

$(APPNAME)_SYS_LIBS_WIN32 += Iphlpapi

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE


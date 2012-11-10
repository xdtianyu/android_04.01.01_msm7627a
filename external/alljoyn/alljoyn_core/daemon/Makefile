# Copyright 2012, Qualcomm Innovation Center, Inc.
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
# 

.PHONY: all clean

all: daemon test_progs

BT_SRCS = BTController.cc BTTransport.cc BTNodeDB.cc
DAEMON_SRCS = $(wildcard *.cc)
DAEMON_SRCS += $(wildcard *.c)

ifeq "$(OS)" "darwin"
    # Darwin has its own version of the daemon transport
    DAEMON_SRCS += $(OS)/DaemonTransport.cc
else
    DAEMON_SRCS += $(OS_GROUP)/DaemonTransport.cc
endif

# Select BlueZ for bluetooth support or not
ifeq "$(OS)" "android_donut"
    # Skip bluetooth
else
    # Use BlueZ
    INCLUDE += -I$(PWD)/bt_bluez
    DAEMON_SRCS += $(wildcard bt_bluez/*.cc)
endif


DAEMON_OBJS = $(patsubst %.cc,%.o,$(DAEMON_SRCS))
$(DAEMON_OBJS) : $(DAEMON_SRC)
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) -c -o $@ $(patsubst %.o,%.cc,$@)


CONFIG_SRCS = ConfigDB.cc ServiceDB.cc PropertyDB.cc PolicyDB.cc
CONFIG_OBJS = $(patsubst %.cc,%.o,$(CONFIG_SRCS))

DAEMON_MAIN = $(OS_GROUP)/daemon-main.cc
DAEMON_MAIN_OBJ = $(patsubst %.cc,%.o,$(DAEMON_MAIN))

TESTDIR = test

$(TESTDIR)/advtunnel.o : $(TESTDIR)/advtunnel.cc
$(TESTDIR)/bbdaemon.o : $(TESTDIR)/bbdaemon.cc
$(TESTDIR)/DaemonTest.o : $(TESTDIR)/DaemonTest.cc
$(TESTDIR)/mcmd.o : $(TESTDIR)/mcmd.cc
$(TESTDIR)/ns.o : $(TESTDIR)/ns.cc

%.o:%.cc
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) -c -o $@ $<

daemon : $(DAEMON_OBJS) $(DAEMON_MAIN_OBJ)
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o alljoyn-daemon $(DAEMON_OBJS) $(DAEMON_MAIN_OBJ) $(LIBS)
	cp alljoyn-daemon $(INSTALLDIR)/dist/bin
 
test_progs: advtunnel bbdaemon DaemonTest mcmd ns

advtunnel : $(DAEMON_OBJS) $(TESTDIR)/advtunnel.o
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o advtunnel $(DAEMON_OBJS) $(TESTDIR)/advtunnel.o $(LIBS)
	cp advtunnel $(INSTALLDIR)/dist/bin

bbdaemon : $(DAEMON_OBJS) $(TESTDIR)/bbdaemon.o
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o bbdaemon $(DAEMON_OBJS) $(TESTDIR)/bbdaemon.o $(LIBS)
	cp bbdaemon $(INSTALLDIR)/dist/bin

DaemonTest : $(DAEMON_OBJS) $(TESTDIR)/DaemonTest.o
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o DaemonTest $(DAEMON_OBJS) $(TESTDIR)/DaemonTest.o $(LIBS)
	cp DaemonTest $(INSTALLDIR)/dist/bin

mcmd : $(DAEMON_OBJS) $(TESTDIR)/mcmd.o
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o mcmd $(DAEMON_OBJS) $(TESTDIR)/mcmd.o $(LIBS)
	cp mcmd $(INSTALLDIR)/dist/bin

ns : $(DAEMON_OBJS) $(TESTDIR)/ns.o
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o ns $(DAEMON_OBJS) $(TESTDIR)/ns.o $(LIBS)
	cp ns $(INSTALLDIR)/dist/bin

clean:
	@rm -f *.o *~ $(OS_GROUP)/*.o $(TESTDIR)/*.o bt_bluez/*.o alljoyn-daemon advtunnel bbdaemon DaemonTest mcmd ns



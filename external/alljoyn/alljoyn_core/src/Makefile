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

STATIC_LIB = liballjoyn.a
DYNAMIC_LIB = liballjoyn.so

.PHONY: all clean

all: corelib

ifeq "$(OS)" "darwin"
   OS_SRC_DIR = $(OS)
else
   OS_SRC_DIR = $(OS_GROUP)
endif

ALLJOYN_CC_SRCS = $(wildcard *.cc)
ALLJOYN_CC_SRCS += $(wildcard $(OS_SRC_DIR)/*.cc)
ALLJOYN_CC_OBJS = $(patsubst %.cc,%.o,$(ALLJOYN_CC_SRCS))
COMMON_OBJS = $(wildcard $(COMMONDIR)/src/*.o) $(wildcard $(COMMONDIR)/os/$(OS_GROUP)/*.o) $(wildcard $(COMMONDIR)/crypto/$(CRYPTO)/*.o)
LINKFLAGS += -shared

$(ALLJOYN_CC_OBJS) : $(ALLJOYN_CC_SRCS)
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) -c -o $@ $(patsubst %.o,%.cc,$@)

ALLJOYN_C_SRCS = $(wildcard *.c)
ALLJOYN_C_OBJS = $(patsubst %.c,%.o,$(ALLJOYN_C_SRCS))

$(ALLJOYN_C_OBJS) : $(ALLJOYN_C_SRCS)
	$(CC) $(CFLAGS) $(CPPDEFINES) $(INCLUDE) -c -o $@ $(patsubst %.o,%.c,$@)

%.o:%.cc
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) -c -o $@ $<
    
corelib: $(ALLJOYN_CC_OBJS) $(ALLJOYN_C_OBJS)
	$(AR) r $(STATIC_LIB) $(ALLJOYN_CC_OBJS) $(ALLJOYN_C_OBJS) $(COMMON_OBJS)
	cp $(STATIC_LIB) $(INSTALLDIR)/dist/lib
	$(CC) -o $(DYNAMIC_LIB) $(CXXFLAGS) $(CPPDEFINES) $(LINKFLAGS) $(INCLUDE)  $(ALLJOYN_CC_OBJS) $(ALLJOYN_C_OBJS) $(COMMON_OBJS) $(LIBS)
	cp $(DYNAMIC_LIB) $(INSTALLDIR)/dist/lib

clean:
	@rm -f *.o *~ $(STATIC_LIB) $(DYNAMIC_LIB) $(OS_SRC_DIR)/*.o


# Copyright 2010-2011, Qualcomm Innovation Center, Inc.
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

ALLJOYN_DIST := ../..
#ALLJOYN_DIST := ../../../build/linux/x86/debug/dist

ALLJOYN_LIB := $(ALLJOYN_DIST)/lib/liballjoyn.a

CXXFLAGS = -Wall -pipe -std=c++98 -fno-rtti -fno-exceptions -Wno-long-long -Wno-deprecated -g -DQCC_OS_LINUX -DQCC_OS_GROUP_POSIX -DQCC_CPU_X86

LIBS = -lalljoyn -lstdc++ -lcrypto -lpthread -lrt 

.PHONY: default clean

default: chat

clean:
	rm -f chat.o chat

chat: chat.o $(ALLJOYN_LIB)
	$(CXX) -o $@ chat.o -L$(ALLJOYN_DIST)/lib $(LIBS)

chat.o: chat.cc $(ALLJOYN_LIB)
	$(CXX) -c $(CXXFLAGS) -I$(ALLJOYN_DIST)/inc -o $@ chat.cc


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

all: basic chat
	cp README.$(OS).txt $(INSTALLDIR)/dist/samples/README.txt

BASIC_SRCS = $(wildcard basic/*.cc)
BASIC_BINS = $(patsubst %.cc,%,$(BASIC_SRCS))

basic : $(BASIC_BINS)

$(BASIC_BINS) : $(BASIC_SRCS)
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o $@ $< $(LIBS)
	cp $@ $(INSTALLDIR)/dist/bin/samples/
	@mkdir -p $(INSTALLDIR)/dist/samples/basic
	cp $(wildcard basic/*.cc) $(INSTALLDIR)/dist/samples/basic/
	cp $(wildcard basic/Make*) $(INSTALLDIR)/dist/samples/basic/
	cp basic/README.txt $(INSTALLDIR)/dist/samples/basic/


chat : chat_$(OS)

chat_linux: chat/linux/chat.cc
	$(CC) $(CXXFLAGS) $(CPPDEFINES) $(INCLUDE) $(LINKFLAGS) -o chat/linux/chat $< $(LIBS)
	cp chat/linux/chat $(INSTALLDIR)/dist/bin/samples/
	@mkdir -p $(INSTALLDIR)/dist/samples/chat

	cp $(wildcard chat/linux/*.cc) $(INSTALLDIR)/dist/samples/chat/ 
	cp $(wildcard chat/linux/Make*) $(INSTALLDIR)/dist/samples/chat/

clean:
	@rm -f $(BASIC_BINS) chat/linux/chat


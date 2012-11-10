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

all : common

common:
	cd src; make; 
	cd os/$(OS_GROUP); make;
	cd crypto/$(CRYPTO); make;

	@mkdir -p $(INSTALLDIR)/dist/inc/qcc
	cp inc/qcc/Log.h \
       inc/qcc/ManagedObj.h \
       inc/qcc/String.h \
       inc/qcc/atomic.h \
       inc/qcc/SocketWrapper.h \
       inc/qcc/platform.h $(INSTALLDIR)/dist/inc/qcc

	@mkdir -p $(INSTALLDIR)/dist/inc/qcc/$(OS_GROUP)
	cp inc/qcc/${OS_GROUP}/atomic.h \
       inc/qcc/${OS_GROUP}/platform_types.h \
       inc/qcc/${OS_GROUP}/unicode.h $(INSTALLDIR)/dist/inc/qcc/$(OS_GROUP)

clean:
	cd src; make clean;
	cd os/$(OS_GROUP); make clean;
	cd crypto/$(CRYPTO); make clean;
	@rm -f *.o

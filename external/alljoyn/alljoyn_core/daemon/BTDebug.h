/**
 * @file
 * BusObject responsible for implementing the AllJoyn methods (org.alljoyn.BTDebug)
 * for getting statistical information and tweaking certain internals.
 */

/******************************************************************************
 * Copyright 2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/
#ifndef _ALLJOYN_BTDEBUGOBJ_H
#define _ALLJOYN_BTDEBUGOBJ_H

// Include contents in debug builds only.
#ifndef NDEBUG

#include <qcc/platform.h>

#include <list>
#include <map>
#include <vector>

#include "AllJoynDebugObj.h"


namespace ajn {

namespace debug {


class BTDebugObjAccess {
    friend class BTDebugObj;

  public:
    virtual ~BTDebugObjAccess() { }

  private:
    virtual void FlushCachedNames() = 0;
};


/**
 * BusObject responsible for implementing the AllJoyn methods for org.alljoyn.BTDebug
 * for getting statistical information and tweaking certain internals.
 *
 * @cond ALLJOYN_DEV
 *
 * This is implemented entirely in the header file for the following reasons:
 *
 * - It is only instantiated in one place in debug builds only.
 * - It is easily excluded from release builds by conditionally including it.
 *
 * @endcond
 */
class BTDebugObj : public AllJoynDebugObjAddon {
  public:
    class BTDebugTimingProperty {

        struct TimingElement {
            BDAddress addr;
            uint32_t duration;
            TimingElement(const BDAddress& addr, uint32_t duration) : addr(addr), duration(duration) { }
        };

        typedef std::list<TimingElement> TimingInfo;

      public:

        QStatus Get(MsgArg& val) const
        {
            std::vector<MsgArg> elements;
            elements.reserve(timingInfo.size());
            TimingInfo::const_iterator it;
            for (it = timingInfo.begin(); it != timingInfo.end(); ++it) {
                elements.push_back(MsgArg("(su)", it->addr.ToString().c_str(), it->duration));
            }
            QStatus status = val.Set("a(su)", elements.size(), &elements.front());
            val.Stabilize();
            return status;
        }

        void FlushTimes() { timingInfo.clear(); }

        uint64_t StartTime()
        {
            qcc::Timespec ts;
            qcc::GetTimeNow(&ts);
            return ts.GetAbsoluteMillis();
        }

        void RecordTime(const BDAddress& addr, uint64_t startTime)
        {
            qcc::Timespec ts;
            qcc::GetTimeNow(&ts);
            uint64_t now = ts.GetAbsoluteMillis();
            timingInfo.push_back(TimingElement(addr, static_cast<uint32_t>(now - startTime)));
        }


      private:
        TimingInfo timingInfo;
        std::map<uint32_t, uint64_t> timers;
    };

    class BTDebugProperties : public AllJoynDebugObj::Properties {
        typedef std::map<qcc::StringMapKey, BTDebugTimingProperty> TimingProps;
        typedef std::pair<qcc::StringMapKey, BTDebugTimingProperty> TimingEntry;

      public:
        BTDebugProperties() {
            timingProps.insert(TimingEntry(qcc::String("DiscoverTimes"), BTDebugTimingProperty()));
            timingProps.insert(TimingEntry(qcc::String("SDPQueryTimes"), BTDebugTimingProperty()));
            timingProps.insert(TimingEntry(qcc::String("ConnectTimes"), BTDebugTimingProperty()));
        }
        QStatus Get(const char* propName, MsgArg& val) const
        {
            TimingProps::const_iterator it = timingProps.find(propName);
            if (it != timingProps.end()) {
                return it->second.Get(val);
            }
            return ER_BUS_NO_SUCH_PROPERTY;
        }

        QStatus Set(const char* propName, MsgArg& val)
        {
            TimingProps::const_iterator it = timingProps.find(propName);
            if (it != timingProps.end()) {
                return ER_BUS_PROPERTY_ACCESS_DENIED;
            }
            return ER_BUS_NO_SUCH_PROPERTY;
        }

        void GetProperyInfo(const AllJoynDebugObj::Properties::Info*& info, size_t& infoSize)
        {
            static const AllJoynDebugObj::Properties::Info ourInfo[] = {
                { "DiscoverTimes", "a(su)", PROP_ACCESS_READ },
                { "SDPQueryTimes", "a(su)", PROP_ACCESS_READ },
                { "ConnectTimes",  "a(su)", PROP_ACCESS_READ },
            };
            info = ourInfo;
            infoSize = ArraySize(ourInfo);
        }

        void FlushTimes(const char* propName)
        {
            TimingProps::iterator it = timingProps.find(propName);
            if (it != timingProps.end()) {
                it->second.FlushTimes();
            }
        }


        BTDebugTimingProperty& GetTimerRef(const char* propName)
        {
            TimingProps::iterator it = timingProps.find(propName);
            assert(it != timingProps.end());
            return it->second;
        }

      private:
        TimingProps timingProps;
    };

    BTDebugObj(BTDebugObjAccess* btc) : btc(btc)
    {
        AllJoynDebugObj* dbg = AllJoynDebugObj::GetAllJoynDebugObj();

#define _MethodHandler(_a) static_cast<AllJoynDebugObjAddon::MethodHandler>(_a)
        AllJoynDebugObj::MethodInfo methodInfo[] = {
            { "FlushDiscoverTimes",   NULL,   NULL, NULL,
              _MethodHandler(&BTDebugObj::FlushTimesHandler) },
            { "FlushSDPQueryTimes",   NULL,   NULL, NULL,
              _MethodHandler(&BTDebugObj::FlushTimesHandler) },
            { "FlushConnectTimes",    NULL,   NULL, NULL,
              _MethodHandler(&BTDebugObj::FlushTimesHandler) },
            { "FlushCachedNames",     NULL,   NULL, NULL,
              _MethodHandler(&BTDebugObj::FlushCachedNamesHandler) },
        };
#undef _MethodHandler

        dbg->AddDebugInterface(this,
                               "org.alljoyn.Bus.Debug.BT",
                               methodInfo, ArraySize(methodInfo),
                               properties);
    }

    BTDebugTimingProperty& LookupTimingProperty(const char* propName)
    {
        return properties.GetTimerRef(propName);
    }

  private:

    QStatus FlushTimesHandler(Message& msg, std::vector<MsgArg>& replyArgs)
    {
        const char* propName;
        if (::strcmp("FlushDiscoverTimes", msg->GetMemberName()) == 0) {
            propName = "DiscoverTimes";
        } else if (::strcmp("FlushSDPQueryTimes", msg->GetMemberName()) == 0) {
            propName = "SDPQueryTimes";
        } else if (::strcmp("FlushConnectTimes", msg->GetMemberName()) == 0) {
            propName = "ConnectTimes";
        } else {
            return ER_FAIL;
        }
        properties.FlushTimes(propName);
        return ER_OK;
    }


    QStatus FlushCachedNamesHandler(Message& msg, std::vector<MsgArg>& replyArgs)
    {
        btc->FlushCachedNames();
        return ER_OK;
    }


    BTDebugObjAccess* btc;
    BTDebugProperties properties;
};



} // namespace debug
} // namespace ajn

#endif
#endif

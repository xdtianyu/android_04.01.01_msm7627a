/*
* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above
*      copyright notice, this list of conditions and the following
*      disclaimer in the documentation and/or other materials provided
*      with the distribution.
*    * Neither the name of Code Aurora Forum, Inc. nor the names of its
*      contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.

* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package com.android.bluetooth.test;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import org.xml.sax.Attributes;

import android.sax.Element;
import android.sax.EndElementListener;
import android.sax.EndTextElementListener;
import android.sax.RootElement;
import android.sax.StartElementListener;
import android.util.Log;
import android.util.Xml;

/**
* This class is used to parse the attribute values from the input xml file
* for the Gatt Server application.
*/
public class GattServiceParser {

    private static final String INCLUDED_SERVICE = "IncludedService";
    private static final String INCLUDED_SERVICES = "IncludedServices";
    private static final String SERVER = "Server";
    private static final String CHARACTERISTICS_VALUE = "CharacteristicsValue";
    private static final String CHARACTERISTIC = "Characteristic";
    private static final String VALUE = "Value";
    private static final String MAX = "max";
    private static final String MIN = "min";
    private static final String RANGE = "Range";
    private static final String PROPERTIES = "Properties";
    private static final String DESCRIPTOR = "Descriptor";
    private static final String DESCRIPTORS = "Descriptors";
    private static final String SECURITY = "Security";
    private static final String SECURITY_SETTINGS = "SecuritySettings";
    private static final String CHARACTERISTICS = "Characteristics";
    private static final String SERVICE = "Service";
    private static final String NAME = "name";
    private static final String TYPE = "type";
    private static final String UUID = "uuid";
    static final String PUB_DATE = "pubDate";

    InputStream raw = null;
    int currentHandle = -1;
    int serviceHandle = 0;
    public int serverMinHandle = 0;
    public int serverMaxHandle = -1;
    protected int charHandle = 0;
    protected int charDescHandle = 0;
    protected int charValueHandle = 0;

    private static final String TAG = "GattServiceParser";

    /**
    * Parses the attribute values from the input xml file into the data structures
    * for the Gatt Server application.
    */
    public void parse(InputStream input) {
        GattServerAppService.gattHandleToAttributes = new ArrayList<Attribute>();

        RootElement root = new RootElement(SERVER);
        root.setEndElementListener(new EndElementListener() {
            public void end() {
                GattServerAppService.serverMaxHandle = currentHandle;
            }
        });

        // Parse the service
        Element service = root.getChild(SERVICE);
        service.setStartElementListener(new StartElementListener() {
            public void start(Attributes attributes) {
                ++currentHandle;
                final Attribute serviceAttribute = new Attribute();
                serviceAttribute.uuid = attributes.getValue(UUID);
                serviceAttribute.type = attributes.getValue(TYPE);
                serviceAttribute.name = attributes.getValue(NAME);
                serviceAttribute.handle = currentHandle;
                serviceAttribute.startHandle = currentHandle;
                serviceHandle = serviceAttribute.handle;
                GattServerAppService.gattHandleToAttributes.add(serviceAttribute);

                //Check if the service is an included service and update handles
                if(GattServerAppService.includedServiceMap.containsKey(serviceAttribute.uuid)) {
                    Attribute inclAttr = GattServerAppService.includedServiceMap.
                            get(serviceAttribute.uuid);
                    inclAttr.startHandle = currentHandle;
                }

                //populate the Gatt Attrib type map with service handles
                List<Integer> hndlList = GattServerAppService.gattAttribTypeToHandle.
                                get(serviceAttribute.type);
                if(hndlList != null) {
                    hndlList.add(serviceAttribute.handle);
                }
            }
        });
        service.setEndElementListener(new EndElementListener() {
            public void end() {
                Attribute attr = GattServerAppService.gattHandleToAttributes.get(serviceHandle);
                attr.endHandle = currentHandle;

                //Check if the service is an included service and update handles
                if(GattServerAppService.includedServiceMap.containsKey(attr.uuid)) {
                    Attribute inclAttr = GattServerAppService.includedServiceMap.get(attr.uuid);
                    inclAttr.endHandle = currentHandle;
                }
            }
        });

        // parse the service Security Settings
        parseServiceSecuritySettings(service);

        //parse Include services
        Element includedServices = service.getChild(INCLUDED_SERVICES);
        Element includedService = includedServices.getChild(INCLUDED_SERVICE);
        includedService.setStartElementListener(new StartElementListener() {
            public void start(Attributes attributes) {
                ++currentHandle;
                final Attribute inclSrvAttribute = new Attribute();
                inclSrvAttribute.uuid = attributes.getValue(UUID);
                inclSrvAttribute.type = attributes.getValue(TYPE);
                inclSrvAttribute.handle = currentHandle;
                inclSrvAttribute.name = INCLUDED_SERVICE;

                //Check if the start handle and end handle of the included service is present
                //already in the includedServiceMap data structure
                if(GattServerAppService.includedServiceMap.containsKey(inclSrvAttribute.uuid)) {
                    if(inclSrvAttribute.startHandle == -1 || inclSrvAttribute.endHandle == -1) {
                        Attribute inclAttr = GattServerAppService.includedServiceMap.
                                get(inclSrvAttribute.uuid);
                        if(inclAttr.startHandle > -1 || inclAttr.endHandle > -1) {
                            inclSrvAttribute.startHandle = inclAttr.startHandle;
                            inclSrvAttribute.endHandle = inclAttr.endHandle;
                        }
                    }
                }
                GattServerAppService.includedServiceMap.put(inclSrvAttribute.uuid, inclSrvAttribute);
                GattServerAppService.gattHandleToAttributes.add(inclSrvAttribute);
                //populate the Gatt Attrib type map with included service handles
                List<Integer> hndlList = GattServerAppService.gattAttribTypeToHandle.
                        get(inclSrvAttribute.type);
                if(hndlList != null) {
                    hndlList.add(inclSrvAttribute.handle);
                }

            }
        });

        Element characteristics = service.getChild(CHARACTERISTICS);

        // Parse the characteristic
        Element characteristic = parseCharacteristics(characteristics);
        characteristic.setEndElementListener(new EndElementListener() {
            public void end() {
                Attribute attr = GattServerAppService.gattHandleToAttributes.get(charHandle);
                attr.endHandle = currentHandle;
            }
        });

        // parse properties for Characteristic
        parseCharacteristicsProperties(characteristic);

        // parse the value
        parseCharacteristicsValue(characteristic);

        // parse descriptors
        parseCharacteristicsDescriptors(characteristic);

        // parse the XML
        try {
            Xml.parse(input, Xml.Encoding.UTF_8, root.getContentHandler());
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
    /**
     * Parses the SecuritySettings for the attribute type "Service"
    */
    private void parseServiceSecuritySettings(Element service2) {
        Element serviceSecuritySettings = service2.getChild(SECURITY_SETTINGS);
        Element serviceSecuritySetting = serviceSecuritySettings.getChild(SECURITY);
        serviceSecuritySetting.getChild("ReadOnly").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(serviceHandle).permBits |= 0x04;
                    }
                }
            });
        serviceSecuritySetting.getChild("ReadWithAuthentication").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(serviceHandle).permBits |= 0x02;
                    }
                }
            });
        serviceSecuritySetting.getChild("ReadWithAuthorization").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(serviceHandle).permBits |= 0x01;
                    }
                }
            });
        serviceSecuritySetting.getChild("WriteWithAuthentication").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(serviceHandle).permBits |= 0x10;
                    }
                }
            });
        serviceSecuritySetting.getChild("WriteWithAuthorization").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(serviceHandle).permBits |= 0x08;
                    }
                }
            });
    }

    /**
     * Parses the SecuritySettings for the attribute type "Characteristic value"
    */
    private void parseCharValueSecuritySettings(Element service2) {
        Element serviceSecuritySettings = service2.getChild(SECURITY_SETTINGS);
        Element serviceSecuritySetting = serviceSecuritySettings.getChild(SECURITY);
        serviceSecuritySetting.getChild("ReadOnly").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(currentHandle).permBits |= 0x04;
                    }
                }
            });
        serviceSecuritySetting.getChild("ReadWithAuthentication").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(currentHandle).permBits |= 0x02;
                    }
                }
            });
        serviceSecuritySetting.getChild("ReadWithAuthorization").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(currentHandle).permBits |= 0x01;
                    }
                }
            });
        serviceSecuritySetting.getChild("WriteWithAuthentication").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(currentHandle).permBits |= 0x10;
                    }
                }
            });
        serviceSecuritySetting.getChild("WriteWithAuthorization").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(currentHandle).permBits |= 0x08;
                    }
                }
            });
    }

    /**
     * Parses the attribute type "Characteristic Descriptors"
    */
    private void parseCharacteristicsDescriptors(Element characteristic) {
        Element descriptors = characteristic.getChild(DESCRIPTORS);
        Element descriptor = descriptors.getChild(DESCRIPTOR);
        descriptor.setStartElementListener(new StartElementListener() {
            public void start(Attributes attributes) {
                ++currentHandle;
                final Attribute charDescAttribute = new Attribute();
                int attrTypeFound = 0;
                charDescAttribute.type = attributes.getValue(TYPE);
                charDescAttribute.name = attributes.getValue(NAME);
                charDescAttribute.handle = currentHandle;
                charDescAttribute.referenceHandle = charValueHandle;
                charDescHandle = charDescAttribute.handle;
                GattServerAppService.gattHandleToAttributes.add(charDescAttribute);

                //populate the Gatt Attrib type map with handles
                if(GattServerAppService.gattAttribTypeToHandle != null) {
                    for(Map.Entry<String, List<Integer>> entry :
                                GattServerAppService.gattAttribTypeToHandle.entrySet()) {
                        if(charDescAttribute.type.equalsIgnoreCase(entry.getKey().toString())) {
                            attrTypeFound = 1;
                            break;
                        }
                    }
                    if(attrTypeFound == 0) {
                        GattServerAppService.gattAttribTypeToHandle.
                                put(charDescAttribute.type, new ArrayList<Integer>());
                    }
                }
                List<Integer> hndlList = GattServerAppService.gattAttribTypeToHandle.
                        get(charDescAttribute.type);
                if(hndlList != null) {
                    hndlList.add(charDescAttribute.handle);
                }
            }
        });
        descriptor.getChild(VALUE).setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    String descName = GattServerAppService.gattHandleToAttributes.get(charDescHandle).name;
                    if(descName != null && descName.equalsIgnoreCase("Characteristic User Description")) {
                        GattServerAppService.gattHandleToAttributes.get(charDescHandle).value =
                                body.getBytes();
                    }
                    else {
                        GattServerAppService.gattHandleToAttributes.get(charDescHandle).value =
                                GattServerAppService.stringToByteArray(body);
                    }
                }
            });
        Element descProperty = descriptor.getChild(PROPERTIES);
        descProperty.getChild("Read").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charDescHandle).properties |= 0x02;
                    }
                }
            });
        descProperty.getChild("Write").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charDescHandle).properties |= 0x08;
                    }
                }
        });
        Element descSecuritySettings = descriptor.getChild(SECURITY_SETTINGS);
        Element descSecuritySetting = descSecuritySettings.getChild(SECURITY);

        descSecuritySetting.getChild("ReadOnly").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charDescHandle).permBits |= 0x04;
                    }
                }
            });
        descSecuritySetting.getChild("ReadWithAuthentication").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charDescHandle).permBits |= 0x02;
                    }
                }
            });
        descSecuritySetting.getChild("ReadWithAuthorization").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charDescHandle).permBits |= 0x01;
                    }
                }
            });
        descSecuritySetting.getChild("WriteWithAuthentication").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charDescHandle).permBits |= 0x10;
                    }
                }
            });
        descSecuritySetting.getChild("WriteWithAuthorization").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if(body!=null && body.equalsIgnoreCase("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charDescHandle).permBits |= 0x08;
                    }
                }
            });

        Element descRange = descriptor.getChild(RANGE);
        descRange.getChild(MIN).setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    GattServerAppService.gattHandleToAttributes.
                            get(charDescHandle).min_range = Integer
                            .parseInt(body);
                }
            });
        descRange.getChild(MAX).setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    GattServerAppService.gattHandleToAttributes.
                            get(charDescHandle).max_range = Integer
                            .parseInt(body);
                }
            });
    }

    /**
     * Parses the attribute type "Characteristic Value"
    */
    private void parseCharacteristicsValue(Element characteristic) {
        Element charValue = characteristic.getChild(CHARACTERISTICS_VALUE);

        Element value = charValue.getChild(VALUE);
        charValue.setStartElementListener(new StartElementListener() {
            public void start(Attributes attributes) {
                ++currentHandle;
                int attrTypeFound = 0;
                final Attribute charValueAttribute = new Attribute();
                charValueAttribute.name = CHARACTERISTICS_VALUE;
                charValueAttribute.type = GattServerAppService.
                                gattHandleToAttributes.get(charHandle).uuid;
                charValueAttribute.handle = currentHandle;
                charValueAttribute.referenceHandle = charHandle;
                charValueHandle = charValueAttribute.handle;
                GattServerAppService.gattHandleToAttributes.add(charValueAttribute);

                //populate the Gatt Attrib type map with handles
                if(GattServerAppService.gattAttribTypeToHandle != null) {
                    for(Map.Entry<String, List<Integer>> entry :
                            GattServerAppService.gattAttribTypeToHandle.entrySet()) {
                        if(charValueAttribute.type.equalsIgnoreCase(entry.getKey().toString())) {
                            attrTypeFound = 1;
                            break;
                        }
                    }
                    if(attrTypeFound == 0) {
                        GattServerAppService.gattAttribTypeToHandle.
                                put(charValueAttribute.type, new ArrayList<Integer>());
                    }
                }
                List<Integer> hndlList = GattServerAppService.gattAttribTypeToHandle.
                                get(charValueAttribute.type);
                if(hndlList != null) {
                    hndlList.add(charValueAttribute.handle);
                }
            }
        });

        value.setEndTextElementListener(new EndTextElementListener() {
            public void end(String body) {
                GattServerAppService.gattHandleToAttributes.get(charValueHandle).value =
                        GattServerAppService.stringToByteArray(body);
            }
        });
        parseCharValueSecuritySettings(charValue);
        parseCharacteristicsValueProperties(charValue);
    }

    /**
     * Parses the properties for attribute type "Characteristics"
    */
    private void parseCharacteristicsProperties(Element characteristic) {
        Element properties = characteristic.getChild(PROPERTIES);

        properties.getChild("Read").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charHandle).properties |= 0x02;
                    }
                }
            });
        properties.getChild("Write").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charHandle).properties |= 0x08;
                    }
                }
            });
        properties.getChild("WriteWithoutResponse").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charHandle).properties |= 0x04;
                    }
                }
            });
        properties.getChild("SignedWrite").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charHandle).properties |= 0x40;
                    }
                }
            });
        properties.getChild("ReliableWrite").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                }
            });
        properties.getChild("Notify").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charHandle).properties |= 0x10;
                    }
                }
            });
        properties.getChild("Indicate").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charHandle).properties |= 0x20;
                    }
                }
            });
        properties.getChild("WritableAuxiliaries").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                }
            });
        properties.getChild("Broadcast").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charHandle).properties |= 0x01;
                    }
                }
            });
    }

    /**
     * Parses the properties for attribute type "Characteristics Value"
    */
    private void parseCharacteristicsValueProperties(Element characteristic) {
        Element properties = characteristic.getChild(PROPERTIES);

        properties.getChild("Read").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charValueHandle).properties |= 0x02;
                    }
                }
            });
        properties.getChild("Write").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charValueHandle).properties |= 0x08;
                    }
                }
            });
        properties.getChild("WriteWithoutResponse").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charValueHandle).properties |= 0x04;
                    }
                }
            });
        properties.getChild("SignedWrite").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charValueHandle).properties |= 0x40;
                    }
                }
            });
        properties.getChild("ReliableWrite").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                }
            });
        properties.getChild("Notify").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charValueHandle).properties |= 0x10;
                    }
                }
            });
        properties.getChild("Indicate").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charValueHandle).properties |= 0x20;
                    }
                }
            });
        properties.getChild("WritableAuxiliaries").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                }
            });
        properties.getChild("Broadcast").setEndTextElementListener(
            new EndTextElementListener() {
                public void end(String body) {
                    if (body.equals("1")) {
                        GattServerAppService.gattHandleToAttributes.
                                get(charValueHandle).properties |= 0x01;
                    }
                }
            });
    }

    /**
     * Parses the attribute type "Characteristics"
    */
    private Element parseCharacteristics(Element characteristics) {
        Element characteristic = characteristics.getChild(CHARACTERISTIC);
        characteristic.setStartElementListener(new StartElementListener() {
            public void start(Attributes attributes) {
                ++currentHandle;
                final Attribute charAttribute = new Attribute();
                charAttribute.uuid = attributes.getValue(UUID);
                charAttribute.type = attributes.getValue(TYPE);
                charAttribute.name = attributes.getValue(NAME);
                charAttribute.handle = currentHandle;
                charAttribute.startHandle = currentHandle;
                charAttribute.referenceHandle = serviceHandle;
                charHandle = charAttribute.handle;
                GattServerAppService.gattHandleToAttributes.add(charAttribute);

                //populate the Gatt Attrib type map with handles
                List<Integer> hndlList = GattServerAppService.
                                gattAttribTypeToHandle.get(charAttribute.type);
                if(hndlList != null) {
                    hndlList.add(charAttribute.handle);
                }
            }
        });
        return characteristic;
    }

}
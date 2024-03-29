/*
    LinKNX KNX home automation platform
    Copyright (C) 2007 Jean-François Meessen <linknx@ouaye.net>
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "objectcontroller.h"
#include "persistentstorage.h"
#include "services.h"
#include <cmath>
#include <cassert>
#ifdef OPEN_HOME_AUTOMATION
#include <algorithm>
#include <iomanip>
#endif

ObjectController* ObjectController::instance_m;

Logger& Object::logger_m(Logger::getInstance("Object"));

Object::Object() : init_m(false), flags_m(Default), refCount_m(0), gad_m(0), readRequestGad_m(0), persist_m(false), writeLog_m(false), readPending_m(false)
{}

Object::~Object()
{
    if (refCount_m > 0)
        logger_m.errorStream() << "Object (id=" << getID() << "): deleted object still has " << refCount_m << " references" << endlog;
}

Object* Object::create(const std::string& type)
{
    if (type == "" || type == "EIS1" || type == "1.001")
        return new SwitchingObject();
    else if (type == "EIS2" || type == "3.007")
        return new DimmingObject();
    else if (type == "3.008")
        return new BlindsObject();
    else if (type == "EIS3" || type == "10.001")
        return new TimeObject();
    else if (type == "EIS4" || type == "11.001")
        return new DateObject();
    else if (type == "EIS5" || type == "9.xxx")
        return new ValueObject();
    else if (type == "14.xxx")
        return new ValueObject32();
    else if (type == "EIS6" || type == "5.xxx")
        return new U8Object();
    else if (type == "5.001")
        return new ScalingObject();
    else if (type == "5.003")
        return new AngleObject();
    else if (type == "heat-mode" || type == "20.102")
        return new HeatingModeObject();
    else if (type == "EIS10" || type == "7.xxx")
        return new U16Object();
    else if (type == "EIS11" || type == "12.xxx")
        return new U32Object();
    else if (type == "EIS14" || type == "6.xxx")
        return new S8Object();
    else if (type == "8.xxx")
        return new S16Object();
    else if (type == "13.xxx")
        return new S32Object();
#ifdef STL_STREAM_SUPPORT_INT64
    else if (type == "29.xxx")
        return new S64Object();
#endif
    else if (type == "16.001")
        return new String14Object();
    else if (type == "EIS15" || type == "16.000")
        return new String14AsciiObject();
    else if (type == "28.001")
        return new StringObject();
#ifdef OPEN_HOME_AUTOMATION
    else if (type == "DBIR01")
        return new DBIRObject(8);
    else if (type == "DMR01")
        return new DBIRObject(5);
    else if (type == "DMOV01")
        return new DISMObject(1);
    else if (type == "DISM04")
        return new DISMObject(4);
    else if (type == "DISM08")
        return new DISMObject(8);
    else if (type == "DDIM01")
        return new DDIMObject();
    else if (type == "DPBU01")
        return new DPBUObject(1);
    else if (type == "DPBU02")
        return new DPBUObject(2);
    else if (type == "DPBU04")
        return new DPBUObject(4);
    else if (type == "DPBU06")
        return new DPBUObject(6);
    else if (type == "DAMPLI01")
        return new DAMPLIObject();
    else if (type == "contact")
        return new ContactObject();
    else if (type == "location")
        return new LocationObject();
#endif
    else
        return 0;
}

Object* Object::create(ticpp::Element* pConfig)
{
    std::string type = pConfig->GetAttribute("type");
    Object* obj = Object::create(type);
    if (obj == 0)
    {
        std::stringstream msg;
        msg << "Object type not supported: '" << type << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
    obj->importXml(pConfig);
    return obj;
}

void Object::setValue(ObjectValue* value)
{
    if (set(value) || forceUpdate())
        onInternalUpdate();
}

void Object::setFloatValue(double value)
{
    if (set(value) || forceUpdate())
        onInternalUpdate();
}

#ifdef OPEN_HOME_AUTOMATION
void Object::updateValueFromExternalInput(const std::string& value)
{
    ObjectValue *pValue = createObjectValue(value);
    if (set(pValue))
        onUpdate();
}
#endif


ObjectValue* Object::get()
{
    if (!init_m)
        read();
    //logger_m.debugStream() << "Object (id=" << getID() << "): get" << endlog;
    return getObjectValue();
}

void Object::importXml(ticpp::Element* pConfig)
{
    std::string id = pConfig->GetAttribute("id");
    if (id == "")
        throw ticpp::Exception("Missing or empty object ID");
    if(id.find("/", 0) != std::string::npos)
    {
        std::stringstream msg;
        msg << "Slash character '/' not allowed in Object ID: " << id <<std::endl;
        throw ticpp::Exception(msg.str());
    }

    if (id_m == "")
        id_m = id;

    std::string gad = pConfig->GetAttributeOrDefault("gad", "nochange");
    // set default value to "nochange" just to see if the attribute was present or not in xml
    if (gad == "")
        gad_m = 0;
    else if (gad != "nochange")
        gad_m = Object::ReadGroupAddr(gad);
    readRequestGad_m = gad_m;

#ifdef OPEN_HOME_AUTOMATION
    std::string address = pConfig->GetAttributeOrDefault("address", "nochange");
    // set default value to "nochange" just to see if the attribute was present or not in xml
    if (address != "nochange")
        address_m = address;
#endif

    bool has_descr = false;
    bool has_listener = false;
    ticpp::Iterator< ticpp::Node > child;
    for ( child = pConfig->FirstChild(false); child != child.end(); child++ )
    {
        std::string val = child->Value();
        if (child->Type() == TiXmlNode::TEXT && val.length())
        {
            if (!has_descr)
            {
                descr_m = "";
                has_descr = true;
            }
            descr_m.append(val);
        }
        else if (child->Type() == TiXmlNode::ELEMENT && val == "listener")
        {
            if (!has_listener)
            {
                listenerGadList_m.clear();
                has_listener = true;
            }
            std::string listener_gad = child->ToElement()->GetAttribute("gad");
            eibaddr_t gad = Object::ReadGroupAddr(listener_gad);
            listenerGadList_m.push_back(gad);
            if (child->ToElement()->GetAttribute("read") == "true")
                readRequestGad_m = gad;
        }
        //        else
        //        {
        //            std::stringstream msg;
        //            msg << "Invalid element '" << val << "' inside object definition" << std::endl;
        //            throw ticpp::Exception(msg.str());
        //        }
    }

    try
    {
        std::string flags;
        pConfig->GetAttribute("flags", &flags);
        flags_m = 0;
        if (flags.find('c') != flags.npos)
            flags_m |= Comm;
        if (flags.find('r') != flags.npos)
            flags_m |= Read;
        if (flags.find('w') != flags.npos)
            flags_m |= Write;
        if (flags.find('t') != flags.npos)
            flags_m |= Transmit;
        if (flags.find('u') != flags.npos)
            flags_m |= Update;
        if (flags.find('i') != flags.npos)
            flags_m |= Init;
        if (flags.find('s') != flags.npos || flags.find('f') != flags.npos)
            flags_m |= Stateless;
    }
    catch( ticpp::Exception& ex )
    {
        flags_m = Default;
    }

    writeLog_m = (pConfig->GetAttribute("log") == "true");

    std::string precision = pConfig->GetAttribute("precision");
    if (!precision.empty())
        getObjectValue()->setPrecision(precision);

    // TODO: do we need to use the 'i' flag instead of init="request" attribute
    persist_m = false;
    initValue_m = pConfig->GetAttribute("init");
    if (initValue_m == "persist")
    {
        PersistentStorage *persistence = Services::instance()->getPersistentStorage();
        if (persistence)
        {
            std::string val = persistence->read(id_m);
            if (val != "")
            {
                ObjectValue *objval = createObjectValue(val);
                set(objval); // Here, we use set() instead of setValue() to avoid call to onInternalUpdate()
                delete objval;
            }
            persist_m = true;
        }
        else
        {
            std::stringstream msg;
            msg << "Unable to persist object '" << id_m << "'; PersistentStorage not configured" << std::endl;
            throw ticpp::Exception(msg.str());
        }
    }
    else if (initValue_m != "" && initValue_m != "request")
    {
        ObjectValue *objval = createObjectValue(initValue_m);
        set(objval); // Here, we use set() instead of setValue() to avoid call to onInternalUpdate()
        delete objval;
    }

#ifndef OPEN_HOME_AUTOMATION
   logger_m.infoStream() << "Configured object '" << id_m << "': gad=" << WriteGroupAddr(gad_m) << endlog;
#else
    logger_m.infoStream() << "Configured object '" << id_m
                          << "': gad=" << WriteGroupAddr(gad_m)
                          << " address=" << address_m << endlog;
#endif
}

void Object::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("type", getType());
    pConfig->SetAttribute("id", id_m);

    if (gad_m != 0)
        pConfig->SetAttribute("gad", WriteGroupAddr(gad_m));

#ifdef OPEN_HOME_AUTOMATION
     if (address_m != "")
        pConfig->SetAttribute("address", address_m);
#endif

    if (initValue_m != "")
        pConfig->SetAttribute("init", initValue_m);

    if (writeLog_m)
        pConfig->SetAttribute("log", "true");

    if (flags_m != Default)
    {
        std::stringstream flags;
        if (flags_m & Comm)
            flags << 'c';
        if (flags_m & Read)
            flags << 'r';
        if (flags_m & Write)
            flags << 'w';
        if (flags_m & Transmit)
            flags << 't';
        if (flags_m & Update)
            flags << 'u';
        if (flags_m & Init)
            flags << 'i';
        if (flags_m & Stateless)
            flags << 's';
        pConfig->SetAttribute("flags", flags.str());
    }

    if (descr_m != "")
        pConfig->SetText(descr_m);

    ListenerGadList_t::iterator it;
    for (it = listenerGadList_m.begin(); it != listenerGadList_m.end(); it++)
    {
        ticpp::Element pElem("listener");
        pElem.SetAttribute("gad", WriteGroupAddr(*it));
        if (readRequestGad_m == (*it))
            pElem.SetAttribute("read", "true");
        pConfig->LinkEndChild(&pElem);
    }
}

void Object::read()
{
    KnxConnection* con = Services::instance()->getKnxConnection();
    if (!readPending_m)
    {
        uint8_t buf[2] = { 0, 0 };
        con->write(getReadRequestGad(), buf, 2);
    }
    readPending_m = true;

    int cnt = 0;
    while (cnt < 100 && readPending_m)
    {
        if (con->isRunning())
            con->checkInput();
        else
            pth_usleep(10000);
        ++cnt;
    }
    // If the device didn't answer after 1 second, we consider the object's
    // default value as the current value to avoid waiting forever.
    init_m = true;
}

void Object::onInternalUpdate()
{
    if ((flags_m & Transmit) && (flags_m & Comm))
        doSend(true);
    onUpdate();
}

void Object::onUpdate()
{
    init_m = true;
    logger_m.infoStream() << "New value " << getValue() << " for object " << getID() << " (type: " << getType() << ")" << endlog;
    
    ListenerList_t::iterator it;
    for (it = listenerList_m.begin(); it != listenerList_m.end(); it++)
    {
        logger_m.debugStream() << "Calling onChange on listener for " << id_m << endlog;
        (*it)->onChange(this);
    }
    if (persist_m || writeLog_m)
    {
        PersistentStorage *persistence = Services::instance()->getPersistentStorage();
        if (persistence)
        {
            if (persist_m)
                persistence->write(id_m, getValue());
            if (writeLog_m)
                persistence->writelog(id_m, getValue());
        }
    }
}

void Object::onWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if ((flags_m & Write) && (flags_m & Comm))
    {
        lastTx_m = src;
        doWrite(buf, len, src);
    }
}

void Object::onRead(const uint8_t* buf, int len, eibaddr_t src)
{
    if ((flags_m & Read) && (flags_m & Comm))
        doSend(false);
}

void Object::onResponse(const uint8_t* buf, int len, eibaddr_t src)
{
    if ((flags_m & Update) && (flags_m & Comm))
    {
        readPending_m = false;
        lastTx_m = src;
        doWrite(buf, len, src);
    }
}

void Object::addChangeListener(ChangeListener* listener)
{
    logger_m.debugStream()  << "Adding listener to object '" << id_m << "'" << endlog;
    listenerList_m.push_back(listener);
}
void Object::removeChangeListener(ChangeListener* listener)
{
    listenerList_m.remove(listener);
}

eibaddr_t Object::ReadGroupAddr(const std::string& addr)
{
    int a, b, c;
    if (sscanf (addr.c_str(), "%d/%d/%d", &a, &b, &c) == 3)
        return ((a & 0x01f) << 11) | ((b & 0x07) << 8) | ((c & 0xff));
    if (sscanf (addr.c_str(), "%d/%d", &a, &b) == 2)
        return ((a & 0x01f) << 11) | ((b & 0x7FF));
    if (sscanf (addr.c_str(), "%x", &a) == 1)
        return a & 0xffff;
    std::stringstream msg;
    msg << "Object: Invalid group address format: '" << addr << "'" << std::endl;
    throw ticpp::Exception(msg.str());
}

eibaddr_t Object::ReadAddr(const std::string& addr)
{
    int a, b, c;
    if (sscanf (addr.c_str(), "%d.%d.%d", &a, &b, &c) == 3)
        return ((a & 0x0f) << 12) | ((b & 0x0f) << 8) | ((c & 0xff));
    if (sscanf (addr.c_str(), "%x", &a) == 1)
        return a & 0xffff;
    std::stringstream msg;
    msg << "Object: Invalid individual address format: '" << addr << "'" << std::endl;
    throw ticpp::Exception(msg.str());
}

std::string Object::WriteGroupAddr(eibaddr_t addr)
{
    char writegaddr_buf[16];
    sprintf (writegaddr_buf, "%d/%d/%d", (addr >> 11) & 0x1f, (addr >> 8) & 0x07, (addr) & 0xff);
    return std::string(writegaddr_buf);
}

std::string Object::WriteAddr(eibaddr_t addr)
{
    char writeaddr_buf[16];
    sprintf (writeaddr_buf, "%d.%d.%d", (addr >> 12) & 0x0f, (addr >> 8) & 0x0f, (addr) & 0xff);
    return std::string(writeaddr_buf);
}

Logger& ObjectValue::logger_m(Logger::getInstance("ObjectValue"));

SwitchingObjectValue::SwitchingObjectValue(const std::string& value)
{
    if (value == "1" || value == "on" || value == "true")
        value_m = true;
    else if (value == "0" || value == "off" || value == "false")
        value_m = false;
    else
    {
        std::stringstream msg;
        msg << "SwitchingObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string SwitchingObjectValue::toString()
{
    return value_m ? "on" : "off";
}

double SwitchingObjectValue::toNumber()
{
    return value_m ? 1.0 : 0.0;
}

bool SwitchingObjectValue::equals(ObjectValue* value)
{
    assert(value);
    SwitchingObjectValue* val = dynamic_cast<SwitchingObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "SwitchingObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "SwitchingObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return value_m == val->value_m;
}

int SwitchingObjectValue::compare(ObjectValue* value)
{
    assert(value);
    SwitchingObjectValue* val = dynamic_cast<SwitchingObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  << "SwitchingObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "SwitchingObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    if (value_m == val->value_m)
        return 0;
    else if (value_m)
        return 1;
    else
        return -1;
}

bool SwitchingObjectValue::set(ObjectValue* value)
{
    SwitchingObjectValue* val = dynamic_cast<SwitchingObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  << "SwitchingObjectValue: ERROR, set() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    if (value_m != val->value_m)
    {
        value_m = val->value_m;
        return true;
    }
    return false;
}

bool SwitchingObjectValue::set(double value)
{
    bool val = (value != 0.0);
    if (value_m != val)
    {
        value_m = val;
        return true;
    }
    return false;
}

Logger& SwitchingObject::logger_m(Logger::getInstance("SwitchingObject"));

SwitchingObject::SwitchingObject() : SwitchingObjectValue(false)
{}

SwitchingObject::~SwitchingObject()
{}

ObjectValue* SwitchingObject::createObjectValue(const std::string& value)
{
    return new SwitchingObjectValue(value);
}

void SwitchingObject::setValue(const std::string& value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

void SwitchingObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    bool newValue;
    if (len == 2)
        newValue = (buf[1] & 0x3F) != 0;
    else
        newValue = buf[2] != 0;
    SwitchingObjectValue val(newValue);

    if (set(&val) || forceUpdate())
        onUpdate();
}

void SwitchingObject::doSend(bool isWrite)
{
#ifndef OPEN_HOME_AUTOMATION
    uint8_t buf[2] = { 0, (isWrite ? 0x80 : 0x40) | (value_m ? 1 : 0) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 2);
#else
    if (getGad())
    {
        uint8_t buf[2] = { 0, (isWrite ? 0x80 : 0x40) | (value_m ? 1 : 0) };
    	Services::instance()->getKnxConnection()->write(getGad(), buf, 2);
        return;
    }

    if (!isWrite || getAddress() == "")
        return;

    std::string connection = getAddress().substr(0, getAddress().find('/'));

    IOPort* port = IOPortManager::instance()->getPort(connection);
    if (!port)
        return;

    std::string address = getAddress().substr(getAddress().find('/'));
    while (address.length() > 0 && address.at(0) == '/')
        address.erase(0, 1);
 
    std::string command = "SET ";
    command.append(address);
    command.append(" ");
    command.append(toString());

    port->send((const uint8_t *)command.c_str(), command.length());
#endif
}

void SwitchingObject::setBoolValue(bool value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

bool StepDirObjectValue::equals(ObjectValue* value)
{
    assert(value);
    StepDirObjectValue* val = dynamic_cast<StepDirObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "StepDirObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "StepDirObjectValue: Compare object='"
    << toString() << "' to value='"
    << val->toString() << "'" << endlog;
    return (direction_m == val->direction_m) && (stepcode_m == val->stepcode_m);
}

#ifdef OPEN_HOME_AUTOMATION
/*----DBIR object------------------------------------------------------------*/

Logger& DBIRObject::logger_m(Logger::getInstance("DBIRObject"));

DBIRObject::DBIRObject(int outputs)
{
    outputs_m = outputs;
}

DBIRObject::~DBIRObject()
{}

std::string DBIRObject::getType()
{
    if (outputs_m == 8)
    {
        return "DBIR01";
    }
    else if (outputs_m == 5)
    {
        return "DMR01";
    }
    else
    {
        logger_m.debugStream() << "DBIRObject: unsupported number of outputs "
                               << outputs_m << endlog;
        return "ERROR";
    }
}

ObjectValue* DBIRObject::createObjectValue(const std::string& value)
{
    return new SwitchingObjectValue(value);
}

void DBIRObject::setValue(const std::string& value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

/* called on update from bus */
void DBIRObject::updateValue(const std::string& value)
{
    SwitchingObjectValue val(value);
    if (set(&val))
    	onUpdate();
}

void DBIRObject::read()
{
/*    logger_m.errorStream() << "DBIRObject: read"
                           << endlog;
    
    DomintellConnection* con = Services::instance()->getDomintellConnection();

    //if (!readPending_m)
    {
        std::string command;

        command.append(std::string("BIR"));
        command.append(getAddress().substr(0, getAddress().length() - 2));
        command.append(std::string("%S"));
    
        logger_m.errorStream() << "DBIRObject: read " << command
                               << endlog;

        con->write((uint8_t*)command.c_str(), command.length());

        //readPending_m = true;
    }

    *int cnt = 0;
    while (cnt < 100 && readPending_m)
    {
        if (con->isRunning())
            con->checkInput();
        else
            pth_usleep(10000);
        ++cnt;
    }*/
    // If the device didn't answer after 1 second, we consider the object's
    // default value as the current value to avoid waiting forever.
    init_m = true;
}

void DBIRObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DBIRObject: doWrite"
                           << endlog;
}

void DBIRObject::doSend(bool isWrite)
{
    std::string command;

    if (outputs_m == 8)
    {
        command.append(std::string("BIR"));
    }
    else if (outputs_m == 5)
    {
        command.append(std::string("DMR"));
    }
    else
    {
        logger_m.debugStream() << "DBIRObject: unsupported number of outputs "
                               << outputs_m << endlog;
        return;
    }

    command.append(getAddress());
    
    if (getValue() == "on")
        command.append(std::string("%I"));
    else
        command.append(std::string("%O"));

    Services::instance()->getDomintellConnection()->write((uint8_t*)command.c_str(), command.length());
}

void DBIRObject::setBoolValue(bool value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

void DBIRObject::onWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DBIRObject: Unexpected call of onWrite"
                           << endlog;
}

void DBIRObject::onRead(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DBIRObject: Unexpected call of onRead"
                           << endlog;
}

void DBIRObject::onResponse(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DBIRObject: Unexpected call of onResponse"
                           << endlog;
}

/*----DBIR object------------------------------------------------------------*/

/*----DISM object------------------------------------------------------------*/
Logger& DISMObject::logger_m(Logger::getInstance("DISMObject"));

DISMObject::DISMObject(int inputs) : SwitchingObjectValue(false)
{
    inputs_m = inputs;
}

DISMObject::~DISMObject()
{}

std::string DISMObject::getType()
{
    if (inputs_m == 1)
    {
        return "DMOV01";
    }
    else if (inputs_m == 4)
    {
        return "DISM04";
    }
    else if (inputs_m == 8)
    {
        return "DISM08";
    }
    else
    {
        logger_m.debugStream() << "DISMObject: unsupported number of inputs "
                               << inputs_m << endlog;
        return "ERROR";
    }
}

ObjectValue* DISMObject::createObjectValue(const std::string& value)
{
    return new SwitchingObjectValue(value);
}

void DISMObject::setValue(const std::string& value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

/* called on update from bus */
void DISMObject::updateValue(const std::string& value)
{
    SwitchingObjectValue val(value);
    if (set(&val))
    	onUpdate();
}

void DISMObject::read()
{
    /*DomintellConnection* con = Services::instance()->getDomintellConnection();
    if (!readPending_m)
    {
        //uint8_t buf[2] = { 0, 0 };
        //con->write(getReadRequestGad(), buf, 2); //TODO
    }
    readPending_m = true;

    int cnt = 0;
    while (cnt < 100 && readPending_m)
    {
        if (con->isRunning())
            con->checkInput();
        else
            pth_usleep(10000);
        ++cnt;
    }
    // If the device didn't answer after 1 second, we consider the object's
    // default value as the current value to avoid waiting forever.*/
    init_m = true;
}

void DISMObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DISMObject: doWrite"
                           << endlog;
}

void DISMObject::doSend(bool isWrite)
{
    logger_m.errorStream() << "DISMObject: doSend"
                           << endlog;
}

void DISMObject::setBoolValue(bool value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

void DISMObject::onWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DISMObject: Unexpected call of onWrite"
                           << endlog;
}

void DISMObject::onRead(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DISMObject: Unexpected call of onRead"
                           << endlog;
}

void DISMObject::onResponse(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DISMObject: Unexpected call of onResponse"
                           << endlog;
}

/*----DISM object------------------------------------------------------------*/

/*----DPBU object------------------------------------------------------------*/

Logger& DPBUObject::logger_m(Logger::getInstance("DPBUObject"));

DPBUObject::DPBUObject(int buttons)
{
    buttons_m = buttons;
}

DPBUObject::~DPBUObject()
{}

std::string DPBUObject::getType()
{
    if (buttons_m == 1)
    {
        return "DPBU01";
    }
    else if (buttons_m == 2)
    {
        return "DPBU02";
    }
    else if (buttons_m == 4)
    {
        return "DPBU04";
    }
    else if (buttons_m == 6)
    {
        return "DPBU06";
    }
    else
    {
        logger_m.debugStream() << "DPBUObject: unsupported number of buttons "
                               << buttons_m << endlog;
        return "ERROR";
    }
}

ObjectValue* DPBUObject::createObjectValue(const std::string& value)
{
    return new SwitchingObjectValue(value);
}

void DPBUObject::setValue(const std::string& value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

/* called on update from bus */
void DPBUObject::updateValue(const std::string& value)
{
    SwitchingObjectValue val(value);
    if (set(&val))
    	onUpdate();
}

void DPBUObject::read()
{
    logger_m.errorStream() << "DPBUObject: read"
                           << endlog;
    
    // If the device didn't answer after 1 second, we consider the object's
    // default value as the current value to avoid waiting forever.
    init_m = true;
}

void DPBUObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DPBUObject: doWrite"
                           << endlog;
}

void DPBUObject::doSend(bool isWrite)
{
    std::string command;

    int output =
        strtol(getAddress().substr(getAddress().length() - 1).c_str(), (char **)NULL, 16);
    if (output < (buttons_m + 1) || output > (buttons_m * 2))
    {
        logger_m.errorStream() << "DPBUObject: doSend failed for "
                               << getAddress() << endlog;
        return;
    }
    output -= buttons_m;

    command.append(std::string("BU6"));
    command.append(getAddress().substr(0, getAddress().length() - 1));
    
    std::stringstream out;
    out << output;
    command.append(out.str());
    
    if (getValue() == "on")
        command.append(std::string("%I"));
    else
        command.append(std::string("%O"));

    Services::instance()->getDomintellConnection()->write((uint8_t*)command.c_str(), command.length());
}

void DPBUObject::setBoolValue(bool value)
{
    SwitchingObjectValue val(value);
    Object::setValue(&val);
}

void DPBUObject::onWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DPBUObject: Unexpected call of onWrite"
                           << endlog;
}

void DPBUObject::onRead(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DPBUObject: Unexpected call of onRead"
                           << endlog;
}

void DPBUObject::onResponse(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DPBUObject: Unexpected call of onResponse"
                           << endlog;
}

/*----DPBU object------------------------------------------------------------*/

/*----DAMPLI object----------------------------------------------------------*/

DAMPLIObjectValue::DAMPLIObjectValue(const std::string& value)
{
    if (value.length() != 15 || value.c_str()[2] != '-' || 
        value.c_str()[7] != '-' || value.c_str()[10] != '-')
    {
        std::stringstream msg;
        msg << "DAMPLIObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }

    volume_m = strtol(value.substr(0, 2).c_str(), (char **)NULL, 16);

    std::string input = value.substr(3, 4);
    if (input == "AUX1")
        input_m = 1;
    else if (input == "AUX2")
        input_m = 2;
    else if (input == "AUX3")
        input_m = 3;
    else if (input == "AUX4")
        input_m = 4;
    else if (input == "TUNE")
        input_m = 5;
    else
    {
        std::stringstream msg;
        msg << "DAMPLIObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }

    freq_m = strtol(value.substr(8, 2).c_str(), (char **)NULL, 16);
    freq_m = freq_m << 16;
    freq_m += (strtol(value.substr(11, 4).c_str(), (char **)NULL, 16) % 0xFFFF);

logger_m.errorStream() << "DAMPLIObjectValue: CONSTR freq= " << freq_m << endlog;
 
}

std::string DAMPLIObjectValue::toString()
{
    std::stringstream stream;
    stream << std::setfill ('0')
           << std::setw(2) 
           << std::hex << volume_m
           << "-";
    if (input_m < 5)
        stream << "AUX" << std::dec << input_m;
    else
        stream << "TUNE";
    stream << "-" << std::hex;

    stream << std::setfill ('0')
           << std::setw(2) 
           << (freq_m >> 16) % 0xFF
           << "-"
           << std::setfill ('0')
           << std::setw(4) 
           << (freq_m & 0xFFFF);

    return stream.str();
}

double DAMPLIObjectValue::toNumber()
{
    return volume_m;
}

bool DAMPLIObjectValue::equals(ObjectValue* value)
{
    assert(value);
    DAMPLIObjectValue* val = dynamic_cast<DAMPLIObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << 
            "DAMPLIObjectValue: ERROR, equals() received invalid class object "
            "(typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }

    if (volume_m == val->volume_m &&
        input_m == val->input_m && freq_m == val->freq_m)
    {
        return true;
    }

    return false;
}

int DAMPLIObjectValue::compare(ObjectValue* value)
{
    assert(value);
    DAMPLIObjectValue* val = dynamic_cast<DAMPLIObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  <<
            "DAMPLIObjectValue: ERROR, compare() received invalid class object "
            "(typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }

    if (volume_m < val->volume_m)
        return -1;
    else if (volume_m > val->volume_m)
        return 1;

    if (input_m < val->input_m)
        return -1;
    else if (input_m > val->input_m)
        return 1;

    if (freq_m < val->freq_m)
        return -1;
    else if (freq_m > val->freq_m)
        return 1;

    return 0;
}

bool DAMPLIObjectValue::set(ObjectValue* value)
{
    DAMPLIObjectValue* val = dynamic_cast<DAMPLIObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  <<
            "DAMPLIObjectValue: ERROR, set() received invalid class object "
            "(typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }

    if (compare(value) == 0)
        return false;

    volume_m = val->volume_m;
    input_m = val->input_m;
    freq_m = val->freq_m;

    return true;
}

bool DAMPLIObjectValue::set(double value)
{
    if (volume_m != value)
    {
        volume_m = value;
        return true;
    }
    return false;
}

void DAMPLIObjectValue::setVolume(int volume)
{
    volume_m = volume;
}

Logger& DAMPLIObject::logger_m(Logger::getInstance("DAMPLIObject"));

DAMPLIObject::DAMPLIObject() : DAMPLIObjectValue("00-TUNE-00-0000")
{
    //
}

DAMPLIObject::~DAMPLIObject()
{}

ObjectValue* DAMPLIObject::createObjectValue(const std::string& value)
{
    return new DAMPLIObjectValue(value);
}

void DAMPLIObject::setValue(const std::string& value)
{
    DAMPLIObjectValue val(value);
    Object::setValue(&val);
}

/* called on update from bus */
void DAMPLIObject::updateValue(const std::string& value)
{
    DAMPLIObjectValue val(value);
    if (set(&val))
    	onUpdate();
}

void DAMPLIObject::read()
{
    logger_m.errorStream() << "DAMPLIObject: read" << endlog;
    
    // If the device didn't answer after 1 second, we consider the object's
    // default value as the current value to avoid waiting forever.
    init_m = true;
}

void DAMPLIObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DAMPLIObject: doWrite" << endlog;
}

void DAMPLIObject::doSend(bool isWrite)
{
    std::stringstream stream;
    
    stream << "AMP" << getAddress() 
           << "%D" << std::dec << volume_m 
           << "%A" << input_m;

    if (input_m == 5)
        stream << "%F" << std::setfill ('0')
               << std::dec
               << (freq_m >> 16) % 0xFF
               << "."
               << (freq_m & 0xFFFF);

    Services::instance()->getDomintellConnection()->write((uint8_t*)stream.str().c_str(), stream.str().length());
}

void DAMPLIObject::onWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DAMPLIObject: Unexpected call of onWrite"
                           << endlog;
}

void DAMPLIObject::onRead(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DAMPLIObject: Unexpected call of onRead"
                           << endlog;
}

void DAMPLIObject::onResponse(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DAMPLIObject: Unexpected call of onResponse"
                           << endlog;
}

/*----DAMPLI object------------------------------------------------------------*/

#endif

int StepDirObjectValue::compare(ObjectValue* value)
{
    assert(value);
    StepDirObjectValue* val = dynamic_cast<StepDirObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  << "StepDirObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "StepDirObjectValue: Compare object='"
    << toString() << "' to value='"
    << val->toString() << "'" << endlog;

    if (stepcode_m == 0 && val->stepcode_m == 0)
        return 0;
    if (direction_m == val->direction_m)
    {
        if (stepcode_m == val->stepcode_m)
            return 0;
        if (stepcode_m > val->stepcode_m) // bigger stepcode => smaller steps
            return direction_m ? -1 : 1;
        else
            return direction_m ? 1 : -1;
    }
    return direction_m ? 1 : -1;
}

bool StepDirObjectValue::set(ObjectValue* value)
{
    StepDirObjectValue* val = dynamic_cast<StepDirObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  << "StepDirObjectValue: ERROR, set() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    if (direction_m != val->direction_m || stepcode_m != val->stepcode_m)
    {
        direction_m = val->direction_m;
        stepcode_m = val->stepcode_m;
        return true;
    }
    return false;
}

bool StepDirObjectValue::set(double value)
{
    int direction = 1;
    int stepcode = 0;
    if (value < 0.0)
    {
        direction = 0;
        value = -value;
    }
    stepcode = static_cast<int>(value);
    if (direction_m != direction || stepcode_m != stepcode)
    {
        direction_m = direction;
        stepcode_m = stepcode;
        return true;
    }
    return false;
}

double StepDirObjectValue::toNumber()
{
    if (stepcode_m == 0)
        return 0.0;
    return (direction_m ? 1.0 : -1.0) * stepcode_m;
}

Logger& StepDirObject::logger_m(Logger::getInstance("StepDirObject"));

StepDirObject::StepDirObject()
{}

StepDirObject::~StepDirObject()
{}

void StepDirObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    int newValue;
    if (len == 2)
        newValue = (buf[1] & 0x3F);
    else
        newValue = buf[2];
    int direction = (newValue & 0x08) >> 3;
    int stepcode = newValue & 0x07;
    if (stepcode == 0)
        direction = 0;

    if (setStep(direction, stepcode) || forceUpdate())
        onUpdate();
}

void StepDirObject::doSend(bool isWrite)
{
    uint8_t buf[2] = { 0, (isWrite ? 0x80 : 0x40) | (getDirection() ? 8 : 0) | (getStepCode() & 0x07) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 2);
}

DimmingObjectValue::DimmingObjectValue(const std::string& value)
{
    std::string dir;
    size_t pos = value.find(":");
    dir = value.substr(0, pos);
    stepcode_m = 1;
    if (pos != value.npos)
    {
        if (value.length() > pos+1)
        {
            char step = value[pos+1];
            if (step >= '1' && step <= '7')
                stepcode_m = step - '0';
            else
            {
                std::stringstream msg;
                msg << "DimmingObjectValue: Invalid stepcode (must be between 1 and 7): '" << step << "'" << std::endl;
                throw ticpp::Exception(msg.str());
            }
        }
    }
    if (dir == "stop")
    {
        direction_m = 0;
        stepcode_m = 0;
    }
    else if (dir == "up")
        direction_m = 1;
    else if (dir == "down")
        direction_m = 0;
    else
    {
        std::stringstream msg;
        msg << "DimmingObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string DimmingObjectValue::toString()
{
    if (stepcode_m == 0)
        return "stop";
    std::string ret(direction_m ? "up" : "down");
    if (stepcode_m != 1)
    {
        ret.push_back(':');
        ret.push_back('0' + stepcode_m);
    }
    return ret;
}

Logger& DimmingObject::logger_m(Logger::getInstance("DimmingObject"));

ObjectValue* DimmingObject::createObjectValue(const std::string& value)
{
    return new DimmingObjectValue(value);
}

void DimmingObject::setValue(const std::string& value)
{
    DimmingObjectValue val(value);
    Object::setValue(&val);
}

void DimmingObject::setStepValue(int direction, int stepcode)
{
    if (forceUpdate() || stepcode_m != stepcode  || direction_m != direction)
    {
        stepcode_m = stepcode;
        direction_m = direction;
        onInternalUpdate();
    }
}

#ifdef OPEN_HOME_AUTOMATION
/*----DDIM object------------------------------------------------------------*/

DDIMObjectValue::DDIMObjectValue(const std::string& value)
{
    value_m = 0;
    step_m = false;
    if (value == "on" || value == "true")
        value_m = 100;
    else if (value == "off" || value == "false")
        value_m = 0;
    else if (value == "up")
    {
        value_m = 5;
        step_m = true;
    }
    else if (value == "down")
    {
        value_m = -5;
        step_m = true;        
    }
    else
    {
        std::istringstream val(value);
        val >> value_m;

        if (val.fail() ||
            val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
        {
            std::stringstream msg;
            msg << "DDIMObjectValue: Bad value: '" << value << "'" << std::endl;
            throw ticpp::Exception(msg.str());
        }

        if (value_m > 100)
            value_m = 100;
        if (value_m < 0)
            value_m = 0;
    }
}

DDIMObjectValue::DDIMObjectValue(int value)
{
    value_m = value;
    step_m = false;
    if (value_m > 100)
        value_m = 100;
    if (value_m < 0)
        value_m = 0;
}

std::string DDIMObjectValue::toString()
{
    std::ostringstream out;
    out.precision(8);
    out << value_m;

    return out.str();
}

double DDIMObjectValue::toNumber()
{
    return (double)value_m;
}

bool DDIMObjectValue::equals(ObjectValue* value)
{
    assert(value);
    DDIMObjectValue* val = dynamic_cast<DDIMObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "DDIMObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "DDIMObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return (step_m == val->step_m && value_m == val->value_m);
}

int DDIMObjectValue::compare(ObjectValue* value)
{
    assert(value);
    DDIMObjectValue* val = dynamic_cast<DDIMObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  << "DDIMObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "DDIMObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
   if (step_m && !val->step_m)
       return -1;
   else if (!step_m && val->step_m)
       return 1;
 
   if (value_m == val->value_m)
        return 0;
    else if (value_m > val->value_m)
        return 1;
    else
        return -1;
}

bool DDIMObjectValue::set(ObjectValue* value)
{
    DDIMObjectValue* val = dynamic_cast<DDIMObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  << "DDIMObjectValue: ERROR, set() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }

    if (val->step_m)
    {
        value_m += val->value_m;
        return true;
    }
    if (value_m != val->value_m)
    {
        value_m = val->value_m;
        return true;
    }
    return false;
}

bool DDIMObjectValue::set(double value)
{
    if (value_m != (int)value)
    {
        value_m = (int)value;
        return true;
    }
    return false;
}

bool DDIMObjectValue::set(int value)
{
    if (value_m != value)
    {
        value_m = value;
        return true;
    }
    return false;
}

Logger& DDIMObject::logger_m(Logger::getInstance("DDIMObject"));

ObjectValue* DDIMObject::createObjectValue(const std::string& value)
{
    return new DDIMObjectValue(value);
}

void DDIMObject::setValue(const std::string& value)
{
    DDIMObjectValue val(value);
    Object::setValue(&val);
}

/* called on update from bus */
void DDIMObject::updateValue(const std::string& value)
{
    DDIMObjectValue val(value);
    if (set(&val))
    	onUpdate();
}

/* called on update from bus */
void DDIMObject::updateValue(int value)
{
    DDIMObjectValue val(value);
    if (set(&val))
    	onUpdate();
}

void DDIMObject::read()
{
    DomintellConnection* con = Services::instance()->getDomintellConnection();

    //if (!readPending_m)
    {
        std::string command;

        command.append(std::string("DIM"));
        command.append(getAddress().substr(0, getAddress().length() - 2));
        command.append(std::string("%S"));
    
        logger_m.errorStream() << "DBIRObject: read " << command
                               << endlog;

        con->write((uint8_t*)command.c_str(), command.length());

        //readPending_m = true;
    }

    // If the device didn't answer after 1 second, we consider the object's
    // default value as the current value to avoid waiting forever.
    init_m = true;
}

void DDIMObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DDIMObject: doWrite"
                           << endlog;
}

void DDIMObject::doSend(bool isWrite)
{
    logger_m.errorStream() << "DDIMObject: doWrite"
                           << endlog;

    std::string command;

    command.append(std::string("DIM"));
    command.append(getAddress());
    
    //if (getValue() == "up")
    //    command.append(std::string("%I%D10"));
    //else if (getValue() == "down")
    //    command.append(std::string("%O%D10"));
    //else
    {
        //int value = (int)getFloatValue();
        command.append(std::string("%D"));
        command.append(get()->toString());
    }

    Services::instance()->getDomintellConnection()->write((uint8_t*)command.c_str(), command.length());
}

void DDIMObject::onWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DDIMObject: Unexpected call of onWrite"
                           << endlog;
}

void DDIMObject::onRead(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DDIMObject: Unexpected call of onRead"
                           << endlog;
}

void DDIMObject::onResponse(const uint8_t* buf, int len, eibaddr_t src)
{
    logger_m.errorStream() << "DDIMObject: Unexpected call of onResponse"
                           << endlog;
}

/*----DDIM object------------------------------------------------------------*/
#endif

BlindsObjectValue::BlindsObjectValue(const std::string& value)
{
    std::string dir;
    size_t pos = value.find(":");
    dir = value.substr(0, pos);
    stepcode_m = 1;
    if (pos != value.npos)
    {
        if (value.length() > pos+1)
        {
            char step = value[pos+1];
            if (step >= '1' && step <= '7')
                stepcode_m = step - '0';
            else
            {
                std::stringstream msg;
                msg << "BlindsObjectValue: Invalid stepcode (must be between 1 and 7): '" << step << "'" << std::endl;
                throw ticpp::Exception(msg.str());
            }
        }
    }
    if (dir == "stop")
    {
        direction_m = 0;
        stepcode_m = 0;
    }
    else if (dir == "close")
        direction_m = 1;
    else if (dir == "open")
        direction_m = 0;
    else
    {
        std::stringstream msg;
        msg << "BlindsObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string BlindsObjectValue::toString()
{
    if (stepcode_m == 0)
        return "stop";
    std::string ret(direction_m ? "close" : "open");
    if (stepcode_m != 1)
    {
        ret.push_back(':');
        ret.push_back('0' + stepcode_m);
    }
    return ret;
}

Logger& BlindsObject::logger_m(Logger::getInstance("BlindsObject"));

ObjectValue* BlindsObject::createObjectValue(const std::string& value)
{
    return new BlindsObjectValue(value);
}

void BlindsObject::setValue(const std::string& value)
{
    BlindsObjectValue val(value);
    Object::setValue(&val);
}

void BlindsObject::setStepValue(int direction, int stepcode)
{
    if (forceUpdate() || stepcode_m != stepcode  || direction_m != direction)
    {
        stepcode_m = stepcode;
        direction_m = direction;
        onInternalUpdate();
    }
}

TimeObjectValue::TimeObjectValue(const std::string& value) : wday_m(-1), hour_m(-1), min_m(-1), sec_m(-1)
{
    if (value == "now")
        return;
    std::istringstream val(value);
    char s1, s2;
    val >> hour_m >> s1 >> min_m >> s2 >> sec_m;
    wday_m = 0;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         s1 != ':' || s2 != ':' ||
         hour_m < 0 || hour_m > 23  || min_m < 0 || min_m > 59 || sec_m < 0 || sec_m > 59 )
    {
        std::stringstream msg;
        msg << "TimeObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string TimeObjectValue::toString()
{
    if (hour_m == -1)
        return "now";
    std::ostringstream out;
    out << hour_m << ":" << min_m << ":" << sec_m;
    return out.str();
}

double TimeObjectValue::toNumber()
{
    if (hour_m == -1)
        return -1.0;
    return hour_m * 3600 + min_m * 60 + sec_m;
}

bool TimeObjectValue::equals(ObjectValue* value)
{
    int wday, hour, min, sec;
    assert(value);
    TimeObjectValue* val = dynamic_cast<TimeObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "TimeObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    // logger_m.debugStream() << "TimeObjectValue (id=" << getID() << "): Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    val->getTimeValue(&wday, &hour, &min, &sec);
    return (sec_m == sec) && (min_m == min) && (hour_m == hour) && (wday_m == wday);
}

int TimeObjectValue::compare(ObjectValue* value)
{
    int wday, hour, min, sec;
    assert(value);
    TimeObjectValue* val = dynamic_cast<TimeObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "TimeObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    // logger_m.debugStream() << "TimeObjectValue (id=" << getID() << "): Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    val->getTimeValue(&wday, &hour, &min, &sec);

    if (wday_m > wday)
        return 1;
    if (wday_m < wday)
        return -1;

    if (hour_m > hour)
        return 1;
    if (hour_m < hour)
        return -1;

    if (min_m > min)
        return 1;
    if (min_m < min)
        return -1;

    if (sec_m > sec)
        return 1;
    if (sec_m < sec)
        return -1;
    return 0;
}

bool TimeObjectValue::set(ObjectValue* value)
{
    int wday, hour, min, sec;
    assert(value);
    TimeObjectValue* val = dynamic_cast<TimeObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "TimeObject: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        val->getTimeValue(&wday, &hour, &min, &sec);
        if (wday_m != wday || hour_m != hour || min_m != min || sec_m != sec)
        {
            wday_m = wday;
            hour_m = hour;
            min_m = min;
            sec_m = sec;
            return true;
        }
    }
    return false;
}

bool TimeObjectValue::set(double value)
{
    int wday, hour, min, sec;
    if (value < 0)
    {
        wday = -1;
        hour = -1;
        min = -1;
        sec = -1;
    }
    else
    {
        wday = 0;
        hour = value / 3600;
        value -= hour * 3600;
        min = value / 60;
        sec = value - min * 60;
    }
    if (wday_m != wday || hour_m != hour || min_m != min || sec_m != sec)
    {
        wday_m = wday;
        hour_m = hour;
        min_m = min;
        sec_m = sec;
        return true;
    }
    return false;
}

void TimeObjectValue::getTimeValue(int *wday, int *hour, int *min, int *sec)
{
    if (hour_m == -1)
    {
        time_t t = time(0);
        struct tm * timeinfo = localtime(&t);
        *wday = timeinfo->tm_wday;
        if (*wday == 0)
            *wday = 7;
        *hour = timeinfo->tm_hour;
        *min = timeinfo->tm_min;
        *sec = timeinfo->tm_sec;
    }
    else
    {
        *wday = wday_m;
        *hour = hour_m;
        *min = min_m;
        *sec = sec_m;
    }
}

Logger& TimeObject::logger_m(Logger::getInstance("TimeObject"));

TimeObject::TimeObject() : TimeObjectValue(0, 0, 0, 0)
{}

TimeObject::~TimeObject()
{}

ObjectValue* TimeObject::createObjectValue(const std::string& value)
{
    return new TimeObjectValue(value);
}

void TimeObject::setValue(const std::string& value)
{
    TimeObjectValue val(value);
    Object::setValue(&val);
}

void TimeObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if (len < 5)
    {
        logger_m.errorStream() << "Invalid packet received for TimeObject (too short)" << endlog;
        return;
    }
    int wday, hour, min, sec;

    wday = (buf[2] & 0xE0) >> 5;
    hour = buf[2] & 0x1F;
    min = buf[3];
    sec = buf[4];
    if (forceUpdate() || wday != wday_m || hour != hour_m || min != min_m || sec != sec_m)
    {
        wday_m = wday;
        hour_m = hour;
        min_m = min;
        sec_m = sec;
        onUpdate();
    }
}

void TimeObject::setTime(time_t time)
{
    struct tm * timeinfo = localtime(&time);
    int wday = timeinfo->tm_wday;
    if (wday == 0)
        wday = 7;
    setTime(wday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
}

void TimeObject::doSend(bool isWrite)
{
    uint8_t buf[5] = { 0, (isWrite ? 0x80 : 0x40), ((wday_m<<5) & 0xE0) | (hour_m & 0x1F), min_m, sec_m };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 5);
}

void TimeObject::setTime(int wday, int hour, int min, int sec)
{
    TimeObjectValue val(wday, hour, min, sec);
    Object::setValue(&val);
}

void TimeObject::getTime(int *wday, int *hour, int *min, int *sec)
{
    *wday = wday_m;
    *hour = hour_m;
    *min = min_m;
    *sec = sec_m;
}

DateObjectValue::DateObjectValue(const std::string& value) : day_m(-1), month_m(-1), year_m(-1)
{
    if (value == "now")
        return;
    std::istringstream val(value);
    char s1, s2;
    val >> year_m >> s1 >> month_m >> s2 >> day_m;
    year_m -= 1900;
    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         s1 != '-' || s2 != '-' ||
         year_m < 0 || year_m > 255 || month_m < 1 || month_m > 12 || day_m < 1 || day_m > 31)
    {
        std::stringstream msg;
        msg << "DateObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string DateObjectValue::toString()
{
    if (day_m == -1)
        return "now";
    std::ostringstream out;
    out << year_m+1900 << "-" << month_m << "-" << day_m;
    return out.str();
}

double DateObjectValue::toNumber()
{
    if (day_m == -1)
        return -1.0;
    return year_m * 400 + month_m * 31 + day_m;
}

bool DateObjectValue::equals(ObjectValue* value)
{
    int day, month, year;
    assert(value);
    DateObjectValue* val = dynamic_cast<DateObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream()  << "DateObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    // logger_m.debugStream() << "DateObjectValue (id=" << getID() << "): Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    val->getDateValue(&day, &month, &year);
    return (day_m == day) && (month_m == month) && (year_m == year);
}

int DateObjectValue::compare(ObjectValue* value)
{
    int day, month, year;
    assert(value);
    DateObjectValue* val = dynamic_cast<DateObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "DateObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    // logger_m.debugStream() << "DateObjectValue (id=" << getID() << "): Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    val->getDateValue(&day, &month, &year);

    if (year_m > year)
        return 1;
    if (year_m < year)
        return -1;

    if (month_m > month)
        return 1;
    if (month_m < month)
        return -1;

    if (day_m > day)
        return 1;
    if (day_m < day)
        return -1;
    return 0;
}

bool DateObjectValue::set(ObjectValue* value)
{
    int day, month, year;
    assert(value);
    DateObjectValue* val = dynamic_cast<DateObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "DateObjectValue: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        val->getDateValue(&day, &month, &year);
        if ( day_m != day || month_m != month || year_m != year )
        {
            day_m = day;
            month_m = month;
            year_m = year;
            return true;
        }
    }
    return false;
}

bool DateObjectValue::set(double value)
{
    int day, month, year;
    if (value < 0)
    {
        day = -1;
        month = -1;
        year = -1;
    }
    else
    {
        year = value / 400;
        value -= year * 400;
        month = value / 31;
        day = value - month * 31;
    }
    if ( day_m != day || month_m != month || year_m != year )
    {
        day_m = day;
        month_m = month;
        year_m = year;
        return true;
    }
    return false;
}

void DateObjectValue::getDateValue(int *day, int *month, int *year)
{
    if (day_m == -1)
    {
        time_t t = time(0);
        struct tm * timeinfo = localtime(&t);
        *day = timeinfo->tm_mday;
        *month = timeinfo->tm_mon+1;
        *year = timeinfo->tm_year;
    }
    else
    {
        *day = day_m;
        *month = month_m;
        *year = year_m;
    }
}

Logger& DateObject::logger_m(Logger::getInstance("DateObject"));

DateObject::DateObject() : DateObjectValue(0, 0, 0)
{}

DateObject::~DateObject()
{}

ObjectValue* DateObject::createObjectValue(const std::string& value)
{
    return new DateObjectValue(value);
}

void DateObject::setValue(const std::string& value)
{
    DateObjectValue val(value);
    Object::setValue(&val);
}

void DateObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if (len < 5)
    {
        logger_m.errorStream() << "Invalid packet received for DateObject (too short)" << endlog;
        return;
    }
    int day, month, year;

    day = buf[2];
    month = buf[3];
    year = buf[4];
    if (year < 90)
        year += 100;
    if (forceUpdate() || day != day_m || month != month_m || year != year_m)
    {
        day_m = day;
        month_m = month;
        year_m = year;
        onUpdate();
    }
}

void DateObject::setDate(time_t time)
{
    struct tm * timeinfo = localtime(&time);
    setDate(timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year);
}

void DateObject::doSend(bool isWrite)
{
    uint8_t buf[5] = { 0,
                       (isWrite ? 0x80 : 0x40),
                       day_m, month_m,
                       (year_m >= 100 && year_m < 190) ? year_m-100 : year_m };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 5);
}

void DateObject::setDate(int day, int month, int year)
{
    if (year >= 1900)
        year -= 1900;
    DateObjectValue val(day, month, year);
    Object::setValue(&val);
}

void DateObject::getDate(int *day, int *month, int *year)
{
    *day = day_m;
    *month = month_m;
    if (year_m < 1900)
        *year = 1900 + year_m;
    else
        *year = 1900;
}

ValueObjectValue::ValueObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         value_m > 670760.96 ||
         value_m < -671088.64)
    {
        std::stringstream msg;
        msg << "ValueObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

void ValueObjectValue::setPrecision(std::string precision)
{
    std::istringstream val(precision);
    val >> precision_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        std::stringstream msg;
        msg << "ValueObjectValue: Bad precision: '" << precision << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string ValueObjectValue::toString()
{
    std::ostringstream out;
    out.precision(8);
    out << value_m;
    return out.str();
}

double ValueObjectValue::toNumber()
{
    return value_m;
}

bool ValueObjectValue::equals(ObjectValue* value)
{
    assert(value);
    ValueObjectValue* val = dynamic_cast<ValueObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "ValueObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "ValueObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return value_m == val->value_m;
}

int ValueObjectValue::compare(ObjectValue* value)
{
    assert(value);
    ValueObjectValue* val = dynamic_cast<ValueObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "ValueObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "ValueObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;

    if (value_m == val->value_m)
        return 0;
    if (value_m > val->value_m)
        return 1;
    else
        return -1;
}

bool ValueObjectValue::set(ObjectValue* value)
{
    assert(value);
    ValueObjectValue* val = dynamic_cast<ValueObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "ValueObject: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        return set(val->value_m);
    }
    return false;
}

bool ValueObjectValue::set(double value)
{
    if (precision_m != 0) {
        int div = (int) (value/precision_m + (value >= 0 ? 0.5 : -0.5));
        value = div*precision_m;
        logger_m.debugStream() << "ValueObject: rounded value "<< value << endlog;
    }
    if (value_m != value)
    {
        value_m = value;
        return true;
    }
    return false;
}

Logger& ValueObject::logger_m(Logger::getInstance("ValueObject"));

ValueObject::ValueObject() : ValueObjectValue(0)
{}

ValueObject::~ValueObject()
{}

ObjectValue* ValueObject::createObjectValue(const std::string& value)
{
    return new ValueObjectValue(value);
}

void ValueObject::setValue(const std::string& value)
{
    ValueObjectValue val(value);
    Object::setValue(&val);
}

void ValueObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if (len < 4)
    {
        logger_m.errorStream() << "Invalid packet received for ValueObject (too short)" << endlog;
        return;
    }
    double newValue;
    int d1 = ((unsigned char) buf[2]) * 256 + (unsigned char) buf[3];
    int m = d1 & 0x7ff;
    if (d1 & 0x8000)
        m |= ~0x7ff;
    int ex = (d1 & 0x7800) >> 11;
    newValue = ((double)m * (1 << ex) / 100);
    if (set(newValue) || forceUpdate())
    {
        onUpdate();
    }
}

void ValueObject::doSend(bool isWrite)
{
    uint8_t buf[4] = { 0, (isWrite ? 0x80 : 0x40), 0, 0 };
    int ex = 0;
    int m = (int)rint(value_m * 100);
    if (m < 0)
    {
        m = -m;
        while (m > 2048)
        {
            m = m >> 1;
            ex++;
        }
        m = -m;
        buf[2] = ((m >> 8) & 0x07) | ((ex << 3) & 0x78) | (1 << 7);
    }
    else
    {
        while (m > 2047)
        {
            m = m >> 1;
            ex++;
        }
        buf[2] = ((m >> 8) & 0x07) | ((ex << 3) & 0x78);
    }
    buf[3] = (m & 0xff);

    Services::instance()->getKnxConnection()->write(getGad(), buf, 4);
}

void ValueObject::setFloatValue(double value)
{
    ValueObjectValue val(value);
    Object::setValue(&val);
}

ValueObject32Value::ValueObject32Value(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        std::stringstream msg;
        msg << "ValueObject32Value: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string ValueObject32Value::toString()
{
    std::ostringstream out;
    out.precision(8);
    out << value_m;
    return out.str();
}

Logger& ValueObject32::logger_m(Logger::getInstance("valueObject32"));

ObjectValue* ValueObject32::createObjectValue(const std::string& value)
{
    return new ValueObject32Value(value);
}

void ValueObject32::setValue(const std::string& value)
{
    ValueObject32Value val(value);
    Object::setValue(&val);
}

void ValueObject32::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if (len < 6)
    {
        logger_m.errorStream() << "Invalid packet received for ValueObject32 (too short)" << endlog;
        return;
    }

    convfloat tmp;
    tmp.u32 = buf[2]<<24 | buf[3]<<16 | buf[4]<<8 | buf[5];
//    logger_m.infoStream() << "New value int tmp " << tmp << " for ValueObject32 " << getID() << endlog;
//    const float* nv = reinterpret_cast<const float*>(&tmp);
    double newValue = tmp.fl;
    if (set(newValue) || forceUpdate())
    {
        onUpdate();
    }
}

void ValueObject32::doSend(bool isWrite)
{
    uint8_t buf[6] = { 0, (isWrite ? 0x80 : 0x40), 0, 0, 0, 0 };
    convfloat tmp;
    tmp.fl = static_cast<float>(value_m);
    buf[5] = static_cast<uint8_t>(tmp.u32 & 0x000000FF);
    buf[4] = static_cast<uint8_t>((tmp.u32 & 0x0000FF00) >> 8);
    buf[3] = static_cast<uint8_t>((tmp.u32 & 0x00FF0000) >> 16);
    buf[2] = static_cast<uint8_t>((tmp.u32 & 0xFF000000) >> 24);

    Services::instance()->getKnxConnection()->write(getGad(), buf, 6);
}

UIntObjectValue::UIntObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        std::stringstream msg;
        msg << "UIntObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string UIntObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

double UIntObjectValue::toNumber()
{
    return value_m;
}

bool UIntObjectValue::equals(ObjectValue* value)
{
    assert(value);
    UIntObjectValue* val = dynamic_cast<UIntObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "UIntObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "UIntObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return value_m == val->value_m;
}

int UIntObjectValue::compare(ObjectValue* value)
{
    assert(value);
    UIntObjectValue* val = dynamic_cast<UIntObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "UIntObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "UIntObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;

    if (value_m == val->value_m)
        return 0;
    if (value_m > val->value_m)
        return 1;
    else
        return -1;
}

bool UIntObjectValue::set(ObjectValue* value)
{
    assert(value);
    UIntObjectValue* val = dynamic_cast<UIntObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "UIntObjectValue: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        if (value_m != val->value_m)
        {
            value_m = val->value_m;
            return true;
        }
    }
    return false;
}

Logger& UIntObject::logger_m(Logger::getInstance("UIntObject"));

UIntObject::UIntObject()
{}

UIntObject::~UIntObject()
{}

void UIntObject::setIntValue(uint32_t value)
{
    if (setInt(value) || forceUpdate())
        onInternalUpdate();
}

uint32_t UIntObject::getIntValue()
{
    if (!init_m)
        read();
    return getInt();
}

Logger& U8ImplObject::logger_m(Logger::getInstance("U8ImplObject"));

U8ImplObject::U8ImplObject()
{}

U8ImplObject::~U8ImplObject()
{}

void U8ImplObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    uint32_t newValue;
    if (len == 2)
        newValue = (buf[1] & 0x3F);
    else
        newValue = buf[2];
    if (setInt(newValue) || forceUpdate())
        onUpdate();
}

void U8ImplObject::doSend(bool isWrite)
{
    uint8_t buf[3] = { 0, (isWrite ? 0x80 : 0x40), (getInt() & 0xff) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 3);
}

U8ObjectValue::U8ObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         value_m > 255 ||
         value_m < 0)
    {
        std::stringstream msg;
        msg << "U8ObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string U8ObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

Logger& U8Object::logger_m(Logger::getInstance("U8Object"));

U8Object::U8Object()
{}

U8Object::~U8Object()
{}

ObjectValue* U8Object::createObjectValue(const std::string& value)
{
    return new U8ObjectValue(value);
}

void U8Object::setValue(const std::string& value)
{
    U8ObjectValue val(value);
    Object::setValue(&val);
}

ScalingObjectValue::ScalingObjectValue(const std::string& value)
{
    float fvalue;
    std::istringstream val(value);
    val >> fvalue;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         fvalue > 100 ||
         fvalue < 0)
    {
        std::stringstream msg;
        msg << "ScalingObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
    value_m = (int)(fvalue * 255 / 100);
}

std::string ScalingObjectValue::toString()
{
    std::ostringstream out;
    out.precision(3);
    out << (float)value_m * 100 / 255;
    return out.str();
}

Logger& ScalingObject::logger_m(Logger::getInstance("ScalingObject"));

ScalingObject::ScalingObject(): ScalingObjectValue(0)
{}

ScalingObject::~ScalingObject()
{}

ObjectValue* ScalingObject::createObjectValue(const std::string& value)
{
    return new ScalingObjectValue(value);
}

void ScalingObject::setValue(const std::string& value)
{
    ScalingObjectValue val(value);
    Object::setValue(&val);
}

AngleObjectValue::AngleObjectValue(const std::string& value)
{
    float fvalue;
    std::istringstream val(value);
    val >> fvalue;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         fvalue > 360 ||
         fvalue < 0)
    {
        std::stringstream msg;
        msg << "AngleObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
    value_m = ((int)(fvalue * 256 / 360)) % 256;
}

std::string AngleObjectValue::toString()
{
    std::ostringstream out;
    out.precision(4);
    out << (float)value_m * 360 / 256;
    return out.str();
}

Logger& AngleObject::logger_m(Logger::getInstance("AngleObject"));

AngleObject::AngleObject() : AngleObjectValue(0)
{}

AngleObject::~AngleObject()
{}

ObjectValue* AngleObject::createObjectValue(const std::string& value)
{
    return new AngleObjectValue(value);
}

void AngleObject::setValue(const std::string& value)
{
    AngleObjectValue val(value);
    Object::setValue(&val);
}

HeatingModeObjectValue::HeatingModeObjectValue(const std::string& value)
{
    if (value == "comfort")
        value_m = 1;
    else if (value == "standby")
        value_m = 2;
    else if (value == "night")
        value_m = 3;
    else if (value == "frost")
        value_m = 4;
    else if (value == "auto")
        value_m = 0;
    else
    {
        std::stringstream msg;
        msg << "HeatingModeObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string HeatingModeObjectValue::toString()
{
    switch (value_m)
    {
    case 0:
        return "auto";
    case 1:
        return "comfort";
    case 2:
        return "standby";
    case 3:
        return "night";
    case 4:
        return "frost";
    }
    return "frost";
}

Logger& HeatingModeObject::logger_m(Logger::getInstance("HeatingModeObject"));

ObjectValue* HeatingModeObject::createObjectValue(const std::string& value)
{
    return new HeatingModeObjectValue(value);
}

void HeatingModeObject::setValue(const std::string& value)
{
    HeatingModeObjectValue val(value);
    Object::setValue(&val);
}

U16ObjectValue::U16ObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         value_m > 65535 ||
         value_m < 0)
    {
        std::stringstream msg;
        msg << "U16ObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string U16ObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

Logger& U16Object::logger_m(Logger::getInstance("U16Object"));

U16Object::U16Object()
{}

U16Object::~U16Object()
{}

ObjectValue* U16Object::createObjectValue(const std::string& value)
{
    return new U16ObjectValue(value);
}

void U16Object::setValue(const std::string& value)
{
    U16ObjectValue val(value);
    Object::setValue(&val);
}

void U16Object::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    unsigned int newValue;
    newValue = (buf[2]<<8) | buf[3];
    if (forceUpdate() || newValue != value_m)
    {
        value_m = newValue;
        onUpdate();
    }
}

void U16Object::doSend(bool isWrite)
{
    uint8_t buf[4] = { 0, (isWrite ? 0x80 : 0x40), ((value_m & 0xff00)>>8), (value_m & 0xff) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 4);
}

U32ObjectValue::U32ObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        std::stringstream msg;
        msg << "U32ObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string U32ObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

Logger& U32Object::logger_m(Logger::getInstance("U32Object"));

U32Object::U32Object()
{}

U32Object::~U32Object()
{}

ObjectValue* U32Object::createObjectValue(const std::string& value)
{
    return new U32ObjectValue(value);
}

void U32Object::setValue(const std::string& value)
{
    U32ObjectValue val(value);
    Object::setValue(&val);
}

void U32Object::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    unsigned int newValue;
    newValue = (buf[2]<<24) | (buf[3]<<16) | (buf[4]<<8) | buf[5];
    if (forceUpdate() || newValue != value_m)
    {
        value_m = newValue;
        onUpdate();
    }
}

void U32Object::doSend(bool isWrite)
{
    uint8_t buf[6] = { 0, (isWrite ? 0x80 : 0x40), ((value_m & 0xff000000)>>24), ((value_m & 0xff0000)>>16), ((value_m & 0xff00)>>8), (value_m & 0xff) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 6);
}

IntObjectValue::IntObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        std::stringstream msg;
        msg << "IntObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string IntObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

double IntObjectValue::toNumber()
{
    return value_m;
}

bool IntObjectValue::equals(ObjectValue* value)
{
    assert(value);
    IntObjectValue* val = dynamic_cast<IntObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "IntObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "IntObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return value_m == val->value_m;
}

int IntObjectValue::compare(ObjectValue* value)
{
    assert(value);
    IntObjectValue* val = dynamic_cast<IntObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "IntObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "IntObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;

    if (value_m == val->value_m)
        return 0;
    if (value_m > val->value_m)
        return 1;
    else
        return -1;
}

bool IntObjectValue::set(ObjectValue* value)
{
    assert(value);
    IntObjectValue* val = dynamic_cast<IntObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "IntObjectValue: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        if (value_m != val->value_m)
        {
            value_m = val->value_m;
            return true;
        }
    }
    return false;
}

Logger& IntObject::logger_m(Logger::getInstance("IntObject"));

IntObject::IntObject()
{}

IntObject::~IntObject()
{}

void IntObject::setIntValue(int32_t value)
{
    if (setInt(value) || forceUpdate())
        onInternalUpdate();
}

int32_t IntObject::getIntValue()
{
    if (!init_m)
        read();
    return getInt();
}

S8ObjectValue::S8ObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         value_m > 127 ||
         value_m < -128)
    {
        std::stringstream msg;
        msg << "S8ObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string S8ObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

Logger& S8Object::logger_m(Logger::getInstance("S8Object"));

S8Object::S8Object()
{}

S8Object::~S8Object()
{}

ObjectValue* S8Object::createObjectValue(const std::string& value)
{
    return new S8ObjectValue(value);
}

void S8Object::setValue(const std::string& value)
{
    S8ObjectValue val(value);
    Object::setValue(&val);
}

void S8Object::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    int32_t newValue;
    if (len == 2)
        newValue = (buf[1] & 0x3F);
    else
        newValue = buf[2];
    if (newValue > 127)
        newValue -= 256;
    if (forceUpdate() || newValue != value_m)
    {
        value_m = newValue;
        onUpdate();
    }
}

void S8Object::doSend(bool isWrite)
{
    uint8_t buf[3] = { 0, (isWrite ? 0x80 : 0x40), (value_m & 0xff) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 3);
}

S16ObjectValue::S16ObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof() || // workaround for wrong val.eof() flag in uClibc++
         value_m > 32767 ||
         value_m < -32768)
    {
        std::stringstream msg;
        msg << "S16ObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string S16ObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

Logger& S16Object::logger_m(Logger::getInstance("S16Object"));

S16Object::S16Object()
{}

S16Object::~S16Object()
{}

ObjectValue* S16Object::createObjectValue(const std::string& value)
{
    return new S16ObjectValue(value);
}

void S16Object::setValue(const std::string& value)
{
    S16ObjectValue val(value);
    Object::setValue(&val);
}

void S16Object::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    int32_t newValue;
    newValue = (buf[2]<<8) | buf[3];
    if (newValue > 32767)
        newValue -= 65536;
    if (forceUpdate() || newValue != value_m)
    {
        value_m = newValue;
        onUpdate();
    }
}

void S16Object::doSend(bool isWrite)
{
    uint8_t buf[4] = { 0, (isWrite ? 0x80 : 0x40), ((value_m & 0xff00)>>8), (value_m & 0xff) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 4);
}

S32ObjectValue::S32ObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        std::stringstream msg;
        msg << "S32ObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string S32ObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

Logger& S32Object::logger_m(Logger::getInstance("S32Object"));

S32Object::S32Object()
{}

S32Object::~S32Object()
{}

ObjectValue* S32Object::createObjectValue(const std::string& value)
{
    return new S32ObjectValue(value);
}

void S32Object::setValue(const std::string& value)
{
    S32ObjectValue val(value);
    Object::setValue(&val);
}

void S32Object::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    int32_t newValue;
    newValue = (buf[2]<<24) | (buf[3]<<16) | (buf[4]<<8) | buf[5];
    if (forceUpdate() || newValue != value_m)
    {
        value_m = newValue;
        onUpdate();
    }
}

void S32Object::doSend(bool isWrite)
{
    uint8_t buf[6] = { 0, (isWrite ? 0x80 : 0x40), ((value_m & 0xff000000)>>24), ((value_m & 0xff0000)>>16), ((value_m & 0xff00)>>8), (value_m & 0xff) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 6);
}

#ifdef STL_STREAM_SUPPORT_INT64
S64ObjectValue::S64ObjectValue(const std::string& value)
{
    std::istringstream val(value);
    val >> value_m;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        std::stringstream msg;
        msg << "S64ObjectValue: Bad value: '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

std::string S64ObjectValue::toString()
{
    std::ostringstream out;
    out << value_m;
    return out.str();
}

double S64ObjectValue::toNumber()
{
    return value_m;
}

bool S64ObjectValue::equals(ObjectValue* value)
{
    assert(value);
    S64ObjectValue* val = dynamic_cast<S64ObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "S64ObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "S64ObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return value_m == val->value_m;
}

int S64ObjectValue::compare(ObjectValue* value)
{
    assert(value);
    S64ObjectValue* val = dynamic_cast<S64ObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "S64ObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "S64ObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;

    if (value_m == val->value_m)
        return 0;
    if (value_m > val->value_m)
        return 1;
    else
        return -1;
}

bool S64ObjectValue::set(ObjectValue* value)
{
    assert(value);
    S64ObjectValue* val = dynamic_cast<S64ObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "S64ObjectValue: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        if (value_m != val->value_m)
        {
            value_m = val->value_m;
            return true;
        }
    }
    return false;
}

Logger& S64Object::logger_m(Logger::getInstance("S64Object"));

S64Object::S64Object()
{}

S64Object::~S64Object()
{}

ObjectValue* S64Object::createObjectValue(const std::string& value)
{
    return new S64ObjectValue(value);
}

void S64Object::setIntValue(int64_t value)
{
    if (setInt(value) || forceUpdate())
        onInternalUpdate();
}

int64_t S64Object::getIntValue()
{
    if (!init_m)
        read();
    return getInt();
}

void S64Object::setValue(const std::string& value)
{
    S64ObjectValue val(value);
    if (set(&val) || forceUpdate())
        onInternalUpdate();
}

void S64Object::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    int64_t newValue;
    newValue = ((int64_t)buf[2]<<56) | ((int64_t)buf[3]<<48) | ((int64_t)buf[4]<<40) | ((int64_t)buf[5]<<32) | (buf[6]<<24) | (buf[7]<<16) | (buf[8]<<8) | buf[9];
    if (forceUpdate() || newValue != value_m)
    {
        value_m = newValue;
        onUpdate();
    }
}

void S64Object::doSend(bool isWrite)
{
    uint8_t buf[10] = { 0, (isWrite ? 0x80 : 0x40), ((value_m & 0xff00000000000000LL)>>56), ((value_m & 0xff000000000000LL)>>48), ((value_m & 0xff0000000000LL)>>40), ((value_m & 0xff00000000LL)>>32),
                                                    ((value_m & 0xff000000LL)>>24), ((value_m & 0xff0000LL)>>16), ((value_m & 0xff00LL)>>8), (value_m & 0xffLL) };
    Services::instance()->getKnxConnection()->write(getGad(), buf, 10);
}
#endif

StringObjectValue::StringObjectValue(const std::string& value)
{
    value_m = value;
//    logger_m.debugStream() << "StringObjectValue: Value: '" << value_m << "'" << endlog;
}

std::string StringObjectValue::toString()
{
    return value_m;
}

double StringObjectValue::toNumber()
{
    std::istringstream val(value_m);
    double value;
    val >> value;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        value = 0;
    }
    return value;
}

bool StringObjectValue::equals(ObjectValue* value)
{
    assert(value);
    StringObjectValue* val = dynamic_cast<StringObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "StringObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "StringObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return value_m == val->value_m;
}

int StringObjectValue::compare(ObjectValue* value)
{
    assert(value);
    StringObjectValue* val = dynamic_cast<StringObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "StringObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return -1;
    }
    logger_m.infoStream() << "StringObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;

    if (value_m == val->value_m)
        return 0;
    if (value_m < val->value_m)
        return -1;
    else
        return 1;
}

bool StringObjectValue::set(ObjectValue* value)
{
    assert(value);
    StringObjectValue* val = dynamic_cast<StringObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "StringObject: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        if (value_m != val->value_m)
        {
            value_m = val->value_m;
            return true;
        }
    }
    return false;
}

bool StringObjectValue::set(double value)
{
    std::ostringstream out;
    out << value;
    if (value_m != out.str())
    {
        value_m = out.str();
        return true;
    }
    return false;
}

Logger& StringObject::logger_m(Logger::getInstance("StringObject"));

StringObject::StringObject()
{}

StringObject::~StringObject()
{}

ObjectValue* StringObject::createObjectValue(const std::string& value)
{
    return new StringObjectValue(value);
}

void StringObject::setValue(const std::string& value)
{
    StringObjectValue val(value);
    Object::setValue(&val);
}

void StringObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if (len < 2)
    {
        logger_m.errorStream() << "Invalid packet received for StringObject (too short)" << endlog;
        return;
    }
    std::string value;
    for(int j=2; j<len && buf[j]!=0; j++)
        value.push_back(buf[j]);
    StringObjectValue val(value);

    if (set(&val) || forceUpdate())
        onUpdate();
}

void StringObject::doSend(bool isWrite)
{
    logger_m.debugStream() << "StringObject: Value: " << value_m << endlog;
    uint bufsz = value_m.size()+3;
    uint8_t *buf = new uint8_t[bufsz];
    memset(buf,0,bufsz);
    buf[1] = (isWrite ? 0x80 : 0x40);
    // Convert to hex
    for(uint j=2;j<bufsz;j++)
        buf[j] = static_cast<uint8_t>(value_m[j-2]);

    Services::instance()->getKnxConnection()->write(getGad(), buf, bufsz);
    delete buf;
}

void StringObject::setStringValue(const std::string& value)
{
    StringObjectValue val(value);
    Object::setValue(&val);
}

String14ObjectValue::String14ObjectValue(const std::string& value): StringObjectValue(value)
{
    if ( value.length() > 14)
    {
        std::stringstream msg;
        msg << "String14ObjectValue: Bad value (too long): '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

String14AsciiObjectValue::String14AsciiObjectValue(const std::string& value): StringObjectValue(value)
{
    if ( value.length() > 14)
    {
        std::stringstream msg;
        msg << "String14AsciiObjectValue: Bad value (too long): '" << value << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
    std::string::const_iterator it = value.begin();
    while ( it != value.end())
    {
        if (*it < 0 || *it > 127)
        {
            std::stringstream msg;
            msg << "String14AsciiObjectValue: Bad value (invalid character): '" << value << "'" << std::endl;
            throw ticpp::Exception(msg.str());
        }
        ++it;
    }
}

Logger& String14Object::logger_m(Logger::getInstance("String14Object"));

String14Object::String14Object()
{}

String14Object::~String14Object()
{}

ObjectValue* String14Object::createObjectValue(const std::string& value)
{
    return new String14ObjectValue(value);
}

void String14Object::setValue(const std::string& value)
{
    String14ObjectValue val(value);
    Object::setValue(&val);
}

void String14Object::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if (len < 2)
    {
        logger_m.errorStream() << "Invalid packet received for String14Object (too short)" << endlog;
        return;
    }
    std::string value;
    for(int j=2; j<len && buf[j]!=0; j++)
        value.push_back(buf[j]);
    String14ObjectValue val(value);

    if (set(&val) || forceUpdate())
        onUpdate();
}

void String14Object::doSend(bool isWrite)
{
    logger_m.debugStream() << "String14Object: Value: " << value_m << endlog;
    uint8_t buf[16];
    memset(buf,0,sizeof(buf));
    buf[1] = (isWrite ? 0x80 : 0x40);
    // Convert to hex
    for(uint j=0;j<value_m.size();j++)
        buf[j+2] = static_cast<uint8_t>(value_m[j]);

    Services::instance()->getKnxConnection()->write(getGad(), buf, sizeof(buf));
}

void String14Object::setStringValue(const std::string& value)
{
    String14ObjectValue val(value);
    Object::setValue(&val);
}

Logger& String14AsciiObject::logger_m(Logger::getInstance("String14AsciiObject"));

String14AsciiObject::String14AsciiObject()
{}

String14AsciiObject::~String14AsciiObject()
{}

ObjectValue* String14AsciiObject::createObjectValue(const std::string& value)
{
    return new String14AsciiObjectValue(value);
}

void String14AsciiObject::setValue(const std::string& value)
{
    String14AsciiObjectValue val(value);
    Object::setValue(&val);
}

void String14AsciiObject::doWrite(const uint8_t* buf, int len, eibaddr_t src)
{
    if (len < 2)
    {
        logger_m.errorStream() << "Invalid packet received for String14AsciiObject (too short)" << endlog;
        return;
    }
    std::string value;
    for(int j=2; j<len && buf[j]!=0; j++)
        value.push_back(buf[j]);
    String14AsciiObjectValue val(value);

    if (set(&val) || forceUpdate())
        onUpdate();
}

void String14AsciiObject::doSend(bool isWrite)
{
    logger_m.debugStream() << "String14AsciiObject: Value: " << value_m << endlog;
    uint8_t buf[16];
    memset(buf,0,sizeof(buf));
    buf[1] = (isWrite ? 0x80 : 0x40);
    // Convert to hex
    for(uint j=0;j<value_m.size();j++)
        buf[j+2] = static_cast<uint8_t>(value_m[j]);

    Services::instance()->getKnxConnection()->write(getGad(), buf, sizeof(buf));
}

void String14AsciiObject::setStringValue(const std::string& value)
{
    String14AsciiObjectValue val(value);
    Object::setValue(&val);
}

#ifdef OPEN_HOME_AUTOMATION
Logger& ContactObject::logger_m(Logger::getInstance("ContactObject"));

ContactObject::ContactObject()
{}

ContactObject::~ContactObject()
{}

void ContactObject::importXml(ticpp::Element* pConfig)
{
    Object::importXml(pConfig);

    std::string fName = pConfig->GetAttribute("firstName");
    if (fName == "")
        throw ticpp::Exception("Missing or empty first name");

    if (firstName_m == "")
        firstName_m = fName;

    std::string lName = pConfig->GetAttribute("lastName");
    if (lName == "")
        throw ticpp::Exception("Missing or empty last name");

    if (lastName_m == "")
        lastName_m = lName;

    std::string hPhone = pConfig->GetAttribute("homePhone");
    if (hPhone != "" && homePhone_m == "")
        homePhone_m = hPhone;

    std::string oPhone = pConfig->GetAttribute("officePhone");
    if (oPhone != "" && officePhone_m == "")
        officePhone_m = oPhone;

    std::string mPhone = pConfig->GetAttribute("mobilePhone");
    if (mPhone != "" && mobilePhone_m == "")
        mobilePhone_m = mPhone;

    std::string email = pConfig->GetAttribute("email");
    if (email != "" && email_m == "")
        email_m = email;

    std::string birthday = pConfig->GetAttribute("birthday");
    if (birthday != "" && birthday_m == "")
    {
        if(birthday.find("/", 0) == std::string::npos)
        {
		std::stringstream msg;
		msg << "Slash character '/' missing in birthday: " << birthday <<std::endl;
		throw ticpp::Exception(msg.str());
    	}

        birthday_m = birthday;
    }
}

void ContactObject::exportXml(ticpp::Element* pConfig)
{
    Object::exportXml(pConfig);

    if (firstName_m != "")
        pConfig->SetAttribute("firstName", firstName_m);

    if (lastName_m != "")
        pConfig->SetAttribute("lastName", lastName_m);

    if (homePhone_m != "")
        pConfig->SetAttribute("homePhone", homePhone_m);

    if (officePhone_m != "")
        pConfig->SetAttribute("officePhone", officePhone_m);

    if (mobilePhone_m != "")
        pConfig->SetAttribute("mobilePhone", mobilePhone_m);

    if (email_m != "")
        pConfig->SetAttribute("email", email_m);

    if (birthday_m != "")
        pConfig->SetAttribute("birthday", birthday_m);
}

LocationObjectValue::LocationObjectValue(const std::string& value)
{
    value_m = value;
}

std::string LocationObjectValue::toString()
{
    return value_m;
}

double LocationObjectValue::toNumber()
{
    std::istringstream val(value_m);
    double value;
    val >> value;

    if ( val.fail() ||
         val.peek() != std::char_traits<char>::eof()) // workaround for wrong val.eof() flag in uClibc++
    {
        value = 0;
    }
    return value;
}

#include <cmath>

bool LocationObjectValue::equals(ObjectValue* value)
{
    assert(value);
    LocationObjectValue* val = dynamic_cast<LocationObjectValue*>(value);
    if (val == 0)
    {
        logger_m.errorStream() << "LocationObjectValue: ERROR, equals() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return false;
    }
    logger_m.infoStream() << "LocationObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;
    return value_m == val->value_m;
}

int LocationObjectValue::compare(ObjectValue* value)
{
    assert(value);
    LocationObjectValue* val =
        dynamic_cast<LocationObjectValue*>(value);

    if (val == 0)
    {
        logger_m.errorStream() << "LocationObjectValue: ERROR, compare() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
        return -1;
    }

    logger_m.infoStream() << "LocationObjectValue: Compare value_m='" << value_m << "' to value='" << val->value_m << "'" << endlog;

    int offset = value_m.find(";");
    
    std::string str1 = value_m.substr(0, offset);
    std::istringstream val1(str1);
    double lat1;
    val1 >> lat1;

    std::string str2 = value_m.substr(offset + 1);
    std::istringstream val2(str2);
    double long1;
    val2 >> long1;

    offset = val->value_m.find(";");

    std::string str3 = val->value_m.substr(0, offset);
    std::istringstream val3(str3);
    double lat2;
    val3 >> lat2;

    std::string str4 = val->value_m.substr(offset + 1);
    std::istringstream val4(str4);
    double long2;
    val4 >> long2;

        double PI = 4.0*atan(1.0);
        
        //main code inside the class
        double dlat1=lat1*(PI/180);

        double dlong1=long1*(PI/180);
        double dlat2=lat2*(PI/180);
        double dlong2=long2*(PI/180);

        double dLong=dlong1-dlong2;
        double dLat=dlat1-dlat2;

        double aHarv= pow(sin(dLat/2.0),2.0)+cos(dlat1)*cos(dlat2)*pow(sin(dLong/2),2);
        double cHarv=2*atan2(sqrt(aHarv),sqrt(1.0-aHarv));
        //earth's radius from wikipedia varies between 6,356.750 km — 6,378.135 km (˜3,949.901 — 3,963.189 miles)
        //The IUGG value for the equatorial radius of the Earth is 6378.137 km (3963.19 mile)
        const double earth=6378.137;//I am doing miles, just change this to radius in kilometers to get distances in km
        double distance=earth*cHarv;
 
    logger_m.infoStream() << "LocationObjectValue: Compare result: "<< distance << endlog;
    return distance*1000;
}

bool LocationObjectValue::set(ObjectValue* value)
{
    assert(value);
    LocationObjectValue* val = dynamic_cast<LocationObjectValue*>(value);
    if (val == 0)
        logger_m.errorStream() << "LocationObject: ERROR, setValue() received invalid class object (typeid=" << typeid(*value).name() << ")" << endlog;
    else
    {
        if (value_m != val->value_m)
        {
            value_m = val->value_m;
            return true;
        }
    }
    return false;
}

bool LocationObjectValue::set(double value)
{
    std::ostringstream out;
    out << value;
    if (value_m != out.str())
    {
        value_m = out.str();
        return true;
    }
    return false;
}

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

Logger& LocationObject::logger_m(Logger::getInstance("LocationObject"));

LocationObject::LocationObject()
{}

LocationObject::~LocationObject()
{
    if (task_m)
        delete(task_m);
    task_m = NULL;
}

ObjectValue* LocationObject::createObjectValue(const std::string& value)
{
    return new LocationObjectValue(value);
}

void LocationObject::setValue(const std::string& value)
{
    LocationObjectValue val(value);
    Object::setValue(&val);
}

/* the function to invoke as the data recieved */
size_t static write_callback_func(void *buffer,
                        size_t size,
                        size_t nmemb,
                        void *userp)
{
    char **response_ptr =  (char**)userp;

    *response_ptr = strndup((const char *)buffer, (size_t)(size *nmemb));
    return nmemb*size; 
}


void LocationObject::importXml(ticpp::Element* pConfig)
{
    Object::importXml(pConfig);

    std::string id = pConfig->GetAttribute("id");
    if (id == "")
        throw ticpp::Exception("Missing or empty id");
    if (id_m == "")
        id_m = id;

    std::string lat_id = pConfig->GetAttribute("badge");
    if (lat_id != "" && badge_m == "")
        badge_m = lat_id;

    std::string location = pConfig->GetAttribute("location");
    if (location != "" && location_m == "")
        location_m = location;
 
    std::string interval = pConfig->GetAttribute("interval");
    if (interval != "")
    {
        interval_m = 0;
        std::istringstream val(interval);
        val >> interval_m;
    }

    if (badge_m == "" && location_m == "")
      throw ticpp::Exception("Missing location or badge id");
    
    if (badge_m != "" && location_m != "")
      throw ticpp::Exception("Cannot have both static location and badge id");

    if (badge_m != "")
    {
        if (interval_m == 0)
            interval_m = 300;

        task_m = new PeriodicTask(this);
        task_m->setAfter(interval_m);
        task_m->reschedule(0);
    }

    if (location_m != "")
    {
#ifndef HAVE_LIBCURL
        throw ticpp::Exception("LocationObject: LibCuRL not available");
#else
        //http://maps.google.com/maps/geo?q=copernicuslaan%2050,%20antwerpen&output=json
        CURL *curl;
        CURLcode res;
        char *response = NULL;
    
        curl = curl_easy_init();
        if(!curl)
            throw ticpp::Exception("LocationObject: Failed to initialize LibCuRL");

        char *curlEscapedLocation = curl_easy_escape(curl, location_m.c_str(), strlen(location_m.c_str()));
        if (!curlEscapedLocation)
        {
            curl_easy_cleanup(curl);
            throw ticpp::Exception("LocationObject: Failed to parse location");
        }

        std::stringstream msg;
        msg << "http://maps.google.com/maps/geo?q=" << curlEscapedLocation << "&output=kml";
        std::string url = msg.str();

        //curl_easy_setopt(curl, CURLOPT_VERBOSE, TRUE);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_func);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        curl_free(curlEscapedLocation);
        curl_easy_cleanup(curl);
 
        if (res != 0)
	    throw ticpp::Exception("LocationObject: LibCuRL returned an error");

        try
        {
            ticpp::Document doc;
            doc.LoadFromString(response); 

            ticpp::Element* pKml = doc.FirstChildElement("kml");
            if (pKml)
            {
                ticpp::Element* pDocument = pKml->FirstChildElement("Response");
                if (pDocument)
                {
                    ticpp::Element* pPlacemark = pDocument->FirstChildElement("Placemark");
                    if (pPlacemark)
                    {
                        ticpp::Element* pPoint = pPlacemark->FirstChildElement("Point");
                        if (pPoint)
                        {
                            ticpp::Element* pCoordinates = pPoint->FirstChildElement("coordinates");
                            if (pCoordinates)
                            {
                               ticpp::Iterator< ticpp::Node > child;
                               child = pCoordinates->FirstChild(false);
                               std::string val = child->Value();
                               if (child->Type() == TiXmlNode::TEXT && val.length())
                               {
                                   int offset = val.find(',');
                                   if (offset != std::string::npos)
                                   {
                                       std::string lat = val.substr(offset+1);
                                       std::string lon = val.substr(0, offset);
                                       std::string newLocation = "";
                                       offset = lat.find(',');
                                       if (offset != std::string::npos)
                                           newLocation.append(lat.substr(0, offset));
                                       else
                                           newLocation.append(lat);
                                       newLocation.append(";");
                                       newLocation.append(lon);
                                       logger_m.infoStream() << "location for address: " << newLocation << endlog;
                                       setValue(newLocation);
                                   }
                                }
                            }
                            else
                            {
                                logger_m.errorStream() << "coordinates node not found" << endlog;
                            }
                        }
                        else
                        {
                            logger_m.errorStream() << "Point node not found" << endlog;
                        }
                    }
                    else
                    {
                        logger_m.errorStream() << "Placemark node not found" << endlog;
                    }
                }
                else
                {
                    logger_m.errorStream() << "Response node not found" << endlog;
                }
            }
            else
            {
                logger_m.errorStream() << "KML node not found" << endlog;
            }
        }
        catch( ticpp::Exception& ex )
        {
            logger_m.errorStream() << "Parsing failed: " << ex.m_details << endlog;
        }
#endif
    }
}

void LocationObject::exportXml(ticpp::Element* pConfig)
{
    Object::exportXml(pConfig);

    if (id_m != "")
        pConfig->SetAttribute("id", id_m);

    if (badge_m != "")
    {
        pConfig->SetAttribute("badge", badge_m);
        pConfig->SetAttribute("interval", interval_m);    
    }

    if (location_m != "")
        pConfig->SetAttribute("location", location_m);
}

void LocationObject::onChange(Object* object)
{
    if (task_m->getValue() == true)
    {
        /*
         * Timer will fire 2 events, one with value true to start an action,
         * one with value false to stop the action. Only respond when the
         * value is false here.
         */
        return;
    }

    logger_m.infoStream() << "LocationObject: fetching location" << endlog;

#ifdef HAVE_LIBCURL
    CURL *curl;
    CURLcode res;
    char *response = NULL;
    
    curl = curl_easy_init();
    if(curl)
    {
        std::stringstream msg;
        msg << "http://www.google.com/latitude/apps/badge/api?user=" << badge_m << "&type=kml";
        std::string url = msg.str();
        
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, TRUE);
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        /* setting a callback function to return the data */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback_func);

        /* passing the pointer to the response as the callback parameter */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);

        if (res != 0)
	    logger_m.infoStream() << "msg=" << curl_easy_strerror(res) << endlog;
        curl_easy_cleanup(curl);

        if (res == 0)
        {
            try
            {
                ticpp::Document doc;
                doc.LoadFromString(response); 

                ticpp::Element* pKml = doc.FirstChildElement("kml");
                if (pKml)
                {
                    ticpp::Element* pDocument = pKml->FirstChildElement("Document");
                    if (pDocument)
                    {
                        ticpp::Element* pPlacemark = pDocument->FirstChildElement("Placemark");
                        if (pPlacemark)
                        {
                            ticpp::Element* pPoint = pPlacemark->FirstChildElement("Point");
                            if (pPoint)
                            {
                                ticpp::Element* pCoordinates = pPoint->FirstChildElement("coordinates");
                                if (pCoordinates)
                                {
                                   ticpp::Iterator< ticpp::Node > child;
                                   child = pCoordinates->FirstChild(false);
                                   std::string val = child->Value();
                                   if (child->Type() == TiXmlNode::TEXT && val.length())
                                   {
                                       int offset = val.find(',');
                                       if (offset != std::string::npos)
                                       {
                                           std::string lat = val.substr(offset+1);
                                           std::string lon = val.substr(0, offset);
                                           std::string newLocation = "";
                                           newLocation.append(lat);
                                           newLocation.append(";");
                                           newLocation.append(lon);

                                           if (newLocation != curr_location_m)
                                           {
                                               logger_m.infoStream() << "new location: " << newLocation << endlog;
                                               curr_location_m = newLocation;
                                               //set(createObjectValue(location_m));
                                               //onUpdate();
                                               setValue(curr_location_m);
                                           }
                                       }
                                   }
                                }
                                else
                                {
                                    logger_m.errorStream() << "coordinates node not found" << endlog;
                                }
                            }
                            else
                            {
                                logger_m.errorStream() << "Point node not found" << endlog;
                            }
                        }
                        else
                        {
                            logger_m.errorStream() << "Placemark node not found" << endlog;
                        }
                    }
                    else
                    {
                        logger_m.errorStream() << "Document node not found" << endlog;
                    }
                }
                else
                {
                    logger_m.errorStream() << "KML node not found" << endlog;
                }
            }
            catch( ticpp::Exception& ex )
            {
                logger_m.errorStream() << "Parsing failed: " << ex.m_details << endlog;
            }
        }
    }
    else
        logger_m.errorStream() << "Unable to execute latitude update. Curl not available" << endlog;
# else
    logger_m.errorStream() << "Latitude not supported, libcurn not available" << endlog;
#endif
}

#endif

ObjectController::ObjectController()
{}

ObjectController::~ObjectController()
{
    ObjectIdMap_t::iterator it;
    for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
        delete (*it).second;
}

ObjectController* ObjectController::instance()
{
    if (instance_m == 0)
        instance_m = new ObjectController();
    return instance_m;
}

#ifdef OPEN_HOME_AUTOMATION
void ObjectController::onBusEvent(const uint8_t* buf, int len)
{
    std::string type;
    std::string address;
    
    if (len < 12)
        return;

    type.assign((const char *)buf, 3);
    address.assign((const char *)(buf + 3), 6);
    replace(address.begin(), address.end(), ' ', '0');

    if (len == 12 && (type == "BIR" || type == "DMR"))
    {
        DBIRObject *dbir = NULL;
        ObjectIdMap_t::iterator it;
        for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
        {
            if ((dbir = dynamic_cast<DBIRObject *>((*it).second)) != NULL)
            {
                std::string birAddr = dbir->getAddress();
                if (birAddr.substr(0, birAddr.length() - 2) == address)
                {
                    int output = atoi(birAddr.substr(birAddr.length() - 1).c_str());
                    if (output < 1 || output > dbir->getOutputs())
                        return;

                    int value =
                        strtol(std::string((const char *)(buf + 10), 2).c_str(),
                                           (char **)NULL, 16);

                    int chanValue = (value >> (output-1)) & 0x1;
                    dbir->updateValue(chanValue == 0 ? "off" : "on");
                }
            }
        }
    }
    else if (len == 12 && (type == "BU1" || type == "BU2" ||
                           type == "BU4" || type == "BU6"))
    {
        DPBUObject *dpbu = NULL;
        ObjectIdMap_t::iterator it;
        for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
        {
            if ((dpbu = dynamic_cast<DPBUObject *>((*it).second)) != NULL)
            {
                std::string pbuAddr = dpbu->getAddress();
                if (pbuAddr.substr(0, pbuAddr.length() - 2) == address)
                {
                    int output = atoi(pbuAddr.substr(pbuAddr.length() - 1).c_str());
                    if (output < 1 || output > (dpbu->getButtons() * 2))
                        continue;

                    if (buf[9] == 'I' && output > dpbu->getButtons())
                        continue;
                    else if (buf[9] == 'O' && output < (dpbu->getButtons() + 1))
                        continue;

                    if (buf[9] == 'O')
                        output -= dpbu->getButtons();
                    
                    int value =
                        strtol(std::string((const char *)(buf + 10), 2).c_str(),
                                           (char **)NULL, 16);

                    int chanValue = (value >> (output-1)) & 0x1;
                    dpbu->updateValue(chanValue == 0 ? "off" : "on");
                }
            }
        }
    }
    else if (len == 12 && (type == "DET" || type == "IS4" || type == "IS8"))
    {
        DISMObject *dism = NULL;
        ObjectIdMap_t::iterator it;
        for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
        {
            if ((dism = dynamic_cast<DISMObject *>((*it).second)) != NULL)
            {
                std::string dismAddr = dism->getAddress();
                if (dismAddr.substr(0, dismAddr.length() - 2) == address)
                {
                    int output = atoi(dismAddr.substr(dismAddr.length() - 1).c_str());
                    if (output < 1 || output > dism->getInputs())
                        return;

                    int value =
                        strtol(std::string((const char *)(buf + 10), 2).c_str(),
                                           (char **)NULL, 16);

                    int chanValue = (value >> (output-1)) & 0x1;
                    dism->updateValue(chanValue == 0 ? "off" : "on");
                }
            }
        }
    }
    else if (len == 26 && type == "DIM")
    {
        //DIM  15C9D 0 0 0 0 0 0 064
        DDIMObject *dim = NULL;
        
        std::string valueStr((const char *)(buf + 10));
        replace(valueStr.begin(), valueStr.end(), ' ', '0');        

        ObjectIdMap_t::iterator it;
        for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
        {
            if ((dim = dynamic_cast<DDIMObject *>((*it).second)) != NULL)
            {
                std::string dimAddr = dim->getAddress();
                if (dimAddr.substr(0, dimAddr.length() - 2) == address)
                {
                    int output = atoi(dimAddr.substr(dimAddr.length() - 1).c_str());
                    if (output < 1 || output > 8)
                        return;

                    int value =
                        strtol(valueStr.substr(((output - 1) * 2), 2).c_str(),
                               (char **)NULL, 16);

                    dim->updateValue(value);
                }
            }
        }
    }
    else if (len == 27 && type == "AMP")
    {
        DAMPLIObject *dampli = NULL;
        ObjectIdMap_t::iterator it;
        
        std::string ampliAddress((const char *)(buf + 3), 8);
        replace(ampliAddress.begin(), ampliAddress.end(), ' ', '0');
        ampliAddress.at(6) = '-';

        std::string valueStr((const char *)(buf + 12));

        for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
        {
            if ((dampli = dynamic_cast<DAMPLIObject *>((*it).second)) != NULL)
            {
                std::string dampliAddr = dampli->getAddress();
                if (dampliAddr == ampliAddress)
                {
                    int output = atoi(dampliAddr.substr(dampliAddr.length() - 1).c_str());
                    if (output < 1 || output > 4)
                        return;

                    dampli->updateValue(valueStr);
                }
            }
        }
    }
    else
    {
        printf("Unknown BUS event %s\n", buf);
    }
}
#endif

void ObjectController::onWrite(eibaddr_t src, eibaddr_t dest, const uint8_t* buf, int len)
{
    std::pair<ObjectMap_t::iterator, ObjectMap_t::iterator> range;
    range = objectMap_m.equal_range(dest);
    ObjectMap_t::iterator it;
    for (it = range.first; it != range.second; it++)
        (*it).second->onWrite(buf, len, src);
}

void ObjectController::onRead(eibaddr_t src, eibaddr_t dest, const uint8_t* buf, int len)
{
    std::pair<ObjectMap_t::iterator, ObjectMap_t::iterator> range;
    range = objectMap_m.equal_range(dest);
    ObjectMap_t::iterator it;
    for (it = range.first; it != range.second; it++)
        (*it).second->onRead(buf, len, src);
}

void ObjectController::onResponse(eibaddr_t src, eibaddr_t dest, const uint8_t* buf, int len)
{
    std::pair<ObjectMap_t::iterator, ObjectMap_t::iterator> range;
    range = objectMap_m.equal_range(dest);
    ObjectMap_t::iterator it;
    for (it = range.first; it != range.second; it++)
        (*it).second->onResponse(buf, len, src);
}

Object* ObjectController::getObject(const std::string& id)
{
    ObjectIdMap_t::iterator it = objectIdMap_m.find(id);
    if (it == objectIdMap_m.end())
    {
        std::stringstream msg;
        msg << "ObjectController: Object ID not found: '" << id << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
    it->second->incRefCount();
    return (*it).second;
}

#ifdef OPEN_HOME_AUTOMATION
bool ObjectController::objectExists(const std::string& id)
{
    ObjectIdMap_t::iterator it = objectIdMap_m.find(id);
    if (it == objectIdMap_m.end())
    	return false;
    return true;
}

Object* ObjectController::getObjectForAddress(const std::string& address)
{
    ObjectAddressMap_t::iterator it = objectAddressMap_m.find(address);
    if (it == objectAddressMap_m.end())
    {
        std::stringstream msg;
        msg << "ObjectController: Object not found with address: '" << address << "'" << std::endl;
        throw ticpp::Exception(msg.str());
        return NULL;
    }
    it->second->incRefCount();
    return (*it).second;
}

bool ObjectController::objectExistsForAddress(const std::string& address)
{
    ObjectAddressMap_t::iterator it = objectAddressMap_m.find(address);
    if (it == objectAddressMap_m.end())
    	return false;
    return true;
}

void ObjectController::changeObjectAddress(Object* object,
                                           const std::string& address)
{
    if (object->getAddress() != "")
    {
        ObjectAddressMap_t::iterator addressIt =
            objectAddressMap_m.find(object->getAddress());
        if (addressIt != objectAddressMap_m.end())
            objectAddressMap_m.erase(addressIt);
    }

    object->setAddress(address.c_str());

    if (object->getAddress() != "")
    {
        objectAddressMap_m.insert(
            ObjectAddressPair_t(object->getAddress(), object));
    }
}
#endif

void ObjectController::addObject(Object* object)
{
    if (!objectIdMap_m.insert(ObjectIdPair_t(object->getID(), object)).second)
        throw ticpp::Exception("Object ID already exists");
    if (object->getGad())
        objectMap_m.insert(ObjectPair_t(object->getGad(), object));
#ifdef OPEN_HOME_AUTOMATION
    if (object->getAddress() != "")
        objectAddressMap_m.insert(ObjectAddressPair_t(object->getAddress(), object));
#endif
    std::list<eibaddr_t>::iterator it2, it_end;
    it_end = object->getListenerGadEnd();
    for (it2=object->getListenerGad(); it2!=it_end; it2++)
        objectMap_m.insert(ObjectPair_t((*it2), object));
}

void ObjectController::removeObjectFromAddressMap(eibaddr_t gad, Object* object)
{
    if (gad == 0)
        return;
    std::pair<ObjectMap_t::iterator, ObjectMap_t::iterator> range =
        objectMap_m.equal_range(gad);
    ObjectMap_t::iterator it = range.first;
    while(it != range.second) {
        if ((*it).second == object)
            objectMap_m.erase(it++);
        else
            ++it;
    }
}

void ObjectController::removeObject(Object* object)
{
    ObjectIdMap_t::iterator it = objectIdMap_m.find(object->getID());
    if (it != objectIdMap_m.end())
    {
        eibaddr_t gad = it->second->getGad();
        removeObjectFromAddressMap(gad, object);

        std::list<eibaddr_t>::iterator it2, it_end;
        it_end = object->getListenerGadEnd();
        for (it2=object->getListenerGad(); it2!=it_end; it2++)
            removeObjectFromAddressMap((*it2), object);

#ifdef OPEN_HOME_AUTOMATION
        if (object->getAddress() != "")
        {
             ObjectAddressMap_t::iterator addressIt =
                 objectAddressMap_m.find(object->getAddress());
             if (addressIt != objectAddressMap_m.end())
                 objectAddressMap_m.erase(addressIt);
        }
#endif

        if (it->second->inUse())
            throw ticpp::Exception("Delete failed! Object still in use.");
        delete it->second;

        objectIdMap_m.erase(it);
    }
}

void ObjectController::importXml(ticpp::Element* pConfig)
{
    ticpp::Iterator< ticpp::Element > child("object");
    for ( child = pConfig->FirstChildElement("object", false); child != child.end(); child++ )
    {
        std::string id = child->GetAttribute("id");
        bool del = child->GetAttribute("delete") == "true";
        ObjectIdMap_t::iterator it = objectIdMap_m.find(id);
        if (it != objectIdMap_m.end())
        {
            Object* object = it->second;

            std::list<eibaddr_t>::iterator it2, it_end;
            it_end = object->getListenerGadEnd();
            for (it2=object->getListenerGad(); it2!=it_end; it2++)
                removeObjectFromAddressMap((*it2), object);
            removeObjectFromAddressMap(object->getGad(), object);

            if (del)
            {
                if (object->inUse())
                    throw ticpp::Exception("Delete failed! Object still in use.");

#ifdef OPEN_HOME_AUTOMATION
                if (object->getAddress() != "")
                {
                     ObjectAddressMap_t::iterator addressIt =
                         objectAddressMap_m.find(object->getAddress());
                     if (addressIt != objectAddressMap_m.end())
                         objectAddressMap_m.erase(addressIt);
                }
#endif

                delete object;
                objectIdMap_m.erase(it);
            }
            else
            {
                object->importXml(&(*child));
                if (object->getGad())
                    objectMap_m.insert(ObjectPair_t(object->getGad(), object));
                std::list<eibaddr_t>::iterator it2, it_end;
                it_end = object->getListenerGadEnd();
                for (it2=object->getListenerGad(); it2!=it_end; it2++)
                    objectMap_m.insert(ObjectPair_t((*it2), object));
                objectIdMap_m.insert(ObjectIdPair_t(id, object));
#ifdef OPEN_HOME_AUTOMATION
                if (object->getAddress() != "")
                    objectAddressMap_m.insert(ObjectAddressPair_t(object->getAddress(), object));
#endif
            }
        }
        else
        {
            if (del)
                throw ticpp::Exception("Object not found");
            Object* object = Object::create(&(*child));
            if (object->getGad())
                objectMap_m.insert(ObjectPair_t(object->getGad(), object));
            std::list<eibaddr_t>::iterator it2, it_end;
            it_end = object->getListenerGadEnd();
            for (it2=object->getListenerGad(); it2!=it_end; it2++)
                objectMap_m.insert(ObjectPair_t((*it2), object));
            objectIdMap_m.insert(ObjectIdPair_t(id, object));
#ifdef OPEN_HOME_AUTOMATION
                if (object->getAddress() != "")
                    objectAddressMap_m.insert(ObjectAddressPair_t(object->getAddress(), object));
#endif
        }
    }

}

void ObjectController::exportXml(ticpp::Element* pConfig)
{
    ObjectIdMap_t::iterator it;
    for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
    {
        ticpp::Element pElem("object");
        (*it).second->exportXml(&pElem);
        pConfig->LinkEndChild(&pElem);
    }
}

void ObjectController::exportObjectValues(ticpp::Element* pObjects)
{
    ObjectIdMap_t::iterator it;
    for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
    {
        ticpp::Element pElem("object");
        pElem.SetAttribute("id", (*it).second->getID());
        pElem.SetAttribute("value", (*it).second->getValue());
        pObjects->LinkEndChild(&pElem);
    }
}

// Delivers all objects
std::list<Object*> ObjectController::getObjects()
{
    std::list<Object*> objects;
    ObjectIdMap_t::iterator it;
    for (it = objectIdMap_m.begin(); it != objectIdMap_m.end(); it++)
    {
      it->second->incRefCount();
      objects.push_back((*it).second);
    }
    return objects;
}

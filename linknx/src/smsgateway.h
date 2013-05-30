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

#ifndef SMSGATEWAY_H
#define SMSGATEWAY_H

#include <string>
#include "config.h"
#include "logger.h"
#include "ticpp.h"

#ifdef OPEN_HOME_AUTOMATION
#include "ioport.h"

class SmsPollThread;

typedef struct
{
    std::string id;
    std::string from;
    std::string message;
    bool complete;
} SmsMessage;

class SmsListener
{
public:
    virtual ~SmsListener() {};
    virtual void onSmsReceived(SmsMessage *message) = 0;
};

class SmsGateway : IOPortListener
#else
class SmsGateway
#endif
{
public:
    SmsGateway();
    ~SmsGateway();

    void importXml(ticpp::Element* pConfig);
    void exportXml(ticpp::Element* pConfig);

    void sendSms(std::string &id, std::string &value);

#ifdef OPEN_HOME_AUTOMATION
    void onDataReceived(const uint8_t* buf, unsigned int len);

    void addListener(SmsListener *listener);
    bool removeListener(SmsListener *listener);

    void sendAT(std::string command);
    
#endif

private:
    enum SmsGatewayType
    {
        Clickatell,
#ifdef OPEN_HOME_AUTOMATION
        IoPort,
#endif
        Unknown
    };

    SmsGatewayType type_m;

    std::string user_m;
    std::string pass_m;
    std::string data_m;
    std::string from_m;
#ifdef OPEN_HOME_AUTOMATION
    std::string ioport_m;

    typedef std::list<SmsListener*> SmsListenerList_t;
    SmsListenerList_t listenerList_m;

    std::auto_ptr<SmsPollThread> pollThread_m;    
#endif

    static Logger& logger_m;
};

#ifdef OPEN_HOME_AUTOMATION
class SmsPollThread : public Thread
{
public:
    SmsPollThread();
    virtual ~SmsPollThread();

private:
    void Run (pth_sem_t * stop);
    static Logger& logger_m;
};



class RxSmsCondition : public Condition, public SmsListener
{
public:
    RxSmsCondition(ChangeListener* cl);
    virtual ~RxSmsCondition();

    virtual bool evaluate();
    virtual void importXml(ticpp::Element* pConfig);
    virtual void exportXml(ticpp::Element* pConfig);
    virtual void statusXml(ticpp::Element* pStatus);

    virtual void onSmsReceived(SmsMessage *message);

private:
    std::string from_m;
    std::string exp_m;
    bool value_m;
    ChangeListener* cl_m;
};
#endif

#endif

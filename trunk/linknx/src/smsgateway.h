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
#include "messagecontroller.h"
typedef struct
{
    time_t      received;
    std::string from;
    std::string message;
} SmsMessage;

class SmsListener
{
public:
    virtual ~SmsListener() {};
    virtual void onSmsReceived(SmsMessage *message) = 0;
};
#endif

class SmsGateway
{
public:
    SmsGateway();
    ~SmsGateway();

    void importXml(ticpp::Element* pConfig);
    void exportXml(ticpp::Element* pConfig);

    void sendSms(std::string &id, std::string &value);

#ifdef OPEN_HOME_AUTOMATION
    void receiveSms(std::string &from, std::string &text);

    void addListener(SmsListener *listener);
    bool removeListener(SmsListener *listener);

    bool isConfigured() { return configured_m; }; 
#endif

private:
    enum SmsGatewayType
    {
        Clickatell,
#ifdef OPEN_HOME_AUTOMATION
        Arduino,
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
    std::string module_m;
    bool        configured_m;

    typedef std::list<SmsListener*> SmsListenerList_t;
    SmsListenerList_t listenerList_m;
#endif

    static Logger& logger_m;
};

#ifdef OPEN_HOME_AUTOMATION
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

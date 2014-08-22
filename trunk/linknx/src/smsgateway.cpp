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

#include "smsgateway.h"
#include <iostream>
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#ifdef OPEN_HOME_AUTOMATION
#include "services.h"
#include "ioport.h"
#include <vector>
#include <string>

typedef enum
{
    eGsmStateIdle,
    eGsmStateRxSms,
} eGsmState;

eGsmState state = eGsmStateIdle;
#endif

Logger& SmsGateway::logger_m(Logger::getInstance("SmsGateway"));

#ifdef OPEN_HOME_AUTOMATION
SmsGateway::SmsGateway() : type_m(Unknown), configured_m(false)
#else
SmsGateway::SmsGateway() : type_m(Unknown)
#endif
{}

SmsGateway::~SmsGateway()
{}

void SmsGateway::importXml(ticpp::Element* pConfig)
{
    std::string type = pConfig->GetAttribute("type");
    if (type == "clickatell")
    {
#ifdef HAVE_LIBCURL
        type_m = Clickatell;
        pConfig->GetAttribute("user", &user_m);
        pConfig->GetAttribute("pass", &pass_m);
        pConfig->GetAttribute("api_id", &data_m);
        from_m = pConfig->GetAttribute("from");
# else
        std::stringstream msg;
        msg << "SmsGateway: Gateway type 'clickatell' not supported, libcurl not available" << std::endl;
        throw ticpp::Exception(msg.str());
#endif
#ifdef OPEN_HOME_AUTOMATION
        configured_m = true;
#endif
    }
#ifdef OPEN_HOME_AUTOMATION
    else if (type == "arduino")
    {
        ArduinoUdpIOPort *arduinoIoPort;

        type_m = Arduino;
        ioport_m = pConfig->GetAttribute("ioport");
        module_m = pConfig->GetAttribute("module");
        if (!IOPortManager::instance()->getPort(ioport_m))
        {
            std::stringstream msg;
            msg << "SmsGateway: IO Port ID not found: '" << ioport_m << "'" << std::endl;
            throw ticpp::Exception(msg.str());
	}
        if ((arduinoIoPort =
               dynamic_cast<ArduinoUdpIOPort *>
               (IOPortManager::instance()->getPort(ioport_m))) == NULL)
        {
            std::stringstream msg;
            msg << "SmsGateway: IO Port is not an arduino port: '" << 
                   ioport_m << "'" << std::endl;
            throw ticpp::Exception(msg.str());
	}
        configured_m = true;
    }
#endif
    else if (type == "")
    {
        type_m = Unknown;
        user_m.clear();
        pass_m.clear();
        data_m.clear();
#ifdef OPEN_HOME_AUTOMATION
        ioport_m.clear();
        module_m.clear();
#endif
    }
    else
    {
        std::stringstream msg;
        msg << "SmsGateway: Bad gateway type: '" << type << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

void SmsGateway::exportXml(ticpp::Element* pConfig)
{
    if (type_m == Clickatell)
    {
        pConfig->SetAttribute("type", "clickatell");
        pConfig->SetAttribute("user", user_m);
        pConfig->SetAttribute("pass", pass_m);
        pConfig->SetAttribute("api_id", data_m);
        if (!from_m.empty())
            pConfig->SetAttribute("from", from_m);
    }
#ifdef OPEN_HOME_AUTOMATION
    else if (type_m == Arduino)
    {
        pConfig->SetAttribute("type", "arduino");
        pConfig->SetAttribute("ioport", ioport_m);
        pConfig->SetAttribute("module", module_m);
     }
#endif
}

void SmsGateway::sendSms(std::string &id, std::string &value)
{
    if (type_m == Clickatell)
    {
#ifdef HAVE_LIBCURL
        CURL *curl;
        CURLcode res;

        curl = curl_easy_init();
        if(curl)
        {
            char *escaped_value = curl_escape(value.c_str(), value.length());
            std::stringstream msg;
            msg << "http://api.clickatell.com/http/sendmsg?user=" << user_m
            << "&password=" << pass_m
            << "&api_id=" << data_m;
            if (!from_m.empty())
                msg << "&from=" << from_m;
            msg << "&to=" << id << "&text=" << escaped_value;
            std::string url = msg.str();
            curl_free(escaped_value);
            //        curl_easy_setopt(curl, CURLOPT_VERBOSE, TRUE);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            res = curl_easy_perform(curl);

            logger_m.infoStream() << "curl_easy_perform returned: " << res << endlog;
            if (res != 0)
                logger_m.infoStream() << "msg=" << curl_easy_strerror(res) << endlog;

            curl_easy_cleanup(curl);
        }
        else
            logger_m.errorStream() << "Unable to execute SendSmsAction. Curl not available" << endlog;
#endif
    }
#ifdef OPEN_HOME_AUTOMATION
    else if (type_m == Arduino)
    {
        IOPort* port = IOPortManager::instance()->getPort(ioport_m);
        if (!port)
            return;
        
        std::string command = "TX_SMS ";
        //command.append(module_m);
        //command.append(" ");
        command.append(id);
        command.append(" ");
        command.append(value);

        port->send((const uint8_t *)command.c_str(), command.length());
    }
#endif
    else
        logger_m.errorStream() << "Unable to send SMS, gateway not set." << endlog;
}

#ifdef OPEN_HOME_AUTOMATION
void SmsGateway::receiveSms(std::string &from, std::string &text)
{
    SmsMessage *message = new SmsMessage();
    message->received = time(0);
    message->from = from;
    message->message = text;
    logger_m.debugStream() << "SmsGateway: RX message from \"" << message->from << "\" with text \"" << message->message << "\"" << endlog;

    Services::instance()->getMessageController()->storeMessage(message->from, message->message);

    SmsListenerList_t::iterator it;
    for (it = listenerList_m.begin(); it != listenerList_m.end(); it++)
    {
        (*it)->onSmsReceived(message);
    }
}

void SmsGateway::addListener(SmsListener *listener)
{
    listenerList_m.push_back(listener);
}

bool SmsGateway::removeListener(SmsListener *listener)
{
    listenerList_m.remove(listener);
    return true;
}

void StringExplode(std::string str, std::string separator, std::vector<std::string>* results){
    int found;
    found = str.find_first_of(separator);
    while(found != std::string::npos){
        if(found > 0){
            results->push_back(str.substr(0,found));
        }
        str = str.substr(found+1);
        found = str.find_first_of(separator);
    }
    if(str.length() > 0){
        results->push_back(str);
    }
}

RxSmsCondition::RxSmsCondition(ChangeListener* cl) : value_m(false), cl_m(cl)
{}

RxSmsCondition::~RxSmsCondition()
{
    Services::instance()->getSmsGateway()->removeListener(this);
}

bool RxSmsCondition::evaluate()
{
    return value_m;
}

void RxSmsCondition::importXml(ticpp::Element* pConfig)
{
    if (!cl_m)
        throw ticpp::Exception("Rx SMS condition is not supported in this context");
    from_m = pConfig->GetAttribute("from");
    exp_m = pConfig->GetAttribute("expected");
    
    Services::instance()->getSmsGateway()->addListener(this);
    logger_m.infoStream() << "RxSmsCondition: configured to watch for '" << exp_m << "' from " << from_m << endlog;
}

void RxSmsCondition::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("type", "sms-rx");
    pConfig->SetAttribute("from", from_m);
    pConfig->SetAttribute("expected", exp_m);
}

void RxSmsCondition::statusXml(ticpp::Element* pStatus)
{
    pStatus->SetAttribute("type", "sms-rx");
}

void RxSmsCondition::onSmsReceived(SmsMessage *message)
{
    if (!cl_m)
        return;

    if (exp_m == message->message)
    {
        std::vector<std::string> v;
        int i;

        StringExplode(from_m, ",", &v);

        for (i=0; i<v.size(); i++)
        {
            if (v[i] == message->from)
                break;
        }

        if (i >= v.size())
            return;

        logger_m.debugStream() << "RxSmsCondition: expected message received: '" << exp_m << "'" << endlog;
        value_m = true;
        cl_m->onChange(0);
        value_m = false;
        cl_m->onChange(0);
    }
}
#endif


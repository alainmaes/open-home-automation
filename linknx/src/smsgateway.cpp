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

SmsGateway::SmsGateway() : type_m(Unknown)
{}

SmsGateway::~SmsGateway()
{}

void SmsGateway::importXml(ticpp::Element* pConfig)
{
#ifdef OPEN_HOME_AUTOMATION
    if (ioport_m != "" &&
        IOPortManager::instance()->getPort(ioport_m))
    {
        IOPortManager::instance()->getPort(ioport_m)->removeListener(this);
    }
#endif

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
    }
#ifdef OPEN_HOME_AUTOMATION
    else if (type == "ioport")
    {
        SerialIOPort *serialPort;

        type_m = IoPort;
        ioport_m = pConfig->GetAttribute("ioport");
        if (!IOPortManager::instance()->getPort(ioport_m))
        {
            std::stringstream msg;
            msg << "SmsGateway: IO Port ID not found: '" << ioport_m << "'" << std::endl;
            throw ticpp::Exception(msg.str());
	}
        if ((serialPort =
               dynamic_cast<SerialIOPort *>
               (IOPortManager::instance()->getPort(ioport_m))) == NULL)
        {
            std::stringstream msg;
            msg << "SmsGateway: IO Port is not a serial port: '" << 
                   ioport_m << "'" << std::endl;
            throw ticpp::Exception(msg.str());
	}
        
        IOPortManager::instance()->getPort(ioport_m)->addListener(this);
        pollThread_m.reset(new SmsPollThread());
    }
#endif
    else if (type == "")
    {
        type_m = Unknown;
        user_m.clear();
        pass_m.clear();
        data_m.clear();
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
    else if (type_m == IoPort)
    {
        pConfig->SetAttribute("type", "ioport");
        pConfig->SetAttribute("ioport", ioport_m);
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
    else if (type_m == IoPort)
    {
        //IOPort::addListener
        //send
        //
    }
#endif
    else
        logger_m.errorStream() << "Unable to send SMS, gateway not set." << endlog;
}

#ifdef OPEN_HOME_AUTOMATION

void SmsGateway::addListener(SmsListener *listener)
{
    if (listenerList_m.empty())
    {
        if (pollThread_m.get())
            pollThread_m->Start();
    }

    listenerList_m.push_back(listener);
}

bool SmsGateway::removeListener(SmsListener *listener)
{
    listenerList_m.remove(listener);
    if (listenerList_m.empty())
    {
        if (pollThread_m.get())
            pollThread_m->Stop();
    }
    return true;
}

void SmsGateway::sendAT(std::string command)
{
    while (state != eGsmStateIdle)
        pth_usleep(10);

    SerialIOPort *serialPort;
    if (IOPortManager::instance()->getPort(ioport_m) &&
        (serialPort = dynamic_cast<SerialIOPort *>
                          (IOPortManager::instance()->getPort(ioport_m))) != NULL)
    {
        if (command.substr(0, 7) == "AT+CMGL")
            state = eGsmStateRxSms;
        serialPort->send((const uint8_t*)command.c_str(), strlen(command.c_str()));
    }
}

Logger& SmsPollThread::logger_m(Logger::getInstance("SmsPollThread"));

SmsPollThread::SmsPollThread()
{}

SmsPollThread::~SmsPollThread()
{
    Stop();
}

void SmsPollThread::Run (pth_sem_t * stop1)
{
    pth_event_t stop = pth_event (PTH_EVENT_SEM, stop1);
    logger_m.debugStream() << "Starting SMS POLL loop." << endlog;
    
    Services::instance()->getSmsGateway()->sendAT("AT+CMGF=1\r");
    //pth_sleep(1);
    //Services::instance()->getSmsGateway()->sendAT("AT+CMGS=\"1912\"\r");
    //pth_sleep(2);
    //Services::instance()->getSmsGateway()->sendAT("CONSULT\x1A\r");
    //pth_sleep(10);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    while (pth_event_status (stop) != PTH_STATUS_OCCURRED)
    {
        Services::instance()->getSmsGateway()->sendAT("AT+CMGL=\"all\"\r");
        tv.tv_sec = 1;
        pth_select_ev(0,0,0,0,&tv,stop);
    }
    logger_m.debugStream() << "Out of SMS POLL loop." << endlog;
    pth_event_free (stop, PTH_FREE_THIS);
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

void SmsGateway::onDataReceived(const uint8_t* buf, unsigned int len)
{
    static SmsMessage *message = NULL;

    if (len < 2)
        return;

    std::string msg(reinterpret_cast<const char*>(buf), len);
    if (state == eGsmStateRxSms)
    {
        if (msg.substr(0, 2) == "OK")
        {
            state = eGsmStateIdle;

            if (message)
            {
                char buf[100];
                snprintf(buf, sizeof(buf), "AT+CMGD=%s\r", message->id.c_str());
                sendAT(std::string(buf));

                delete(message);
                message = NULL;
            }
        }
        else if (msg.substr(0, 6) == "+CMGL:" && message == NULL)
        {
            //+CMGL: 9,"REC READ","+32000000000",,"12/12/19,15:36:00+04"
            std::vector<std::string> v;
            StringExplode(msg.substr(7), ",", &v);

            message = new SmsMessage();
            message->id = v[0];
            message->from = v[2].substr(1, v[2].length() - 2);
            message->message = "";
            message->complete = false;
        }
        else
        {
            if (message && !message->complete)
            {
                message->complete = true;
                message->message = msg.substr(0, msg.length() - 1);
                logger_m.debugStream() << "SmsGateway: RX message from \"" << message->from << "\" with text \"" << message->message << "\"" << endlog;
                
                SmsListenerList_t::iterator it;
                for (it = listenerList_m.begin(); it != listenerList_m.end(); it++)
                {
                    (*it)->onSmsReceived(message);
                }
            }
            else
            {
                //logger_m.errorStream() << "SmsGateway: RX " << msg << endlog;
            }
        }
    }
    else
    {
       logger_m.errorStream() << "SmsGateway: Received " << msg << endlog;
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


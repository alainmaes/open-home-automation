/*
    LinKNX KNX home automation platform
    Copyright (C) 2007-2009 Jean-François Meessen <linknx@ouaye.net>
 
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

#include <iostream>
#include <iomanip>
#include "ioport.h"
#include <fcntl.h>
#ifdef OPEN_HOME_AUTOMATION
#include <algorithm>
#include "ruleserver.h"
#include "services.h"
#include "usercontroller.h"
#endif

Logger& IOPort::logger_m(Logger::getInstance("IOPort"));
Logger& RxThread::logger_m(Logger::getInstance("RxThread"));
Logger& UdpIOPort::logger_m(Logger::getInstance("UdpIOPort"));
#ifdef OPEN_HOME_AUTOMATION
Logger& ArduinoUdpIOPort::logger_m(Logger::getInstance("ArduinoUdpIOPort"));
Logger& DetectThread::logger_m(Logger::getInstance("DetectThread"));
Logger& AIOKeepAliveThread::logger_m(Logger::getInstance("AIOKeepAliveThread"));
#endif
Logger& TcpClientIOPort::logger_m(Logger::getInstance("TcpClientIOPort"));
Logger& SerialIOPort::logger_m(Logger::getInstance("SerialIOPort"));

IOPortManager* IOPortManager::instance_m;

IOPortManager::IOPortManager()
{}

IOPortManager::~IOPortManager()
{
    IOPortMap_t::iterator it;
    for (it = portMap_m.begin(); it != portMap_m.end(); it++) {
        delete (*it).second;
    }
}

IOPortManager* IOPortManager::instance()
{
    if (instance_m == 0)
        instance_m = new IOPortManager();
    return instance_m;
}

IOPort* IOPortManager::getPort(const std::string& id)
{
    IOPortMap_t::iterator it = portMap_m.find(id);
    if (it == portMap_m.end())
        return 0;
    return (*it).second;
}

void IOPortManager::addPort(IOPort* port)
{
    if (!portMap_m.insert(IOPortPair_t(port->getID(), port)).second)
        throw ticpp::Exception("IO Port ID already exists");
}

void IOPortManager::removePort(IOPort* port)
{
    IOPortMap_t::iterator it = portMap_m.find(port->getID());
    if (it != portMap_m.end())
    {
        delete it->second;
        portMap_m.erase(it);
    }
}

void IOPortManager::importXml(ticpp::Element* pConfig)
{
    ticpp::Iterator< ticpp::Element > child("ioport");
    for ( child = pConfig->FirstChildElement("ioport", false); child != child.end(); child++ )
    {
        std::string id = child->GetAttribute("id");
        bool del = child->GetAttribute("delete") == "true";
        IOPortMap_t::iterator it = portMap_m.find(id);
        if (it != portMap_m.end())
        {
            IOPort* port = it->second;

            if (del)
            {
                delete port;
                portMap_m.erase(it);
            }
            else
            {
                port->importXml(&(*child));
                portMap_m.insert(IOPortPair_t(id, port));
            }
        }
        else
        {
            if (del)
                throw ticpp::Exception("IO Port not found");
            IOPort* port = IOPort::create(&(*child));
            portMap_m.insert(IOPortPair_t(id, port));
        }
    }

}

void IOPortManager::exportXml(ticpp::Element* pConfig)
{
    IOPortMap_t::iterator it;
    for (it = portMap_m.begin(); it != portMap_m.end(); it++)
    {
        ticpp::Element pElem("ioport");
        (*it).second->exportXml(&pElem);
        pConfig->LinkEndChild(&pElem);
    }
}

#ifdef OPEN_HOME_AUTOMATION
void saveConfig()
{
    std::string filename = Services::instance()->getConfigFile();
    if (filename != "")
    {
        try
        {
            // Save a document
            ticpp::Document doc;
            ticpp::Declaration decl("1.0", "", "");
            doc.LinkEndChild(&decl);
            ticpp::Element pConfig("config");

            ticpp::Element pUsers("users");
            UserController::instance()->exportXml(&pUsers);
            pConfig.LinkEndChild(&pUsers);
            ticpp::Element pServices("services");
            Services::instance()->exportXml(&pServices);
            pConfig.LinkEndChild(&pServices);
            ticpp::Element pObjects("objects");
            ObjectController::instance()->exportXml(&pObjects);
            pConfig.LinkEndChild(&pObjects);
            ticpp::Element pRules("rules");
            RuleServer::instance()->exportXml(&pRules);
            pConfig.LinkEndChild(&pRules);
            ticpp::Element pLogging("logging");
            Logging::instance()->exportXml(&pLogging);
            pConfig.LinkEndChild(&pLogging);

            doc.LinkEndChild(&pConfig);
            doc.SaveFile(filename);
        }
        catch( ticpp::Exception& ex )
        {
            // If any function has an error, execution will enter here.
            // Report the error
            throw "Error writing config to file";
        }
    }
}

DetectThread::DetectThread() : isRunning_m(false), stop_m(0)
{}

DetectThread::~DetectThread()
{
    Stop();
}

bool DetectThread::sleep(int delay, pth_sem_t * stop)
{
    struct timeval timeout;
    timeout.tv_sec = delay / 1000;
    timeout.tv_usec = (delay % 1000) * 1000;
    pth_event_t stop_ev = pth_event (PTH_EVENT_SEM, stop);
    pth_select_ev(0, NULL, NULL, NULL, &timeout, stop_ev);
    return (pth_event_status (stop_ev) == PTH_STATUS_OCCURRED);
}

void DetectThread::Run (pth_sem_t * stop1)
{
    stop_m = pth_event (PTH_EVENT_SEM, stop1);
    uint8_t buf[1024];
    int retval;
    int sockfd_m;
    struct sockaddr_in addr_m;

    memset (&addr_m, 0, sizeof (addr_m));
    addr_m.sin_family = AF_INET;
    addr_m.sin_port = htons(1900);
    addr_m.sin_addr.s_addr = inet_addr("239.255.255.250");

    sockfd_m = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_m < 0)
    {
        logger_m.errorStream() << "Unable to create socket" << endlog;
        return;
    }

    logger_m.debugStream() << "Start ssdp loop." << endlog;

    while (true)
    {
        char data[] = "M-SEARCH * HTTP/1.1\r\n"
                      "Host: 239.255.255.250:1900\r\n"
                      "Man: \"ssdp:discover\"\r\n"
                      "ST:upnp:rootdevice\r\n"
                      "MX:3\r\n"
                      "\r\n";

        ssize_t nbytes = pth_sendto(sockfd_m, data, strlen(data), 0,
                                (const struct sockaddr *) &addr_m,
                                sizeof (addr_m));
        if (nbytes != strlen(data))
        {
            logger_m.errorStream() << "Unable to send to socket" << endlog;
        }
        else
        {
            while (true)
            {
                socklen_t rl;
                sockaddr_in r;
                rl = sizeof (r);
                memset (&r, 0, sizeof (r));
                pth_event_t timeoutEvent = pth_event(PTH_EVENT_TIME, pth_timeout(1,0));
                ssize_t i = pth_recvfrom_ev(sockfd_m, buf, sizeof(buf), 0,
                                            (struct sockaddr *) &r, &rl, timeoutEvent);
                //logger_m.debugStream() << "Out of recvfrom " << i << " rl=" << rl << endlog;
                if (i > 0 && rl == sizeof (r))
                {
                    std::string msg(reinterpret_cast<const char*>(buf), i);
                    std::size_t found;
                    std::string server;
                    std::string usn;
                    std::string location;

                    while ((found = msg.find("\r\n")) !=std::string::npos)
                    {
                        //logger_m.debugStream() << "Received '" <<
                        //                            msg.substr(0, found) << 
                        //                            "'" << endlog;

                        if (msg.length() > 8 &&
                            msg.substr(0, 8) == "SERVER: ")
                        {
                            server = msg.substr(8, found - 8);
                        }
                        if (msg.length() > 5 &&
                            msg.substr(0, 5) == "USN: ")
                        {
                            usn = msg.substr(5, found - 5);
                        }
                        if (msg.length() > 10 &&
                            msg.substr(0, 10) == "LOCATION: ")
                        {
                            location = msg.substr(10, found - 10);
                        }
                        
                        if (msg.length() > found +2)
                          msg = msg.substr(found + 2);
                        else
                          break;                     
                    }

                    if (server.find("Arduino") !=std::string::npos &&
                        server.find("AIO") !=std::string::npos &&
                        (usn.length() > 5 && usn.substr(0, 5) == "uuid:") &&
                        (location.length() > 7 && location.substr(0, 7) == "http://") &&
                         location.substr(7).find("/") !=std::string::npos)
                    {
                        //logger_m.debugStream() << "=> SERVER = " << server << endlog;
                        //logger_m.debugStream() << "=> USN = " << usn << endlog;
                        //logger_m.debugStream() << "=> LOCATION = " << location << endlog;

                        std::string id = usn.substr(5);
                        std::string host;

                        host = location.substr(7);
                        host = host.substr(0, host.find("/"));

                        //logger_m.debugStream() << "=> ID = " << id << endlog;
                        //logger_m.debugStream() << "=> HOST = " << host << endlog;

                        IOPort* pPort = NULL;

                        if ((pPort = IOPortManager::instance()->getPort(id)) == NULL)
                        {
                            ticpp::Element config("ioport");

                            config.SetAttribute("id", id);
                            config.SetAttribute("type", "arduino");
                            config.SetAttribute("host", host);
                            config.SetAttribute("port", "1234");

                            IOPort* pPort = IOPort::create("arduino");
                            pPort->importXml(&config);

                            IOPortManager::instance()->addPort(pPort);
                            saveConfig();
                        }
                        else
                        {
                            //TODO check host/port
                        }

                        if (server.find("AIOSMS") !=std::string::npos &&
                            !Services::instance()->getSmsGateway()->isConfigured())
                        {
                            ticpp::Element config("smsgateway");
                            config.SetAttribute("type", "arduino");
                            config.SetAttribute("ioport", id);
                            //config.SetAttribute("module", module);
                            Services::instance()->getSmsGateway()->importXml(&config);
                            saveConfig();
                        }
                    }
                }
                else
                {
                    break;
                }
            }
        }

        if (sleep(60000, stop1))
            break;        
    }

    logger_m.debugStream() << "Out of ssdp loop." << endlog;
    pth_event_free (stop_m, PTH_FREE_THIS);
    stop_m = 0;
}

void IOPortManager::startAutoDetect()
{
    detectThread_m.reset(new DetectThread());
    detectThread_m->startDetection();
}
#endif

IOPort::IOPort()
{}

IOPort::~IOPort()
{
}

IOPort* IOPort::create(const std::string& type)
{
    if (type == "" || type == "udp")
        return new UdpIOPort();
#ifdef OPEN_HOME_AUTOMATION
    else if (type == "arduino")
        return new ArduinoUdpIOPort();
#endif
    else if (type == "tcp")
        return new TcpClientIOPort();
    else if (type == "serial")
        return new SerialIOPort();
    else
        return 0;
}

IOPort* IOPort::create(ticpp::Element* pConfig)
{
    std::string type = pConfig->GetAttribute("type");
    IOPort* obj = IOPort::create(type);
    if (obj == 0)
    {
        std::stringstream msg;
        msg << "IOPort type not supported: '" << type << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
    obj->importXml(pConfig);
    return obj;
}

void IOPort::importXml(ticpp::Element* pConfig)
{
    id_m = pConfig->GetAttribute("id");
    if (isRxEnabled())
        rxThread_m.reset(new RxThread(this));
}

void IOPort::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("id", id_m);
}

void IOPort::addListener(IOPortListener *l)
{
    if (rxThread_m.get())
        rxThread_m->addListener(l);
}

bool IOPort::removeListener(IOPortListener *l)
{
    if (rxThread_m.get())
        return (rxThread_m->removeListener(l));
    else
        return false;
}

RxThread::RxThread(IOPort *port) : port_m(port), isRunning_m(false), stop_m(0)
{}

RxThread::~RxThread()
{
    Stop();
}

void RxThread::addListener(IOPortListener *listener)
{
    if (listenerList_m.empty())
        Start();
    listenerList_m.push_back(listener);
}

bool RxThread::removeListener(IOPortListener *listener)
{
    listenerList_m.remove(listener);
    if (listenerList_m.empty())
        Stop();
    return true;
}

void RxThread::Run (pth_sem_t * stop1)
{
    stop_m = pth_event (PTH_EVENT_SEM, stop1);
    uint8_t buf[1024];
    int retval;
    logger_m.debugStream() << "Start IO Port loop." << endlog;
    memset(buf, 0, sizeof(buf));
    while ((retval = port_m->get(buf, sizeof(buf), stop_m)) > 0)
    {
        ListenerList_t::iterator it;
        for (it = listenerList_m.begin(); it != listenerList_m.end(); it++)
        {
//            logger_m.debugStream() << "Calling onDataReceived on listener for " << port_m->getID() << endlog;
            (*it)->onDataReceived(buf, retval);
        }
        memset(buf, 0, sizeof(buf));
    }
    logger_m.debugStream() << "Out of IO Port loop." << endlog;
    pth_event_free (stop_m, PTH_FREE_THIS);
    stop_m = 0;
}

UdpIOPort::UdpIOPort() : sockfd_m(-1), port_m(0), rxport_m(0)
{
    memset (&addr_m, 0, sizeof (addr_m));
}

UdpIOPort::~UdpIOPort()
{
    if (sockfd_m >= 0)
        close(sockfd_m);
    Logger::getInstance("UdpIOPort").debugStream() << "Deleting UdpIOPort " << endlog;
}

void UdpIOPort::importXml(ticpp::Element* pConfig)
{
    memset (&addr_m, 0, sizeof (addr_m));
    addr_m.sin_family = AF_INET;
    pConfig->GetAttribute("port", &port_m);
    addr_m.sin_port = htons(port_m);
    host_m = pConfig->GetAttribute("host");
    addr_m.sin_addr.s_addr = inet_addr(host_m.c_str());
    pConfig->GetAttributeOrDefault("rxport", &rxport_m, 0);
    IOPort::importXml(pConfig);

    sockfd_m = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_m >= 0 && rxport_m > 0) {
        struct sockaddr_in addr;
        bzero(&addr,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(rxport_m);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sockfd_m, (struct sockaddr *)&addr,sizeof(addr)) < 0) /* error */
        {
            logger_m.errorStream() << "Unable to bind socket for ioport " << getID() << endlog;
        }
    }
    else {
        logger_m.errorStream() << "Unable to create  socket for ioport " << getID() << endlog;
    }    
    
   
    logger_m.infoStream() << "UdpIOPort configured for host " << host_m << " and port " << port_m << endlog;
}

void UdpIOPort::exportXml(ticpp::Element* pConfig)
{
    IOPort::exportXml(pConfig);
    pConfig->SetAttribute("type", "udp");
    pConfig->SetAttribute("host", host_m);
    pConfig->SetAttribute("port", port_m);
    if (rxport_m > 0)
        pConfig->SetAttribute("rxport", rxport_m);
}

int UdpIOPort::send(const uint8_t* buf, int len)
{
    logger_m.infoStream() << "send(buf, len=" << len << "):"
        << buf << endlog;

    if (sockfd_m >= 0) {
        ssize_t nbytes = pth_sendto(sockfd_m, buf, len, 0,
               (const struct sockaddr *) &addr_m, sizeof (addr_m));
        if (nbytes == len) {
            return nbytes;
        }
        else {
            logger_m.errorStream() << "Unable to send to socket for ioport " << getID() << endlog;
        }
    }
    return -1;
}

int UdpIOPort::get(uint8_t* buf, int len, pth_event_t stop)
{
    logger_m.debugStream() << "get(buf, len=" << len << "):"
        << buf << endlog;
    if (sockfd_m >= 0) {
        socklen_t rl;
        sockaddr_in r;
        rl = sizeof (r);
        memset (&r, 0, sizeof (r));
        ssize_t i = pth_recvfrom_ev(sockfd_m, buf, len, 0,
               (struct sockaddr *) &r, &rl, stop);
//        logger_m.debugStream() << "Out of recvfrom " << i << " rl=" << rl << endlog;
        if (i > 0 && rl == sizeof (r))
        {
            std::string msg(reinterpret_cast<const char*>(buf), i);
            logger_m.debugStream() << "Received '" << msg << "' on ioport " << getID() << endlog;
            return i;
        }
    }
    return -1;
}

#ifdef OPEN_HOME_AUTOMATION
AIOKeepAliveThread::AIOKeepAliveThread(IOPort *port) : port_m(port), isRunning_m(false), stop_m(0)
{}

AIOKeepAliveThread::~AIOKeepAliveThread()
{
    Stop();
}

bool AIOKeepAliveThread::sleep(int delay, pth_sem_t * stop)
{
    struct timeval timeout;
    timeout.tv_sec = delay / 1000;
    timeout.tv_usec = (delay % 1000) * 1000;
    pth_event_t stop_ev = pth_event (PTH_EVENT_SEM, stop);
    pth_select_ev(0, NULL, NULL, NULL, &timeout, stop_ev);
    return (pth_event_status (stop_ev) == PTH_STATUS_OCCURRED);
}

void AIOKeepAliveThread::Run (pth_sem_t * stop1)
{
    stop_m = pth_event (PTH_EVENT_SEM, stop1);
    
    logger_m.debugStream() << "Start AIO keepalive loop." << endlog;

    while (true)
    {
        logger_m.debugStream() << "Doing keepalive." << endlog;

        port_m->send((const uint8_t*)"LOGIN", 5);

        if (sleep(60000, stop1))
            break;
    }

    logger_m.debugStream() << "Out of AIO keepalive loop." << endlog;
    pth_event_free (stop_m, PTH_FREE_THIS);
    stop_m = 0;
}

ArduinoUdpIOPort::ArduinoUdpIOPort() : sockfd_m(-1), port_m(0)
{
    memset (&addr_m, 0, sizeof (addr_m));
}

ArduinoUdpIOPort::~ArduinoUdpIOPort()
{
    if (sockfd_m >= 0)
        close(sockfd_m);
    Logger::getInstance("ArduinoUdpIOPort").debugStream() << "Deleting ArduinoUdpIOPort " << endlog;
}

void ArduinoUdpIOPort::importXml(ticpp::Element* pConfig)
{
    memset (&addr_m, 0, sizeof (addr_m));
    addr_m.sin_family = AF_INET;
    pConfig->GetAttribute("port", &port_m);
    addr_m.sin_port = htons(port_m);
    host_m = pConfig->GetAttribute("host");
    addr_m.sin_addr.s_addr = inet_addr(host_m.c_str());
    IOPort::importXml(pConfig);

    sockfd_m = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_m < 0)
    {
        logger_m.errorStream() << "Unable to create  socket for arduino " << getID() << endlog;
    }    
   
    logger_m.infoStream() << "ArduinoUdpIOPort configured for host " << host_m << " and port " << port_m << endlog;

    addListener(this);

    keepaliveThread_m.reset(new AIOKeepAliveThread(this));
    keepaliveThread_m->startKeepAlive();
}

void ArduinoUdpIOPort::exportXml(ticpp::Element* pConfig)
{
    IOPort::exportXml(pConfig);
    pConfig->SetAttribute("type", "arduino");
    pConfig->SetAttribute("host", host_m);
    pConfig->SetAttribute("port", port_m);
}

int ArduinoUdpIOPort::send(const uint8_t* buf, int len)
{
    logger_m.infoStream() << "send(buf, len=" << len << "):"
        << buf << endlog;

    if (sockfd_m >= 0) {
        ssize_t nbytes = pth_sendto(sockfd_m, buf, len, 0,
               (const struct sockaddr *) &addr_m, sizeof (addr_m));
        if (nbytes == len) {
            return nbytes;
        }
        else {
            logger_m.errorStream() << "Unable to send to socket for arduino " << getID() << endlog;
        }
    }
    return -1;
}

int ArduinoUdpIOPort::get(uint8_t* buf, int len, pth_event_t stop)
{
    //logger_m.debugStream() << "get(buf, len=" << len << "):"
    //    << buf << endlog;
    if (sockfd_m >= 0) {
        socklen_t rl;
        sockaddr_in r;
        rl = sizeof (r);
        memset (&r, 0, sizeof (r));
        ssize_t i = pth_recvfrom_ev(sockfd_m, buf, len, 0,
               (struct sockaddr *) &r, &rl, stop);
        //logger_m.debugStream() << "Out of recvfrom " << i << " rl=" << rl << endlog;
        if (i > 0 && rl == sizeof (r))
        {
            //std::string msg(reinterpret_cast<const char*>(buf), i);
            //logger_m.debugStream() << "Received '" << msg << "' on arduino " << getID() << endlog;
            return i;
        }
    }
    return -1;
}

void ArduinoUdpIOPort::onDataReceived(const uint8_t* buf, unsigned int len)
{
    Object *obj = NULL;
    std::string msg(reinterpret_cast<const char*>(buf), len);

    if (msg.substr(0,7) == "RX_SMS ")
    {
        int pos;

        msg.erase(0, 7);

        pos = msg.find(' ');
        if (pos == std::string::npos)
            return;
        std::string id = msg.substr(0, pos);
        msg.erase(0, pos+1);

        pos = msg.find(' ');
        if (pos == std::string::npos)
            return;
        std::string from = msg.substr(0, pos);
        msg.erase(0, pos+1);

        Services::instance()->getSmsGateway()->receiveSms(from, msg);

        std::string command = "RM_SMS ";
        command.append(id);
        send((const uint8_t *)command.c_str(), command.length());

        /* make sure the SMS is saved */
        saveConfig();
    }
    else if (msg.substr(0,3) == "PIN")
    {
        int pos;

        /* Format: PIN 1/8 relay */
        
        if (msg.find(' ') == std::string::npos)
            return;
        
        msg.erase(0, 4);

        pos = msg.find(' ');
        std::string address = getID();
        address.append("/");
        address.append(msg.substr(0, pos));
        msg.erase(0, pos + 1);

//temperature => ValueObject32

        if (msg != "input" && msg != "output" && msg != "relay" && msg != "DHT11")
        {
            logger_m.errorStream() << "++++ Received INVALID type: " << msg << endlog;
            return;
        }
        
        if (ObjectController::instance()->objectExistsForAddress(address))
        {
            obj = ObjectController::instance()->getObjectForAddress(address);
            obj->decRefCount();            

            if ((msg == "input" && obj->getType() != "1.001") ||
                (msg == "output" && obj->getType() != "1.001") ||
                (msg == "relay" && obj->getType() != "1.001") ||
                (msg == "DHT11" && obj->getType() != "14.xxx"))
            {
                logger_m.errorStream() << "++++ Type changed for " << address << endlog;
                ObjectController::instance()->removeObject(obj);
                obj = NULL;
            }
        }

        if (!obj)
        {
            if (msg == "input")
                obj = Object::create("1.001");
            else if (msg == "output")
                obj = Object::create("1.001");
            else if (msg == "relay")
                obj = Object::create("1.001");
            else if (msg == "DHT11")
                obj = Object::create("14.xxx");

            if (!obj)
            {
                logger_m.errorStream() << "++++ Failed to create new object " << address << endlog; 
                return;
            }

            std::string id = address;
            replace(id.begin(), id.end(), '/', '-');
            obj->setID(id.c_str());
            obj->setAddress(address.c_str());
            obj->setDescr(address.c_str());
            ObjectController::instance()->addObject(obj);

            saveConfig();
        }

        return;
    }
    else if (msg.substr(0,4) == "CMD ")
    {
        int pos;

        /* Format: CMD relay001 67 1:O 2:O */

        msg.erase(0, 4);
        
        /* Format: relay001 67 1:O 2:O */
        
        pos = msg.find(' ');
        std::string module = msg.substr(0, pos);
        msg.erase(0, pos+1);
        
        //logger_m.debugStream() << "Command from " << module
        //                       << " on " << getID() << " found"
        //                       << endlog;

        std::string modLastActDate = module;
        modLastActDate.append("-lastActDate");
        if (!ObjectController::instance()->objectExists(modLastActDate))
        {
            obj = Object::create("11.001");
            if (!obj)
            {
                logger_m.errorStream() << "++++ Failed to create new object "
                                       << modLastActDate << endlog; 
                return;
            }

            std::string id = modLastActDate;
            obj->setID(id.c_str());
            obj->setDescr(id.c_str());
            ObjectController::instance()->addObject(obj);
            Services::instance()->saveConfig();
        }

        if ((obj = ObjectController::instance()->getObject(modLastActDate)) != NULL)
        {
            obj->setValue("now");
            obj->decRefCount();
        }

        std::string modLastActTime = module;
        modLastActTime.append("-lastActTime");
        if (!ObjectController::instance()->objectExists(modLastActTime))
        {
            obj = Object::create("10.001");
            if (!obj)
            {
                logger_m.errorStream() << "++++ Failed to create new object " << modLastActTime << endlog; 
                return;
            }

            std::string id = modLastActTime;
            obj->setID(id.c_str());
            obj->setDescr(id.c_str());
            ObjectController::instance()->addObject(obj);
            Services::instance()->saveConfig();
        }

        if ((obj = ObjectController::instance()->getObject(modLastActTime)) != NULL)
        {
            obj->setValue("now");
            obj->decRefCount();
        }

        /* Format: 67 1:O 2:O */
        
        pos = msg.find(' ');
        char cmd = atoi(msg.substr(0, pos).c_str());
        msg.erase(0, pos+1);
        
        switch (cmd)
        {
            case 'C':
            {
                //logger_m.debugStream() << "Got capabilities from " << module << endlog;
                
                std::string addressPfx = getID();
                addressPfx.append("/");
                addressPfx.append(module);
                addressPfx.append("/");

                while (msg.length() > 0)
                {
                    pos = msg.find(':');
                    if (pos == std::string::npos)
                        return;
                    std::string pin = msg.substr(0, pos);
                    msg.erase(0, pos+1);
                    pos = msg.find(' ');
                    std::string type;
                    if (pos == std::string::npos)
                    {
                        type = msg;
                        msg.clear();
                    }
                    else
                    {
                        type = msg.substr(0, pos);
                        msg.erase(0, pos+1);
                    }

                    std::string address = addressPfx;
                    address.append(pin);

                    //logger_m.debugStream() << "Found pin " << address
                    //                       << " of type " << type 
                    //                       << endlog;

                    if (type == "smsgateway" &&
                        !Services::instance()->getSmsGateway()->isConfigured())
                    {
                        ticpp::Element config("smsgateway");

                        config.SetAttribute("type", "arduino");
                        config.SetAttribute("ioport", this->getID());
                        config.SetAttribute("module", module);
                        
                        Services::instance()->getSmsGateway()->importXml(&config);
                        return;
                    }

                    if (type != "input" && type != "output" &&
                        type != "digin" && type != "digout" &&
                        type != "count" && type != "ultra" && type != "anin")
                    {
                        logger_m.errorStream() << "++++ Received INVALID type: "
                                               << type << endlog;
                        return;
                    }
        
                    obj = NULL;

                    std::string objectId = module;
                    objectId.append("-");
                    objectId.append(pin);
                        
                    if (ObjectController::instance()->objectExists(objectId))
                    {
                        obj = ObjectController::instance()->getObject(objectId);
                        obj->decRefCount();            

                        if (obj->getAddress() != address)
                        {
                            logger_m.errorStream() << "++++ Address changed for "
                                                   << objectId << endlog;
                            ObjectController::instance()->changeObjectAddress(obj, address);
                            Services::instance()->saveConfig();
                        }
                        
                        if ((type == "input" && obj->getType() != "1.001") ||
                            (type == "output" && obj->getType() != "1.001") ||
                            (type == "digin" && obj->getType() != "1.001") ||
                            (type == "digout" && obj->getType() != "1.001") ||
                            (type == "anin" && obj->getType() != "7.xxx") ||
                            (type == "count" && obj->getType() != "7.xxx") ||
                            (type == "ultra" && obj->getType() != "7.xxx"))
                        {
                            logger_m.errorStream() << "++++ Type changed for "
                                                   << objectId << endlog;
                            ObjectController::instance()->removeObject(obj);
                            obj = NULL;
                        }
                    }

                    if (obj)
                    {
                        //logger_m.debugStream() << "Pin " << address
                        //                       << " already exists" 
                        //                       << endlog;
                    }
                    else
                    {
                        if (type == "input" || type == "output" ||
                            type == "digin" || type == "digout")
                            obj = Object::create("1.001");
                        if (type == "count" || type == "ultra" || type == "anin")
                            obj = Object::create("7.xxx");
                        if (!obj)
                        {
                            logger_m.errorStream() << "++++ Failed to create new object " << address << endlog; 
                            return;
                        }

                        obj->setID(objectId.c_str());
                        obj->setAddress(address.c_str());
                        obj->setDescr(objectId.c_str());
                        ObjectController::instance()->addObject(obj);

                        logger_m.debugStream() << "Created new object ID=" << objectId
                                               << " address=" << address 
                                               << endlog;
                        Services::instance()->saveConfig();
                    }
                }

                break;
            }
            case 'V':
            {
                //logger_m.debugStream() << "Got value from " << module << endlog;

                std::string addressPfx = getID();
                addressPfx.append("/");
                addressPfx.append(module);
                addressPfx.append("/");

                while (msg.length() > 0)
                {
                    pos = msg.find(':');
                    if (pos == std::string::npos)
                        return;
                    std::string pin = msg.substr(0, pos);
                    msg.erase(0, pos+1);
                    pos = msg.find(' ');
                    std::string state;
                    if (pos == std::string::npos)
                    {
                        state = msg;
                        msg.clear();
                    }
                    else
                    {
                        state = msg.substr(0, pos);
                        msg.erase(0, pos+1);
                    }

                    std::string address = addressPfx;
                    address.append(pin);

                    //logger_m.debugStream() << "Found pin " << address
                    //                       << " in state " << state 
                    //                       << endlog;

                    if (ObjectController::instance()->objectExistsForAddress(address))
                    {
                        obj = ObjectController::instance()->getObjectForAddress(address);
                        obj->updateValueFromExternalInput(state);
                        obj->decRefCount();            
                    }
                }
                break;
            }
            case 12:
            {
                //presence
                break;
            }
            case 0x31:
            {
                //ICSC_SMS_RX
                pos = msg.find(' ');
                if (pos == std::string::npos)
                    return;
                std::string from = msg.substr(0, pos);
                msg.erase(0, pos+1);
                    
                Services::instance()->getSmsGateway()->receiveSms(from, msg);
                break;
            }
            default:
                logger_m.debugStream() << "Unhandled command from " << module
                                       << " on " << getID() << " found" << endlog;
                break;
        }

        return;
    }

    /* pin state update */
    if (msg.find(' ') == std::string::npos)
        return;

    std::string address = getID();
    address.append("/");
    address.append(msg.substr(0, msg.find(' ')));

    //logger_m.errorStream() << "++++ Received update for '" << address << endlog;

    if (!ObjectController::instance()->objectExistsForAddress(address))
    {
        return;
    }

    obj = ObjectController::instance()->getObjectForAddress(address);
    obj->decRefCount();

    if (obj && msg.find(' ') != std::string::npos)
    {
        std::string value = msg.substr(msg.find(' '));
        while (value.length() > 0 && value.at(0) == ' ')
            value.erase(0, 1);
        try
        {
            obj->updateValueFromExternalInput(value);
        }
        catch (ticpp::Exception& ex)
        {
            logger_m.errorStream() << "Parsing failed: " << ex.m_details << endlog;
        }
    }
}
#endif

TcpClientIOPort::TcpClientIOPort() : sockfd_m(-1), port_m(0)
{
    memset (&addr_m, 0, sizeof (addr_m));
}

TcpClientIOPort::~TcpClientIOPort()
{
    if (sockfd_m >= 0)
        close(sockfd_m);
    Logger::getInstance("TcpClientIOPort").debugStream() << "Deleting TcpClientIOPort " << endlog;
}

void TcpClientIOPort::importXml(ticpp::Element* pConfig)
{
    memset (&addr_m, 0, sizeof (addr_m));
    addr_m.sin_family = AF_INET;
    pConfig->GetAttribute("port", &port_m);
    addr_m.sin_port = htons(port_m);
    host_m = pConfig->GetAttribute("host");
    addr_m.sin_addr.s_addr = inet_addr(host_m.c_str());
    std::string perm = pConfig->GetAttribute("permanent");
    permanent_m = (perm == "true" || perm == "yes");
    IOPort::importXml(pConfig);

    logger_m.infoStream() << "TcpClientIOPort " << (permanent_m?"(permanent) ":"") << "configured for host " << host_m << " and port " << port_m << endlog;
}

void TcpClientIOPort::exportXml(ticpp::Element* pConfig)
{
    IOPort::exportXml(pConfig);
    pConfig->SetAttribute("type", "tcp");
    pConfig->SetAttribute("host", host_m);
    pConfig->SetAttribute("port", port_m);
    if (permanent_m)
        pConfig->SetAttribute("permanent", "true");
}

int TcpClientIOPort::send(const uint8_t* buf, int len)
{
    logger_m.infoStream() << "send(buf, len=" << len << "):"
        << buf << endlog;

    connectToServer();
    if (sockfd_m >= 0) {
        ssize_t nbytes = pth_write(sockfd_m, buf, len);
        if (nbytes == len) {
            if (!permanent_m)
                disconnectFromServer();
            return nbytes;
        }
        else {
            logger_m.errorStream() << "Error while sending data for ioport " << getID() << endlog;
            disconnectFromServer();
        }
    }
    return -1;
}

int TcpClientIOPort::get(uint8_t* buf, int len, pth_event_t stop)
{
    logger_m.debugStream() << "get(buf, len=" << len << ")" << endlog;
    bool retry = true;
    while (retry) {
        connectToServer();
        if (sockfd_m >= 0) {
            ssize_t i = pth_read_ev(sockfd_m, buf, len, stop);
            logger_m.debugStream() << "Out of read " << i << endlog;
            if (i > 0)
            {
                std::string msg(reinterpret_cast<const char*>(buf), i);
                logger_m.debugStream() << "Received '" << msg << "' on ioport " << getID() << endlog;
                return i;
            }
            else {
                disconnectFromServer();
                if (pth_event_status (stop) == PTH_STATUS_OCCURRED)
                    retry = false;
            }
        }
        else {
            struct timeval tv;
            tv.tv_sec = 60;
            tv.tv_usec = 0;
            pth_select_ev(0,0,0,0,&tv,stop);
            if (pth_event_status (stop) == PTH_STATUS_OCCURRED)
                retry = false;
        }
    }
    logger_m.debugStream() << "Abort get() on ioport " << getID() << endlog;
    return -1;
}

void TcpClientIOPort::connectToServer()
{
    if (sockfd_m < 0) {
        sockfd_m = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_m >= 0) {
            if (pth_connect(sockfd_m, (const struct sockaddr *) &addr_m, sizeof (addr_m)) < 0) {
                logger_m.errorStream() << "Unable to connect to server for ioport " << getID() << endlog;
                disconnectFromServer();
            }
        }
        else {
            logger_m.errorStream() << "Unable to create  socket for ioport " << getID() << endlog;
        }    
    }
}

void TcpClientIOPort::disconnectFromServer()
{
    if (close(sockfd_m) < 0) {
        logger_m.errorStream() << "Unable to close connection to server for ioport " << getID() << endlog;
    }
    sockfd_m = -1;
}

SerialIOPort::SerialIOPort() : fd_m(-1)
{
    memset (&newtio_m, 0, sizeof (newtio_m));
}

SerialIOPort::~SerialIOPort()
{
    if (fd_m >= 0) {
    int rxport_m;
        // restore old port settings
        tcsetattr(fd_m, TCSANOW, &oldtio_m);
        close(fd_m);
    }
    Logger::getInstance("SerialIOPort").debugStream() << "Deleting SerialIOPort " << endlog;
}

void SerialIOPort::importXml(ticpp::Element* pConfig)
{
    ErrorMessage err;
    int speed;
    struct termios newtio;
    std::string framing = pConfig->GetAttributeOrDefault("framing", "8N1");
    std::string flow = pConfig->GetAttributeOrDefault("flow", "none");
    memset (&newtio, 0, sizeof (newtio));
    pConfig->GetAttribute("speed", &speed);
    newtio.c_cflag = CLOCAL | CREAD;
    newtio.c_iflag = ICRNL;
    newtio.c_oflag = 0;
    newtio.c_lflag = ICANON;

    newtio.c_cc[VTIME] = 0; // inter character timer disabled
    newtio.c_cc[VMIN]  = 1; // block until 1 character arrives
    switch (framing[0]) {
        case '5':
            newtio.c_cflag |= CS5;
            break;
        case '6':
            newtio.c_cflag |= CS6;
            break;
        case '7':
            newtio.c_cflag |= CS7;
            break;
        case '8':
            newtio.c_cflag |= CS8;
            break;
        default:
            err << "Unsupported nb of data bits '" << framing[0] << "' for serial port";
            err.logAndThrow(logger_m);
    }
    switch (framing[1]) {
        case 'E':
            newtio.c_cflag |= PARENB;
            break;
        case 'O':
            newtio.c_cflag |= (PARENB | PARODD);
            break;
        case 'N':
            newtio.c_iflag |= IGNPAR;
            break;
        default:
            err << "Unsupported parity '" << framing[1] << "' for serial port";
            err.logAndThrow(logger_m);
    }

    if (framing[2] == '2')
        newtio.c_cflag |= CSTOPB;
    else if (framing[2] != '1') {
        err << "Unsupported nb of stop bits '" << framing[2] << "' for serial port";
        err.logAndThrow(logger_m);
    }

    if (flow == "xon-xoff")
        newtio.c_iflag |= (IXON | IXOFF);
    else if (flow == "rts-cts")
        newtio.c_cflag |= CRTSCTS;
    else if (flow != "none") {
        err << "Unsupported flow control '" << flow << "' for serial port";
        err.logAndThrow(logger_m);
    }

    switch (speed) {
        case 200:
            speed_m = B200;
            break;
        case 300:
            speed_m = B300;
            break;
        case 600:
            speed_m = B600;
            break;
        case 1200:
            speed_m = B1200;
            break;
        case 1800:
            speed_m = B1800;
            break;
        case 2400:
            speed_m = B2400;
            break;
        case 4800:
            speed_m = B4800;
            break;
        case 9600:
            speed_m = B9600;
            break;
        case 19200:
            speed_m = B19200;
            break;
        case 38400:
            speed_m = B38400;
            break;
        case 57600:
            speed_m = B57600;
            break;
        case 115200:
            speed_m = B115200;
            break;
        case 230400:
            speed_m = B230400;
            break;
        default:
            err << "Unsupported speed '" << speed << "' for serial port";
            err.logAndThrow(logger_m);
    }
    cfsetispeed(&newtio, speed_m);
    cfsetospeed(&newtio, speed_m);
    pConfig->GetAttribute("dev", &dev_m);
    newtio_m = newtio;

    IOPort::importXml(pConfig);

    fd_m = open(dev_m.c_str(), O_RDWR | O_NOCTTY );
    if (fd_m >= 0) {
        // Save previous port settings
        tcgetattr(fd_m, &oldtio_m);
        tcflush(fd_m, TCIFLUSH);
        tcsetattr(fd_m, TCSANOW, &newtio_m);
        logger_m.infoStream() << "SerialIOPort configured for device " << dev_m << endlog;
    }
    else {
        logger_m.errorStream() << "Unable to open device '" << dev_m << "' for ioport " << getID() << endlog;
    }    
}

void SerialIOPort::exportXml(ticpp::Element* pConfig)
{
    int speed;
    IOPort::exportXml(pConfig);
    pConfig->SetAttribute("type", "serial");
    pConfig->SetAttribute("dev", dev_m);
    switch (speed_m) {
        case B200:
            speed = 200;
            break;
        case B300:
            speed = 300;
            break;
        case B600:
            speed = 600;
            break;
        case B1200:
            speed = 1200;
            break;
        case B1800:
            speed = 1800;
            break;
        case B2400:
            speed = 2400;
            break;
        case B4800:
            speed = 4800;
            break;
        case B9600:
            speed = 9600;
            break;
        case B19200:
            speed = 19200;
            break;
        case B38400:
            speed = 38400;
            break;
        case B57600:
            speed = 57600;
            break;
        case B115200:
            speed = 115200;
            break;
        case B230400:
            speed = 230400;
            break;
        default:
            speed = 9600;
            break;
    }
    pConfig->SetAttribute("speed", speed);

    std::string framing;
    switch (newtio_m.c_cflag & CSIZE) {
        case CS5:
            framing.push_back('5');
            break;
        case CS6:
            framing.push_back('6');
            break;
        case CS7:
            framing.push_back('7');
            break;
        default:
        case CS8:
            framing.push_back('8');
            break;
    }
    if (newtio_m.c_cflag & PARENB == 0)
        framing.push_back('N');
    else if (newtio_m.c_cflag & PARODD)
        framing.push_back('O');
    else
        framing.push_back('E');

    if (newtio_m.c_cflag & CSTOPB)
        framing.push_back('2');
    else
        framing.push_back('1');

    pConfig->SetAttribute("framing", framing);

    if (newtio_m.c_cflag & CRTSCTS)
        pConfig->SetAttribute("flow", "rts-cts");
    else if (newtio_m.c_iflag & IXON)
        pConfig->SetAttribute("flow", "xon-xoff");
    else
        pConfig->SetAttribute("flow", "none");
}

int SerialIOPort::send(const uint8_t* buf, int len)
{
    logger_m.infoStream() << "send(buf, len=" << len << "):"
        << buf << endlog;

    if (fd_m >= 0) {
        ssize_t nbytes = pth_write(fd_m, buf, len);
        if (nbytes == len) {
            return nbytes;
        }
        else {
            logger_m.errorStream() << "Unable to send to socket for ioport " << getID() << endlog;
        }
    }
    return -1;
}

int SerialIOPort::get(uint8_t* buf, int len, pth_event_t stop)
{
//    logger_m.debugStream() << "get(buf, len=" << len << "):"
//        << buf << endlog;
    if (fd_m >= 0) {
        ssize_t i = pth_read_ev(fd_m, buf, len, stop);
//        logger_m.debugStream() << "Out of recvfrom " << i << " rl=" << rl << endlog;
        if (i > 0)
        {
            if (i < len)
                buf[i] = 0;

//            std::string msg(reinterpret_cast<const char*>(buf), i);
//            logger_m.debugStream() << "Received '" << msg << "' on ioport " << getID() << endlog;
            return i;
        }
        return 0;
    }
    return -1;
}

TxAction::TxAction(): varFlags_m(0), hex_m(false)
{}

TxAction::~TxAction()
{}

void TxAction::importXml(ticpp::Element* pConfig)
{
    int i=0;
    port_m = pConfig->GetAttribute("ioport");
    std::string data = pConfig->GetAttribute("data");
    if (!IOPortManager::instance()->getPort(port_m))
    {
        std::stringstream msg;
        msg << "TxAction: IO Port ID not found: '" << port_m << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }

    varFlags_m = 0;
    if (pConfig->GetAttributeOrDefault("hex", "false") != "false")
    {
        hex_m = true;
        while (i < data.length())
        {
            std::istringstream ss(data.substr(i, 2));
            ss.setf(std::ios::hex, std::ios::basefield);
            int value = 0;
            ss >> value;
            data_m.push_back(static_cast<char>(value));
            i += 2;
        }
        logger_m.infoStream() << "TxAction: Configured to send hex data to ioport " << port_m << endlog;
    }
    else
    {
        data_m = data;

        if (pConfig->GetAttribute("var") == "true")
        {
            varFlags_m = VarEnabled;
            if (parseVarString(data, true))
                varFlags_m |= VarData;
        }
        logger_m.infoStream() << "TxAction: Configured to send '" << data_m << "' to ioport " << port_m << endlog;
    }
}

void TxAction::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("type", "ioport-tx");
    if (hex_m)
    {
        int i = 0;
        pConfig->SetAttribute("hex", "true");
        pConfig->SetAttribute("data", data_m);
        std::ostringstream ss;
        ss.setf(std::ios::hex, std::ios::basefield);
        ss.fill('0');
        while (i < data_m.length())
            ss << std::setw(2) << int(data_m[i++]);
        pConfig->SetAttribute("data", ss.str());
    }
    else
        pConfig->SetAttribute("data", data_m);
    pConfig->SetAttribute("ioport", port_m);
    if (varFlags_m & VarEnabled)
        pConfig->SetAttribute("var", "true");

    Action::exportXml(pConfig);
}

void TxAction::Run (pth_sem_t * stop)
{
    if (sleep(delay_m, stop))
        return;
    try
    {
        IOPort* port = IOPortManager::instance()->getPort(port_m);
        if (!port)
            throw ticpp::Exception("IO Port ID not found.");
        sendData(port);
    }
    catch( ticpp::Exception& ex )
    {
       logger_m.warnStream() << "Error in TxAction on port '" << port_m << "': " << ex.m_details << endlog;
    }
}

void TxAction::sendData(IOPort* port)
{
    std::string data = data_m;
    if (varFlags_m & VarData)
        parseVarString(data);
    if (hex_m)
        logger_m.infoStream() << "Execute TxAction send hex data to ioport " << port->getID() << endlog;
    else
        logger_m.infoStream() << "Execute TxAction send '" << data << "' to ioport " << port->getID() << endlog;
    const uint8_t* u8data = reinterpret_cast<const uint8_t*>(data.c_str());
    int len = data.length();
    int ret = port->send(u8data, len);
    while (ret < len) {
        if (ret <= 0)
            throw ticpp::Exception("Unable to send data.");
        len -= ret;
        u8data += ret;
        ret = port->send(u8data, len);
    }
}

RxCondition::RxCondition(ChangeListener* cl) : value_m(false), cl_m(cl)
{}

RxCondition::~RxCondition()
{
    IOPort* port = IOPortManager::instance()->getPort(port_m);
    if (port)
        port->removeListener(this);
}

bool RxCondition::evaluate()
{
    return value_m;
}

void RxCondition::importXml(ticpp::Element* pConfig)
{
    if (!cl_m)
        throw ticpp::Exception("Rx condition on IO port is not supported in this context");
    port_m = pConfig->GetAttribute("ioport");
    exp_m = pConfig->GetAttribute("expected");
    IOPort* port = IOPortManager::instance()->getPort(port_m);
    if (!port)
    {
        std::stringstream msg;
        msg << "RxCondition: IO Port ID not found: '" << port_m << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
    port->addListener(this);
    logger_m.infoStream() << "RxCondition: configured to watch for '" << exp_m << "' on ioport " << port_m << endlog;
}

void RxCondition::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("type", "ioport-rx");
    pConfig->SetAttribute("expected", exp_m);
    pConfig->SetAttribute("ioport", port_m);
}

void RxCondition::statusXml(ticpp::Element* pStatus)
{
    pStatus->SetAttribute("type", "ioport-rx");
    pStatus->SetAttribute("ioport", port_m);
}

void RxCondition::onDataReceived(const uint8_t* buf, unsigned int len)
{
    if (len > exp_m.length())
        len = exp_m.length();
    std::string rx(reinterpret_cast<const char*>(buf), len); 
    if (cl_m && exp_m == rx)
    {
        logger_m.debugStream() << "RxCondition: expected message received: '" << exp_m << "'" << endlog;
        value_m = true;
        cl_m->onChange(0);
        value_m = false;
        cl_m->onChange(0);
    }        
}

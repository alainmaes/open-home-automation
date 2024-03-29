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

#include "services.h"
#include "ioport.h"
#ifdef OPEN_HOME_AUTOMATION
#include "usercontroller.h"
#endif

Services* Services::instance_m;

Services::Services() : xmlServer_m(0), persistentStorage_m(0)
{}

Services::~Services()
{
    stop();
    if (xmlServer_m)
        delete xmlServer_m;
    if (persistentStorage_m)
        delete persistentStorage_m;
    IOPortManager::reset();
}

Services* Services::instance()
{
    if (instance_m == 0)
        instance_m = new Services();
    return instance_m;
}

void Services::start()
{
    timers_m.startManager();
    knxConnection_m.startConnection();
#ifdef OPEN_HOME_AUTOMATION
    domintellConnection_m.startConnection();
    IOPortManager::instance()->startAutoDetect();
#endif
}

void Services::stop()
{
    infoStream("Services") << "Stopping services" << endlog;
    timers_m.stopManager();
    knxConnection_m.stopConnection();
#ifdef OPEN_HOME_AUTOMATION
    domintellConnection_m.stopConnection();
#endif
}

void Services::createDefault()
{
    if (xmlServer_m)
        delete xmlServer_m;
    xmlServer_m = new XmlInetServer(1028);
}

void Services::importXml(ticpp::Element* pConfig)
{
#ifdef OPEN_HOME_AUTOMATION
    ticpp::Element* pIOPorts = pConfig->FirstChildElement("ioports", false);
    if (pIOPorts)
        IOPortManager::instance()->importXml(pIOPorts);
#endif
    ticpp::Element* pSmsGateway = pConfig->FirstChildElement("smsgateway", false);
    if (pSmsGateway)
        smsGateway_m.importXml(pSmsGateway);
    ticpp::Element* pEmailGateway = pConfig->FirstChildElement("emailserver", false);
    if (pEmailGateway)
        emailGateway_m.importXml(pEmailGateway);

#ifdef OPEN_HOME_AUTOMATION
    ticpp::Element* pMessages = pConfig->FirstChildElement("message-storage", false);
    if (pMessages)
    {
        if (messageController_m)
            delete messageController_m;
        messageController_m = new MessageController(pMessages);
    }
#endif

    ticpp::Element* pXmlServer = pConfig->FirstChildElement("xmlserver", false);
    if (pXmlServer)
    {
        if (xmlServer_m)
            delete xmlServer_m;
        xmlServer_m = XmlServer::create(pXmlServer);
    }
    ticpp::Element* pKnxConnection = pConfig->FirstChildElement("knxconnection", false);
    if (pKnxConnection)
        knxConnection_m.importXml(pKnxConnection);
#ifdef OPEN_HOME_AUTOMATION
    ticpp::Element* pDomintellConnection = pConfig->FirstChildElement("domintellconnection", false);
    if (pDomintellConnection)
        domintellConnection_m.importXml(pDomintellConnection);
#endif
    ticpp::Element* pExceptionDays = pConfig->FirstChildElement("exceptiondays", false);
    if (pExceptionDays)
        exceptionDays_m.importXml(pExceptionDays);
    ticpp::Element* pLocationInfo = pConfig->FirstChildElement("location", false);
    if (pLocationInfo)
        locationInfo_m.importXml(pLocationInfo);
    ticpp::Element* pPersistence = pConfig->FirstChildElement("persistence", false);
    if (pPersistence)
    {
        if (persistentStorage_m)
            delete persistentStorage_m;
        persistentStorage_m = PersistentStorage::create(pPersistence);
    }
#ifndef OPEN_HOME_AUTOMATION
    ticpp::Element* pIOPorts = pConfig->FirstChildElement("ioports", false);
    if (pIOPorts)
        IOPortManager::instance()->importXml(pIOPorts);
#endif
}

void Services::exportXml(ticpp::Element* pConfig)
{
    ticpp::Element pSmsGateway("smsgateway");
    smsGateway_m.exportXml(&pSmsGateway);
    pConfig->LinkEndChild(&pSmsGateway);

    ticpp::Element pEmailGateway("emailserver");
    emailGateway_m.exportXml(&pEmailGateway);
    pConfig->LinkEndChild(&pEmailGateway);

#ifdef OPEN_HOME_AUTOMATION
    if (messageController_m)
    {
        ticpp::Element pMessages("message-storage");
        messageController_m->exportXml(&pMessages);
        pConfig->LinkEndChild(&pMessages);
    }
#endif

    if (xmlServer_m)
    {
        ticpp::Element pXmlServer("xmlserver");
        xmlServer_m->exportXml(&pXmlServer);
        pConfig->LinkEndChild(&pXmlServer);
    }

    ticpp::Element pKnxConnection("knxconnection");
    knxConnection_m.exportXml(&pKnxConnection);
    pConfig->LinkEndChild(&pKnxConnection);

#ifdef OPEN_HOME_AUTOMATION
    ticpp::Element pDomintellConnection("domintellconnection");
    domintellConnection_m.exportXml(&pDomintellConnection);
    pConfig->LinkEndChild(&pDomintellConnection);
#endif

    ticpp::Element pExceptionDays("exceptiondays");
    exceptionDays_m.exportXml(&pExceptionDays);
    pConfig->LinkEndChild(&pExceptionDays);

#ifdef OPEN_HOME_AUTOMATION
    if (!locationInfo_m.isEmpty())
    {
        ticpp::Element pLocationInfo("location");
        locationInfo_m.exportXml(&pLocationInfo);
        pConfig->LinkEndChild(&pLocationInfo);
    }
#endif

    if (persistentStorage_m)
    {
        ticpp::Element pPersistence("persistence");
        persistentStorage_m->exportXml(&pPersistence);
        pConfig->LinkEndChild(&pPersistence);
    }

    ticpp::Element pIOPorts("ioports");
    IOPortManager::instance()->exportXml(&pIOPorts);
    pConfig->LinkEndChild(&pIOPorts);
}

#ifdef OPEN_HOME_AUTOMATION
void Services::saveConfig()
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
            errorStream("Services") << "Unable to write config to file: "
                                    << ex.m_details << endlog;
            //throw "Error writing config to file";
        }
    }
}
#endif

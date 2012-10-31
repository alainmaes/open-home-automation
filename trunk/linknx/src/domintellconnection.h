/*
    LinKNX KNX home automation platform
    Copyright (C) 2007 Jean-François Meessen <linknx@ouaye.net>
 
    Portions of code borrowed to EIBD (http://bcusdk.sourceforge.net/)
    Copyright (C) 2005-2006 Martin Kögler <mkoegler@auto.tuwien.ac.at>
 
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

#ifndef DOMINTELLCONNECTION_H
#define DOMINTELLCONNECTION_H

#include "sys/cdefs.h"
#include "stdint.h"
#include <pthsem.h>

#include "config.h"
#include "logger.h"
#include "threads.h"
#include <string>
#include "ticpp.h"

/** type represents a connection to domintell socket */
typedef struct _Deth02Connection Deth02Connection;

class BusEventListener
{
public:
    virtual ~BusEventListener() {};
    virtual void onBusEvent(const uint8_t* buf, int len) = 0;
};

class Object;
class PeriodicTask;

class DomintellConnection : public Thread
{
public:
    DomintellConnection();
    virtual ~DomintellConnection();

    virtual void importXml(ticpp::Element* pConfig);
    virtual void exportXml(ticpp::Element* pConfig);

    void startConnection() { isRunning_m = true; Start(); };
    void stopConnection();

    void addBusEventListener(BusEventListener *listener);
    bool removeBusEventListener(BusEventListener *listener);
    void write(uint8_t* buf, int len);
    int checkInput();

private:
    Deth02Connection *con_m;
    bool isRunning_m;
    pth_event_t stop_m;
    std::string url_m;
    bool autodiscovery_m;
    BusEventListener *listener_m;

    int kaInterval_m;
    time_t lastActivity_m;

    void Run (pth_sem_t * stop);
    static Logger& logger_m;
};

#endif

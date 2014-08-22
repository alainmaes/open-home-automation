/* OPEN_HOME_AUTOMATION
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

#ifndef MESSAGECONTROLLER_H
#define MESSAGECONTROLLER_H

#include <list>
#include <string>
#include "config.h"
#include "logger.h"
#include "ticpp.h"
#include "sqlite3.h"

class MessageController
{
public:
    MessageController(ticpp::Element* pConfig);
    ~MessageController();
    void exportXml(ticpp::Element* pConfig);

    void storeMessage(std::string from, std::string message);
    void removeMessage(std::string id);
    void exportMessages(ticpp::Element* pObjects);

private:
    sqlite3 *db;
    std::string db_m;

protected:
    static Logger& logger_m;
};

#endif //MESSAGECONTROLLER_H

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

#ifndef USERCONTROLLER_H
#define USERCONTROLLER_H

#include <list>
#include <string>
#include <map>
#include <stdint.h>
#include "config.h"
#include "logger.h"
#include "ticpp.h"

class User
{
public:
    User();
    ~User();

    static User* create(ticpp::Element* pConfig);
    
    virtual void importXml(ticpp::Element* pConfig);
    virtual void exportXml(ticpp::Element* pConfig);

    virtual bool authenticate(std::string pwType,
                              std::string password);
protected:
    std::string id_m;
    std::string password_m;

    static Logger& logger_m;
};

class UserController
{
public:
    static UserController* instance();
    static void reset()
    {
        if (instance_m)
            delete instance_m;
        instance_m = 0;
    };

    virtual void importXml(ticpp::Element* pConfig);
    virtual void exportXml(ticpp::Element* pConfig);

    virtual std::list<User*> getUsers();

    virtual bool authenticate(std::string id,
                              std::string pwType,
                              std::string password);
private:
    UserController();
    virtual ~UserController();

    typedef std::pair<std::string, User*> UserPair_t;
    typedef std::map<std::string, User*> UserMap_t;
    UserMap_t userMap_m;
    static UserController *instance_m;
};

#endif //USERCONTROLLER_H

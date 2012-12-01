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

#include "usercontroller.h"
#include "services.h"
#include "base64.h"
#include <cmath>
#include <cassert>

UserController* UserController::instance_m;

Logger& User::logger_m(Logger::getInstance("User"));

User::User()
{
    //
}

User::~User()
{
    //
}

User *User::create(ticpp::Element* pConfig)
{
    User *user = new User();
    user->importXml(pConfig);
    return user;
}

void User::importXml(ticpp::Element* pConfig)
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

    std::string type = pConfig->GetAttribute("pw-type");
    if (type == "")
        throw ticpp::Exception("Missing or empty password type");
    if (type != "encoded" && type != "plain")
        throw ticpp::Exception("Invalid password type");

    std::string password = pConfig->GetAttribute("password");
    if (password == "")
        throw ticpp::Exception("Missing or empty password");
    
    if (type == "encoded")
    {
        password_m = base64_decode(password);
    }
    else
    {
        password_m = password;
    }
}

void User::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("id", id_m);
    pConfig->SetAttribute("pw-type", "encoded");
    pConfig->SetAttribute("password",
                           base64_encode(
                               (const unsigned char*)password_m.c_str(),
                               password_m.length()));
}

bool User::authenticate(std::string pwType,
                        std::string password)
{
    if (pwType == "encoded")
    {
        return (password_m == base64_decode(password));
    }
    else if (pwType == "plain")
    {
        return (password_m == password);
    }
    else
    {
        return false;
    }
}

UserController::UserController()
{
    //
}

UserController::~UserController()
{
    UserMap_t::iterator it;
    for (it = userMap_m.begin(); it != userMap_m.end(); it++)
        delete (*it).second;
}

UserController* UserController::instance()
{
    if (instance_m == 0)
        instance_m = new UserController();
    return instance_m;
}

void UserController::importXml(ticpp::Element* pConfig)
{
    ticpp::Iterator< ticpp::Element > child("user");
    for (child = pConfig->FirstChildElement("user", false);
         child != child.end();
         child++)
    {
        std::string id = child->GetAttribute("id");
        bool del = child->GetAttribute("delete") == "true";
        UserMap_t::iterator it = userMap_m.find(id);
        if (it != userMap_m.end())
        {
            User* user = it->second;

            if (del)
            {
                delete user;
                userMap_m.erase(it);
            }
            else
            {
                user->importXml(&(*child));
                userMap_m.insert(UserPair_t(id, user));
            }
        }
        else
        {
            if (del)
                throw ticpp::Exception("User not found");
            User* user = User::create(&(*child));
            userMap_m.insert(UserPair_t(id, user));
        }
    }
}

void UserController::exportXml(ticpp::Element* pConfig)
{
    UserMap_t::iterator it;
    for (it = userMap_m.begin(); it != userMap_m.end(); it++)
    {
        ticpp::Element pElem("user");
        (*it).second->exportXml(&pElem);
        pConfig->LinkEndChild(&pElem);
    }
}

// Delivers all users
std::list<User*> UserController::getUsers()
{
    std::list<User*> objects;
    UserMap_t::iterator it;
    for (it = userMap_m.begin(); it != userMap_m.end(); it++)
    {
      //it->second->incRefCount();
      objects.push_back((*it).second);
    }
    return objects;
}

bool UserController::authenticate(std::string id,
                                  std::string pwType,
                                  std::string password)
{
    UserMap_t::iterator it = userMap_m.find(id);
    if (it == userMap_m.end())
    {
        //user not found
        return false;
    }

    User* user = it->second;
    return user->authenticate(pwType, password);
}

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

#include "messagecontroller.h"
#include "services.h"
#include "base64.h"
#include <cmath>
#include <cassert>

Logger& MessageController::logger_m(Logger::getInstance("MessageController"));

MessageController::MessageController(ticpp::Element* pConfig)
{
    int rc = 0;
    char *zErrMsg = 0;

    db_m = pConfig->GetAttribute("db");
    if (db_m == "")
        throw ticpp::Exception("MessageController: database name missing");

    rc = sqlite3_open(db_m.c_str(), &db);
    if (rc)
    {
        logger_m.errorStream() << "Can't open database: " << sqlite3_errmsg(db) << endlog;
        sqlite3_close(db);
	throw ticpp::Exception("MessageController: error initializing client");
    }

    std::stringstream sql;
    sql << "CREATE TABLE IF NOT EXISTS `messages` "
        << "(id INTEGER PRIMARY KEY AUTOINCREMENT, "
        << "sender VARCHAR(128), message VARCHAR(256), "
        << "ts TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
logger_m.errorStream() << sql.str().c_str();
    rc = sqlite3_exec(db, sql.str().c_str(), NULL, 0, &zErrMsg);
    if (rc)
    {
        logger_m.errorStream() << "Can't create table: " << zErrMsg << endlog;
        sqlite3_close(db);
	throw ticpp::Exception("MessageController: error initializing client");
    }
}

MessageController::~MessageController()
{
    sqlite3_close(db);    
}

void MessageController::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("type", "sqlite");
    pConfig->SetAttribute("db", db_m);
}

void MessageController::storeMessage(std::string from, std::string message)
{
    char *zErrMsg = 0;

    logger_m.infoStream() << "Storing message '" << message
                          << "' from " << from << endlog;

    std::stringstream sql;
    sql << "INSERT OR IGNORE INTO `messages` (`sender`, `message`) VALUES ('"
        << from << "', '" << message << "');";
    int rc = sqlite3_exec(db, sql.str().c_str(), NULL, 0, &zErrMsg);
    if (rc)
    {
        logger_m.errorStream() << "Can't insert record: " << zErrMsg << endlog;
    }
}

void MessageController::exportMessages(ticpp::Element* pObjects)
{
    sqlite3_stmt *stmt;
    
    std::string queryStr = "SELECT id, sender, message, ts from `messages`;";
    
    int rc = sqlite3_prepare(db, queryStr.c_str(),
                             strlen(queryStr.c_str()), &stmt, 0);
    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        ticpp::Element pElem("message");
        pElem.SetAttribute("id", sqlite3_column_text(stmt, 0));
        pElem.SetAttribute("from", (char *)sqlite3_column_text(stmt, 1));
        pElem.SetAttribute("message", (char *)sqlite3_column_text(stmt, 2));
        pElem.SetAttribute("received", sqlite3_column_text(stmt, 3));
        pObjects->LinkEndChild(&pElem);
    }
    sqlite3_finalize(stmt);
}

void MessageController::removeMessage(std::string id)
{
    char *zErrMsg = 0;
    std::stringstream sql;

    if (id == "all")
    {
        logger_m.infoStream() << "Removing all messages" << endlog;
        sql << "DELETE FROM `messages`;";
    }
    else
    {
        logger_m.infoStream() << "Removing message " << id << endlog;
        sql << "DELETE FROM `messages` WHERE `id`='" << id << "';";
    }

    int rc = sqlite3_exec(db, sql.str().c_str(), NULL, 0, &zErrMsg);
    if (rc)
    {
        logger_m.errorStream() << "Can't remove record: " << zErrMsg << endlog;
    }
}

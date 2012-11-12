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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <iostream>
#include <iomanip>
#include "objectcontroller.h"
#include "domintellconnection.h"

#include <algorithm>
#include "ruleserver.h"
#include "services.h"


/** unsigned char */
typedef uint8_t uchar;

/** DETH02 Connection internal */
struct _Deth02Connection
{
    int (*complete) (Deth02Connection *);
    /** file descriptor */
    int fd;
    pth_event_t ev;
    unsigned readlen;
    /** buffer */
    uchar buf[1024];
    /** buffer size */
    unsigned buflen;
};

Logger& DomintellConnection::logger_m(Logger::getInstance("DomintellConnection"));


void hexdump(void *ptr, int buflen) {
  unsigned char *buf = (unsigned char*)ptr;
  int i, j;
  for (i=0; i<buflen; i+=16) {
    printf("%06x: ", i);
    for (j=0; j<16; j++) 
      if (i+j < buflen)
        printf("%02x ", buf[i+j]);
      else
        printf("   ");
    printf(" ");
    for (j=0; j<16; j++) 
      if (i+j < buflen)
        printf("%c", isprint(buf[i+j]) ? buf[i+j] : '.');
    printf("\n");
  }
}

int
Deth02Close(Deth02Connection * con)
{
    if (!con)
    {
        errno = EINVAL;
        return -1;
    }
    close(con->fd);
    free(con);
    return 0;
}

/** resolve host name */
static int
Deth02GetHostIP (struct sockaddr_in *sock, const char *Name)
{
#ifdef HAVE_GETHOSTBYNAME_R
    int len = 2000;
    struct hostent host;
    char *buf = (char *) malloc (len);
    int res;
    int err;
#endif

    struct hostent *h;
    if (!Name)
        return 0;
    memset (sock, 0, sizeof (*sock));
#ifdef HAVE_GETHOSTBYNAME_R

    do
    {
        res = gethostbyname_r (Name, &host, buf, len, &h, &err);
        if (res == ERANGE)
        {
            len += 2000;
            buf = (char *) realloc (buf, len);
        }
        if (!buf)
            return 0;
    }
    while (res == ERANGE);

    if (res || !h)
    {
        free (buf);
        return 0;
    }
#else
    h = gethostbyname (Name);
    if (!h)
        return 0;
#endif

    sock->sin_family = h->h_addrtype;
    sock->sin_addr.s_addr = (*((unsigned long *) h->h_addr_list[0]));
#ifdef HAVE_GETHOSTBYNAME_R

    if (buf)
        free (buf);
#endif

    return 1;
}
Deth02Connection *
Deth02SocketRemote(const char *host, int port)
{
    Deth02Connection *con = (Deth02Connection *) malloc(sizeof(Deth02Connection));
    struct sockaddr_in addr;
    if (!con)
    {
        errno = ENOMEM;
        return 0;
    }

    if (!Deth02GetHostIP(&addr, host))
    {
        free(con);
        errno = ECONNREFUSED;
        return 0;
    }
    addr.sin_port = htons(port);

    con->fd = socket(addr.sin_family, SOCK_DGRAM, 0);
    con->ev = 0;
    if (con->fd == -1)
    {
        free(con);
        return 0;
    }

    if (pth_connect_ev(con->fd, (struct sockaddr *) &addr, sizeof (addr), con->ev) == -1)
    {
        int saveerr = errno;
        close(con->fd);
        free(con);
        errno = saveerr;
        return 0;
    }
    con->buflen = sizeof(con->buf);
    //con->buf = 0;
    con->readlen = 0;

    return con;
}

Deth02Connection *
Deth02SocketURL (const char *url)
{
    if (!url)
    {
        errno = EINVAL;
        return 0;
    }
    if (!strncmp (url, "ip:", 3))
    {
        char *a = strdup (url + 3);
        char *b;
        int port;
        Deth02Connection *c;
        if (!a)
        {
            errno = ENOMEM;
            return 0;
        }
        for (b = a; *b; b++)
            if (*b == ':')
                break;
        if (*b == ':')
        {
            *b = 0;
            port = atoi (b + 1);
        }
        else
            port = 17481;
        c = Deth02SocketRemote (a, port);
        free (a);
        return c;
    }
    errno = EINVAL;
    return 0;
}

void Deth02SetEvent(Deth02Connection * con, pth_event_t ev)
{
    con->ev = ev;
}

int
Deth02Complete(Deth02Connection * con)
{
    if (!con)
    {
        errno = EINVAL;
        return -1;
    }
    return con->complete (con);
}

/** send a request to eibd */
static int
Deth02SendFrame(Deth02Connection * con, unsigned int size, uchar * data)
{
    int i, start;

    if (size > 0xffff || size < 2)
    {
        errno = EINVAL;
        return -1;
    }

    start = 0;    
lp1:
    i = pth_write_ev (con->fd, data + start, size - start, con->ev);
    if (i == -1 && errno == EINTR)
        goto lp1;
    if (i == -1)
        return -1;
    if (i == 0)
    {
        errno = ECONNRESET;
        return -1;
    }
    start += i;
    if (start < size)
        goto lp1;
    return 0;
}

static int
Deth02CheckFrame(Deth02Connection * con)
{
    int i;
   
    i = pth_read_ev(con->fd, con->buf, con->buflen,
                    pth_event(PTH_EVENT_TIME, pth_timeout(30,0)));
    if (i == -1 && errno == EINTR)
        return 0;
    if (i == -1)
        return -1;
    if (i == 0)
    {
        errno = ECONNRESET;
        return -1;
    }
    con->readlen += i;

    return 0;
}

/** receive packet from Deth02 */
static int
Deth02GetFrame(Deth02Connection * con, int timeout)
{
    time_t start = time(0);

    con->readlen = 0;
 
    while (con->readlen < 1)
    {
        if (timeout > 0 && ((time(0) - start) >= timeout))
            return con->readlen;

        if (Deth02CheckFrame (con) == -1)
            return -1; 
    }

    return con->readlen;
}

static int
Deth02Login_complete (Deth02Connection * con)
{
    int i;
    i = Deth02GetFrame(con, 60);
    if (i == -1)
        return -1;

    hexdump(con->buf, con->readlen); //TODO

    if (strncmp ((const char *)con->buf, "INFO:Session opened:INFO", 24) != 0)
    {
        errno = ECONNRESET;
        return -1;
    }
    return 0;
}

int
Deth02Login_async(Deth02Connection * con, int write_only)
{
    uchar head[5];
    int i;
    if (!con)
    {
        errno = EINVAL;
        return -1;
    }
    
    i = Deth02SendFrame(con, 5, (unsigned char *)"LOGIN");
    if (i == -1)
        return -1;
    con->complete = Deth02Login_complete;
    return 0;
}

int
Deth02Login(Deth02Connection * con, int write_only)
{
    if (Deth02Login_async (con, write_only) == -1)
        return -1;
    return Deth02Complete (con);
}

static int
Deth02Logout_complete (Deth02Connection * con)
{
    int i;
    i = Deth02GetFrame(con, 30);
    if (i == -1)
        return -1;

    hexdump(con->buf, con->readlen); //TODO

    if (strncmp ((const char *)con->buf, "INFO:Session closed:INFO", 24) != 0)
    {
        errno = ECONNRESET;
        return -1;
    }
    return 0;
}

int
Deth02Logout_async(Deth02Connection * con, int write_only)
{
    int i;
    if (!con)
    {
        errno = EINVAL;
        return -1;
    }
    
    i = Deth02SendFrame(con, 6, (unsigned char *)"LOGOUT");
    if (i == -1)
        return -1;
    con->complete = Deth02Logout_complete;
    return 0;
}

int
Deth02Logout(Deth02Connection * con, int write_only)
{
    if (Deth02Logout_async (con, write_only) == -1)
        return -1;
    return Deth02Complete (con);
}

DomintellConnection::DomintellConnection() : con_m(0), isRunning_m(false), stop_m(0), listener_m(0), autodiscovery_m(false), lastRx_m(time(0)), lastTx_m(time(0)), kaInterval_m(0)
{}

DomintellConnection::~DomintellConnection()
{
    if (con_m)
        Deth02Close(con_m);
}

void DomintellConnection::importXml(ticpp::Element* pConfig)
{
    url_m = pConfig->GetAttribute("url");
    autodiscovery_m =
        (pConfig->GetAttributeOrDefault("autodiscovery", 
                                        "false") == "true");

    std::string interval = pConfig->GetAttribute("keepalive");
    if (interval != "")
    {
        std::istringstream val(interval);
        val >> kaInterval_m;
    }

    if (isRunning_m)
    {
        Stop();
        Start();
    }
}

void DomintellConnection::exportXml(ticpp::Element* pConfig)
{
    pConfig->SetAttribute("url", url_m);
    pConfig->SetAttribute("autodiscovery", autodiscovery_m ? "true" : "false");
    pConfig->SetAttribute("keepalive", kaInterval_m);
}

void DomintellConnection::addBusEventListener(BusEventListener *listener)
{
    if (listener_m)
        throw ticpp::Exception("DomintellConnection: BusEventListener already registered");
    listener_m = listener;
}

bool DomintellConnection::removeBusEventListener(BusEventListener *listener)
{
    if (listener_m != listener)
        return false;
    listener_m = 0;
    return true;
}

void DomintellConnection::stopConnection()
{
    if (isRunning_m)
        Deth02Logout(con_m, 0);
    isRunning_m = false;
    Stop();
};

void DomintellConnection::write(uint8_t* buf, int len)
{
    if (len == 0)
        return;
 
    logger_m.debugStream() << "write(buf=" << buf << ", len=" << len << ")" << endlog;

    PersistentStorage *persistence =
        Services::instance()->getPersistentStorage();
    if (persistence)
    {
        persistence->writelog("linknx", (const char *)buf);
    }

    if (con_m)
    {
        //TODO
	len = Deth02SendFrame(con_m, len, buf);
	if (len == -1)
        {
	    logger_m.errorStream() << "Write failed" << endlog;
            return;
        }
	logger_m.debugStream() << "Write request sent" << endlog;
        lastTx_m = time(0);
    }
}

void DomintellConnection::Run (pth_sem_t * stop1)
{
    if (url_m == "")
        return;
    stop_m = pth_event (PTH_EVENT_SEM, stop1);
    bool retry = true;
    while (retry)
    {
        con_m = Deth02SocketURL(url_m.c_str());
        if (con_m)
        {
            Deth02SetEvent(con_m, stop_m);
            if (Deth02Login(con_m, 0) != -1)
            {
                logger_m.infoStream() << "DomintellConnection: Login sent. Waiting for messages." << endlog;
                
                if (autodiscovery_m)
                {
                    bool done = false;
                    int i;

		    logger_m.infoStream() << "DomintellConnection: Starting autodiscovery."
                                          << endlog;
    
                    write((uint8_t* )"APPINFO", 7);

                    while (!done)
                    {
                        i = Deth02GetFrame(con_m, 0);
                        if (pth_event_status (stop_m) == PTH_STATUS_OCCURRED)
                            break;
                        
                        if (i > 0)
                        {
                            if (strncmp ((const char *)con_m->buf, "END APPINFO", 11) == 0)
                            {
                                done = true;
                                printf("\n\nDISCOVERY DONE\n\n");
                            }
                            else
                            {
                                std::string tmp;
                                std::string type;
                                std::string id;
                                std::string address;
                                std::string description;

                                if (!std::string((const char *)con_m->buf, con_m->readlen - 2).find('['))
                                    continue;

                                //IS8  3711-1Alarm Ingeschakeld[Huis||][PUSH=SHORT]

                                tmp.assign((const char *)con_m->buf, 3);
                                if (tmp == "BIR")
                                    type.assign("DBIR01");
                                else if (tmp == "IS8")
                                    type.assign("DISM08");
                                else if (tmp == "IS4")
                                    type.assign("DISM04");
                                else if (tmp == "DET")
                                    type.assign("DMOV01");
                                else if (tmp == "DIM")
                                    type.assign("DDIM01");
                                else if (tmp == "BU1")
                                    type.assign("DPBU01");
                                else if (tmp == "BU2")
                                    type.assign("DPBU02");
                                else if (tmp == "BU4")
                                    type.assign("DPBU04");
                                else if (tmp == "BU6")
                                    type.assign("DPBU06");
                                else if (tmp == "DMR")
                                    type.assign("DMR01");
                                else
                                {
                                    logger_m.errorStream() <<
                                        " discovery: " <<
                                        std::string((char *)con_m->buf, con_m->readlen - 2) <<
                                        endlog;
                                    continue;
                                }
                            
                                address.assign((const char *)(con_m->buf + 3), 8);
                                replace(address.begin(), address.end(), ' ', '0');
                                description.assign((const char *)(con_m->buf + 11));

                                id.assign(type);
                                id.append("-");
                                id.append(address);

                                if (ObjectController::instance()->objectExists(id.c_str()))
                                   continue;

                                Object *obj = Object::create(type);
                                if (obj)
                                {
                                    obj->setID(id.c_str());
                                    obj->setAddress(address.c_str());
                                    if (description.find('['))
                                        obj->setDescr(description.substr(0, description.find('[')).c_str());
                                    else
                                        obj->setDescr(description.c_str());

                                    //TODO: LOG + PERSIST 
                                    ObjectController::instance()->addObject(obj);
                                }
                                else
                                {
                                    logger_m.errorStream() <<
                                        " discovery: Failed to create object for => " <<
                                        std::string((char *)con_m->buf, con_m->readlen - 2) <<
                                        endlog;
                                }
                            }
                        }
                    }

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
                            logger_m.errorStream() << "Unable to write config to file: " << ex.m_details << endlog;
                            //throw "Error writing config to file";
                        }
                    }
                    else
                    {
                        logger_m.errorStream() << "Config file unknown" << endlog;
                    }
                }

                write((uint8_t* )"PING", 4);
                
                int retval;
                while ((retval = checkInput()) > 0)
                {
                    /*        TODO: find another way to check if event occured
                              struct timeval tv;
                              tv.tv_sec = 1;
                              tv.tv_usec = 0;
                              pth_select_ev(0,0,0,0,&tv,stop);
                    */
                }

                if (retval == -1)
                    retry = false;
                else
                    write((uint8_t* )"LOGOUT", 6);
            }
            else
                logger_m.errorStream() << "Failed to open socket." << endlog;

	    if (con_m)
	        Deth02Close(con_m);

            con_m = 0;
        }
        else
            logger_m.errorStream() << "Failed to open DomintellConnection url." << endlog;

        if (retry)
        {
            struct timeval tv;
            tv.tv_sec = 60;
            tv.tv_usec = 0;
            pth_select_ev(0,0,0,0,&tv,stop_m);
            if (pth_event_status (stop_m) == PTH_STATUS_OCCURRED)
                retry = false;
        }
    }
    logger_m.infoStream() << "Out of DomintellConnection loop." << endlog;
    pth_event_free (stop_m, PTH_FREE_THIS);
    stop_m = 0;
}

int DomintellConnection::checkInput()
{
    int len;
    PersistentStorage *persistence =
        Services::instance()->getPersistentStorage();
     
    if (!con_m)
    {
        logger_m.errorStream() << "No connection" << endlog;
        return 0;
    }
    
    len = Deth02GetFrame(con_m, 30);
    if (pth_event_status (stop_m) == PTH_STATUS_OCCURRED)
        return -1;
    if (len == -1)
    {
        logger_m.errorStream() << "Read failed" << endlog;
        return 0;
    }
    
    // If len is 0, a timeout occured
    if (len == 0)
    {
        goto handle_keepalive;
    }

    //if (logger_m.isDebugEnabled())
    //{
        //DbgStream dbg = logger_m.debugStream();
        //dbg << " packet received: "
        //    << std::string((char *)con_m->buf, con_m->readlen - 2) << endlog;
        //hexdump(con_m->buf, con_m->readlen);
    //}

    // Notify the objects, but strip off the <cr><lf>
    con_m->buf[con_m->readlen - 2] = 0;
    
    if (persistence)
    {
        persistence->writelog("DGQG01", (const char *)con_m->buf);
    }

    if (con_m->readlen - 2 == 14 &&
        con_m->buf[2] == ':' && con_m->buf[5] == ' ' &&
        con_m->buf[8] == '/' && con_m->buf[11] == '/')
    {
        goto handle_keepalive;
    }
    else if (con_m->readlen - 2 == 16 &&
             strncmp((const char *)con_m->buf, "INFO:World:INFO ", 16) == 0)
    {
        goto handle_keepalive;
    }
    else if (con_m->readlen - 2 == 24 &&
             strncmp((const char *)con_m->buf, "INFO:Session closed:INFO", 24) == 0)
    {
        return -1;
    }
    else
    {
        if (listener_m)
            listener_m->onBusEvent(con_m->buf, con_m->readlen - 2);
        else
            logger_m.errorStream() << "No listener!" << endlog;
    }

handle_keepalive:
    // Handle keepalive 
    if (kaInterval_m > 0 &&
        ((time(0) - lastTx_m) >= kaInterval_m))
    {
        write((uint8_t* )"HELLO", 5);
    }

    if (len > 0)
        lastRx_m = time(0);

    if ((time(0) - lastRx_m) > 90)
    {
        logger_m.errorStream() << "No time update received in 90 seconds -> restart connection" << endlog;
        return 0;
    }

    return 1;
}


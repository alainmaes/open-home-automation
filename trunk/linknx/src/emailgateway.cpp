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

#include "emailgateway.h"
#include <iostream>
#ifdef HAVE_LIBESMTP
#include <libesmtp.h>
#include <signal.h>
#elif defined HAVE_LIBCURL //OPEN_HOME_AUTOMATION
#include <curl/curl.h>
#endif

Logger& EmailGateway::logger_m(Logger::getInstance("EmailGateway"));

EmailGateway::EmailGateway() : type_m(Unknown)
{}

EmailGateway::~EmailGateway()
{}

void EmailGateway::importXml(ticpp::Element* pConfig)
{
    std::string type = pConfig->GetAttribute("type");
    if (type == "smtp")
    {
#ifndef HAVE_LIBCURL
 std::stringstream msg_1;
        msg_1 << "EmailGateway: libCuRL not available" << std::endl;
        throw ticpp::Exception(msg_1.str());
#endif

#if (defined HAVE_LIBESMTP || defined HAVE_LIBCURL) //OPEN_HOME_AUTOMATION
        type_m = SMTP;
        pConfig->GetAttribute("host", &host_m);
        pConfig->GetAttribute("from", &from_m);
        login_m = pConfig->GetAttribute("login");
        pass_m = pConfig->GetAttribute("pass");
#else
        std::stringstream msg;
        msg << "EmailGateway: Gateway type 'smtp' not supported, libesmtp not available" << std::endl;
        throw ticpp::Exception(msg.str());
#endif
    }
    else if (type == "")
    {
        type_m = Unknown;
        host_m.clear();
        from_m.clear();
        login_m.clear();
        pass_m.clear();
    }
    else
    {
        std::stringstream msg;
        msg << "EmailGateway: Bad gateway type: '" << type << "'" << std::endl;
        throw ticpp::Exception(msg.str());
    }
}

void EmailGateway::exportXml(ticpp::Element* pConfig)
{
    if (type_m == SMTP)
    {
        pConfig->SetAttribute("type", "smtp");
        pConfig->SetAttribute("host", host_m);
        pConfig->SetAttribute("from", from_m);
        if (login_m != "")
            pConfig->SetAttribute("login", login_m);
        if (pass_m != "")
            pConfig->SetAttribute("pass", pass_m);
    }
}

const char *EmailGateway::callback(void **buf, int *len, void *arg)
{
    MessageBody* body = static_cast<MessageBody*>(arg);
    const char *tmp = body->getData(len);
    logger_m.infoStream() << "EmailGateway: callback " << (tmp ? tmp : "") << " len=" << len << endlog;
    return tmp;
}

#ifdef HAVE_LIBESMTP
int EmailGateway::authCallback(auth_client_request_t request, char **result, int fields, void *arg)
{
    int i;
    EmailGateway* gw = static_cast<EmailGateway*>(arg);

    for (i = 0; i < fields; i++)
    {
        if (request[i].flags & AUTH_PASS)
            result[i] = const_cast<char*>(gw->pass_m.c_str());
        else
            result[i] = const_cast<char*>(gw->login_m.c_str());
    }
    return 1;
}
#endif

MessageBody::MessageBody(std::string& text) : status_m(0)
{
    std::stringstream msg;
    size_t pos, start = 0;
    while ((pos = text.find('\n', start)) != std::string::npos)
    {
        if (pos == 0)
            msg << "\r\n";
        else if (text[pos-1] == '\r')
            msg << text.substr(start, pos-start-1) << "\r\n";
        else
            msg << text.substr(start, pos-start) << "\r\n";
        start = pos+1;
    }
    msg << text.substr(start) << "\r\n";
    text_m = msg.str();
}

const char *MessageBody::getData(int *len)
{
    if (len == NULL)
    {
        status_m = 0;
        return NULL;
    }

    if (status_m == 0)
    {
        const char* hdr = "MIME-Version: 1.0\r\n"
                          "Content-Type: text/plain; charset=ISO-8859-1; format=flowed\r\n"
                          "Content-Transfer-Encoding: 8bit\r\n"
                          "\r\n";
        status_m++;
        *len = strlen(hdr);
        return hdr;
    }
    if (status_m == 1)
    {
        status_m++;
        *len = text_m.size();
        return text_m.data();
    }
    return NULL;
}

#ifdef HAVE_LIBCURL //OPEN_HOME_AUTOMATION
struct upload_status {
  const char *from;
  const char *to;
  const char *subject;
  const char *text;
  int lines_read;
};
 
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
{
    struct upload_status *upload_ctx = (struct upload_status *)userp;
    size_t len;

    if ((size == 0) || (nmemb == 0) || ((size*nmemb) < 1))
    {
        return 0;
    }

    if (upload_ctx->lines_read == 0)
    {
        len = sprintf((char *)ptr, "To: %s\n", upload_ctx->to);
        upload_ctx->lines_read ++;
        return len;
    }
    else if (upload_ctx->lines_read == 1)
    {
        len = sprintf((char *)ptr, "From: %s\n", upload_ctx->from);
        upload_ctx->lines_read ++;
        return len;
    }
    else if (upload_ctx->lines_read == 2)
    {
        len = sprintf((char *)ptr, "Subject: %s\n", upload_ctx->subject);
        upload_ctx->lines_read ++;
        return len;
    }
    else if (upload_ctx->lines_read == 3)
    {
        /* empty line to divide headers from body, see RFC5322 */
        len = sprintf((char *)ptr, "\n", upload_ctx->from);
        upload_ctx->lines_read ++;
        return len;
    }
    else if (upload_ctx->lines_read == 4)
    {
        len = sprintf((char *)ptr, "%s\n", upload_ctx->text);
        upload_ctx->lines_read ++;
        return len;
    }
    else
    {
        return 0;
    }
}
#endif

void EmailGateway::sendEmail(std::string &to, std::string &subject, std::string &text)
{
    if (type_m == SMTP)
    {
#ifdef HAVE_LIBESMTP
        smtp_session_t session;
        smtp_message_t message;
        smtp_recipient_t recipient;
        auth_context_t authctx;
        const smtp_status_t *status;
        struct sigaction sa;
        sa.sa_handler = SIG_IGN;
        sigemptyset (&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction (SIGPIPE, &sa, NULL);

        MessageBody body(text);

        session = smtp_create_session ();
        message = smtp_add_message (session);

        /* Set the host running the SMTP server.  LibESMTP has a default port
           number of 587, however this is not widely deployed so the port
           is specified as 25 along with the default MTA host. */
        smtp_set_server (session, host_m.c_str());

        authctx = auth_create_context ();
        // auth_set_mechanism_flags (authctx, AUTH_PLUGIN_EXTERNAL, 0);
        auth_set_interact_cb (authctx, EmailGateway::authCallback, this);
        
        //  smtp_set_eventcb(session, event_cb, NULL);
        
        smtp_auth_set_context (session, authctx);

        /* Set the reverse path for the mail envelope.  (NULL is ok)
         */
        smtp_set_reverse_path (message, from_m.c_str());

        /* RFC 2822 doesn't require recipient headers but a To: header would
           be nice to have if not present. */
        smtp_set_header (message, "To", NULL, NULL);

        /* Set the Subject: header.  For no reason, we want the supplied subject
           to override any subject line in the message headers. */
        if (subject != "")
        {
            smtp_set_header (message, "Subject", subject.c_str());
            smtp_set_header_option (message, "Subject", Hdr_OVERRIDE, 1);
        }

        /* Open the message file and set the callback to read it.
         */
        smtp_set_messagecb(message, EmailGateway::callback, &body);

        /* Add remaining program arguments as message recipients.
         */
        recipient = smtp_add_recipient (message, to.c_str());

        /* Initiate a connection to the SMTP server and transfer the
           message. */
        if (!smtp_start_session (session))
        {
            char buf[128];
            logger_m.errorStream() << "EmailGateway: SMTP server problem "
            << smtp_strerror (smtp_errno (), buf, sizeof buf) << endlog;
        }
        else
        {
            /* Report on the success or otherwise of the mail transfer.
             */
            status = smtp_message_transfer_status (message);
            logger_m.infoStream() << "EmailGateway: Done " << status->code
            << " => " << (status->text? status->text : "") << endlog;
        }

        /* Free resources consumed by the program.
         */
        smtp_destroy_session (session);
        auth_destroy_context (authctx);
        auth_client_exit ();
#elif defined HAVE_LIBCURL
        //OPEN_HOME_AUTOMATION
        CURL                 *curl;
        CURLcode              res;
        struct curl_slist    *recipients = NULL;
        struct upload_status  upload_ctx;
 
        upload_ctx.lines_read = 0;
        upload_ctx.to = to.c_str();
        upload_ctx.from = from_m.c_str();
        upload_ctx.subject = subject.c_str();
        upload_ctx.text = text.c_str();

        curl = curl_easy_init();
        if (curl)
        {
            //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

            /* this is the URL for your mailserver - you can also use an smtps:// URL
             * here */ 
            curl_easy_setopt(curl, CURLOPT_URL, host_m.c_str());
            //TODO check for '/' in server name
 
            /* Note that this option isn't strictly required, omitting it will result in
             * libcurl will sent the MAIL FROM command with no sender data. All
             * autoresponses should have an empty reverse-path, and should be directed
             * to the address in the reverse-path which triggered them. Otherwise, they
             * could cause an endless loop. See RFC 5321 Section 4.5.5 for more details.
             */ 
            curl_easy_setopt(curl, CURLOPT_MAIL_FROM, from_m.c_str());
 
            /* Note that the CURLOPT_MAIL_RCPT takes a list, not a char array.  */ 
            recipients = curl_slist_append(recipients, to.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
 
            /* You provide the payload (headers and the body of the message) as the
             * "data" element. There are two choices, either:
             * - provide a callback function and specify the function name using the
             * CURLOPT_READFUNCTION option; or
             * - just provide a FILE pointer that can be used to read the data from.
             * The easiest case is just to read from standard input, (which is available
             * as a FILE pointer) as shown here.
             */ 
	    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
            curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
 
	    /* send the message (including headers) */ 
	    res = curl_easy_perform(curl);
            /* Check for errors */ 
            if (res != CURLE_OK)
                logger_m.errorStream() << "EmailGateway: CuRL SMTP failed: " << curl_easy_strerror(res) << endlog;
 
            /* free the list of recipients */ 
            curl_slist_free_all(recipients);
 
            /* curl won't send the QUIT command until you call cleanup, so you should be
             * able to re-use this connection for additional messages (setting
             * CURLOPT_MAIL_FROM and CURLOPT_MAIL_RCPT as required, and calling
             * curl_easy_perform() again. It may not be a good idea to keep the
             * connection open for a very long time though (more than a few minutes may
             * result in the server timing out the connection), and you do want to clean
             * up in the end.
             */ 
             curl_easy_cleanup(curl);
        }
        else
        {
            logger_m.errorStream() << "EmailGateway: Failed to initialize CuRL" << endlog;
        }
#endif
    }
    else
        logger_m.errorStream() << "EmailGateway: Unable to send Email, gateway not set." << endlog;
}

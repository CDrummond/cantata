/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#include "httprequesthandler.h"
#include "httprequest.h"
#include "httpresponse.h"
#include "httpconnection.h"
#include "staticfilecontroller.h"

HttpRequestHandler::HttpRequestHandler(HttpConnection *parent)
    : QObject(parent)
    , m_connection(parent)
{
}

void HttpRequestHandler::service(HttpRequest *request, HttpResponse *response)
{
    StaticFileController(m_connection).service(request, response);
}

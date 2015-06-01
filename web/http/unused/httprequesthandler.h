/**
 * @file
 *
 * @author Stefan Frings
 * @author Petr Bravenec petr.bravenec@hobrasoft.cz
 */

#ifndef _HttpRequestHandler_H_
#define _HttpRequestHandler_H_

#include <QObject>

class HttpRequest;
class HttpResponse;
class HttpConnection;

/**
@brief Processes incoming requests

When building your own http server, you should reimplement this class.

You need to reimplement the service() method, for example:

@code
void RequestMapper::service(HttpRequest *request, HttpResponse *response) {
    QString path = request->path();

    #define ROUTER(address, hclass) \
        if (path.startsWith(address)) { \
            HttpRequestHandler *controller = new hclass (connection()); \
            controller->service(request, response); \
            return; \
            }

    ROUTER("/translations",     ControllerTranslations);
    ROUTER("/errorevents",      ControllerErrorEvents);
    ROUTER("/callqueue",        ControllerCallQueue);

    HttpRequestHandler::service(request, response);
    response->flush();
}
@endcode

Controller classes in the example are derived from HttpRequstHandler, too. 

### How the event streams should be implemented ###
When implementing event streams you should make a slot to receive information about changed status.
Create new HttpResponse in the slot and write your data to it:

@code
MyOwnController::MyOwnController(HttpCOnnection *parent) : HttpRequestHandler(parent) {
    connect(myclass, SIGNAL( statusChanged(MyClass *)),
            this,      SLOT(slotSendUpdate(MyClass *)));
}

void MyOwnController::slotSendUpdate(MyClass *object) {
    HttpResponse response = HttpRequestHandler::response();
    resopnse.setHeader("Transfer-Encoding", "chunked");
    response.setHeader("Content-Type",      "text/event-stream");
    response.setHeader("Cache-Control",     "no-cache,public");
    response.setSendHeaders(false);     // suppress the header to be send

    QByteArray datagram;
    datagram += "event: status\n";
    datagram += "data: ";
    datagram += object->status();       // method returns text formated status of the object
    datagram += "\n";
    datagram += "\n";
    response.write(datagram);
    response.flush();
}
@endcode

If you want to use simplier way to make your own event streams, use the class AbstractController from example.
AbstractController class implements simple json API to get/put/delete objects, lists, event streams.

*/
class HttpRequestHandler : public QObject
{
    Q_OBJECT
public:

    /**
     * @brief Konstruktor
     */
    HttpRequestHandler(HttpConnection *parent);

    /**
     * @brief Request processing. Should be reimplemented in derived class
     *
     * This method should be reimplemeted in derived class. Default implementations
     * can handle static content and shtml files only.
     *
     */
    virtual void service(HttpRequest *request, HttpResponse *response);


protected:
    /**
     * @brief Returns pointer to parent HttpConnection class
     */
    HttpConnection *connection() { return m_connection; }

private:

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    HttpConnection *m_connection;
#endif
};

#endif

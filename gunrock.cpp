#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 1;
int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

deque<MySocket *> BUFFER;
pthread_mutex_t LOCK = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ROOM_AVAILABLE = PTHREAD_COND_INITIALIZER;
pthread_cond_t CLIENT_AVAILABLE = PTHREAD_COND_INITIALIZER;

vector<HttpService *> services;

HttpService *find_service(HTTPRequest *request) {
    // find a service that is registered for this path prefix
    for (unsigned int idx = 0; idx < services.size(); idx++) {
        if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
            return services[idx];
        }
    }

    return NULL;
}


void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
    stringstream payload;

    // invoke the service if we found one
    if (service == NULL) {
        // not found status
        response->setStatus(404);
    } else if (request->isHead()) {
        payload << "HEAD " << request->getPath();
        sync_print("invoke_service_method", payload.str());
        cout << payload.str() << endl;
        service->head(request, response);
    } else if (request->isGet()) {
        payload << "GET " << request->getPath();
        sync_print("invoke_service_method", payload.str());
        cout << payload.str() << endl;
        service->get(request, response);
    } else {
        // not implemented status
        response->setStatus(405);
    }
}

void handle_request(MySocket *client) {
    HTTPRequest *request = new HTTPRequest(client, PORT);
    HTTPResponse *response = new HTTPResponse();
    stringstream payload;

    // read in the request
    bool readResult = false;
    try {
        payload << "client: " << (void *) client;
        sync_print("read_request_enter", payload.str());
        readResult = request->readRequest();
        sync_print("read_request_return", payload.str());
    } catch (...) {
        // swallow it
    }

    if (!readResult) {
        // there was a problem reading in the request, bail
        delete response;
        delete request;
        sync_print("read_request_error", payload.str());
        return;
    }

    HttpService *service = find_service(request);
    invoke_service_method(service, request, response);

    // send data back to the client and clean up
    payload.str("");
    payload.clear();
    payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;
    sync_print("write_response", payload.str());
    cout << payload.str() << endl;
    client->write(response->response());

    delete response;
    delete request;

    payload.str("");
    payload.clear();
    payload << " client: " << (void *) client;
    sync_print("close_connection", payload.str());
    client->close();
    delete client;
}

void *runWorkerThread(void *id) {
    long tid = (long) id;
    MySocket *client;
    while (true) {
        dthread_mutex_lock(&LOCK);
        while (BUFFER.empty()) {
            cout << "WORKER waiting for request, id: " << tid << endl;
            dthread_cond_wait(&CLIENT_AVAILABLE, &LOCK);
        }

        client = BUFFER.front();
        BUFFER.pop_front();
        dthread_cond_signal(&ROOM_AVAILABLE);
        dthread_mutex_unlock(&LOCK);
        cout << "WORKER handling request" << endl;
        handle_request(client);
    }


}

void *runMasterThread() {
    // create a fixed-size pool of worker threads
    pthread_t workerThreads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        int ret = dthread_create(&workerThreads[i], NULL, runWorkerThread, reinterpret_cast<void *>(i));

        if (ret) {
            cout << "Error:unable to create worker thread," << ret << endl;
            exit(-1);
        }

        dthread_detach(workerThreads[i]);
    }


    // TODO: master thread accepting new HTTP connections over the network and placing the descriptor for this connection into a fixed-size buffer
    MyServerSocket *server = new MyServerSocket(PORT);
    MySocket *client;
    while (true) {
        sync_print("waiting_to_accept", "");
        client = server->accept();
        sync_print("client_accepted", "");

        dthread_mutex_lock(&LOCK);

        while ((int) BUFFER.size() == BUFFER_SIZE) {
            cout << "MASTER waiting for room" << endl;
            dthread_cond_wait(&ROOM_AVAILABLE, &LOCK);
        }
        BUFFER.push_back(client);
        dthread_cond_signal(&CLIENT_AVAILABLE);
        dthread_mutex_unlock(&LOCK);
    }
}


int main(int argc, char *argv[]) {

    signal(SIGPIPE, SIG_IGN);
    int option;

    while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
        switch (option) {
            case 'd':
                BASEDIR = string(optarg);
                break;
            case 'p':
                PORT = atoi(optarg);
                break;
            case 't':
                THREAD_POOL_SIZE = atoi(optarg);
                break;
            case 'b':
                BUFFER_SIZE = atoi(optarg);
                break;
            case 's':
                SCHEDALG = string(optarg);
                break;
            case 'l':
                LOGFILE = string(optarg);
                break;
            default:
                cerr << "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
                exit(1);
        }
    }

    set_log_file(LOGFILE);

    sync_print("init", "");

    services.push_back(new FileService(BASEDIR));



    pthread_t masterThread[1];
    int ret = dthread_create(&masterThread[0], NULL, reinterpret_cast<void *(*)(void *)>(runMasterThread), NULL);

    if (ret) {
        cout << "Error:unable to create master thread," << ret << endl;
        exit(-1);
    }

    runMasterThread();
}
#include <iostream>

#include <assert.h>

#include "HttpClient.h"

using namespace std;

int PORT = 8080;

int main(int argc, char *argv[]) {
    cout << "Making http request" << endl;

    HttpClient client("localhost", PORT);
    HttpClient client2("localhost", PORT);
    HttpClient client3("localhost", PORT);
    HttpClient client4("localhost", PORT);
    HttpClient client5("localhost", PORT);
    HttpClient client6("localhost", PORT);
    HttpClient client7("localhost", PORT);

    HTTPResponse *response = client.get("/hello_world.html");
    HTTPResponse *response2 = client2.get("/hello_world.html");
    HTTPResponse *response3 = client3.get("/hello_world.html");
    HTTPResponse *response4 = client4.get("/hello_world.html");
    HTTPResponse *response5 = client5.get("/hello_world.html");
    HTTPResponse *response6 = client6.get("/hello_world.html");
    HTTPResponse *response7 = client7.get("/hello_world.html");

    assert(response->status() == 200);
    assert(response2->status() == 200);
    assert(response3->status() == 200);
    assert(response4->status() == 200);
    assert(response5->status() == 200);
    assert(response6->status() == 200);
    assert(response7->status() == 200);


    delete response;

    cout << "passed" << endl;

    return 0;
}

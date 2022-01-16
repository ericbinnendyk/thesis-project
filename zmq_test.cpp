#include <string>
#include <chrono>
#include <thread>
#include <iostream>

#include <zmq.hpp>

int main() 
{
    using namespace std::chrono_literals;

    // initialize the zmq context with a single IO thread
    zmq::context_t context{1};

    // construct a REP (reply) socket and bind to interface
    zmq::socket_t socket{context, zmq::socket_type::rep};
    socket.bind("tcp://*:5555");

    // prepare some static data for responses
    const std::string data{"Received."};

    uint32_t typeIs = 0, typeIIs = 0;

    for (;;) 
    {
        zmq::message_t request;

        // receive a request from client
        /*socket.recv(request, zmq::recv_flags::none);
        if (request.to_string() == "/typeI") {
            typeIs += 1;
            std::cout << "Received " << typeIs << "th typeI" << std::endl;
        }
        if (request.to_string() == "/typeII") {
            typeIIs += 1;
            std::cout << "Received " << typeIIs << "th typeII" << std::endl;
        }*/

        // simulate work
        //std::this_thread::sleep_for(1s);

        socket.recv(request, zmq::recv_flags::none);
        // I need to use (char *) to convert to a string. Using to_string() instead doesn't seem to include the \0 at the end
        std::cout << (char *) request.data() << std::endl;
        //std::cout << request.to_string() << std::endl;
        // send back pointless message
        socket.send(zmq::buffer(data), zmq::send_flags::none);
    }

    return 0;
}
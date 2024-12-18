#include "broker.hpp"
#include <iostream>
#include <zmq.hpp>

Broker::Broker()
    : context(1), frontend(context, ZMQ_ROUTER), backend(context, ZMQ_ROUTER) {
    frontend.set(zmq::sockopt::linger, 0); // Evita bloqueos al cerrar
    backend.set(zmq::sockopt::linger, 0); 
    frontend.set(zmq::sockopt::rcvtimeo, 5000); // Timeout de 5 segundos para recv
    backend.set(zmq::sockopt::rcvtimeo, 5000);  
    frontend.bind("tcp://*:5555");
    backend.bind("tcp://*:5556");
}

void Broker::start() {
    zmq::pollitem_t items[] = {
        { static_cast<void*>(frontend), 0, ZMQ_POLLIN, 0 },
        { static_cast<void*>(backend), 0, ZMQ_POLLIN, 0 }
    };

    while (true) {
        zmq::poll(items, 2, std::chrono::milliseconds(-1));

        // Handle client requests
        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t client_id, request;
            auto res = frontend.recv(client_id, zmq::recv_flags::none);
            if (!res) {
                // Manejo del error
                std::cerr << "Error: Fallo al recibir el mensaje en frontend" << std::endl;
                // Opcional: lanzar una excepción o tomar alguna acción correctiva.
                return;
            }

            frontend.recv(request);
            if (!workers.empty()) {
                auto it = workers.begin();
                zmq::message_t msg(it->first.data(), it->first.size());
                backend.send(msg, zmq::send_flags::sndmore);
                backend.send(client_id, zmq::send_flags::sndmore);
                backend.send(request, zmq::send_flags::none);
                workers.erase(it);
            }
        }

        // Handle worker responses
        if (items[1].revents & ZMQ_POLLIN) {
            zmq::message_t worker_id, client_id, response;
            backend.recv(worker_id);
            backend.recv(client_id);
            backend.recv(response);
            frontend.send(client_id, zmq::send_flags::sndmore);
            frontend.send(response, zmq::send_flags::none);
            workers.emplace(worker_id.to_string(), 0);
        }
    }
}

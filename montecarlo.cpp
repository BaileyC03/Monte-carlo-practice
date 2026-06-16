#include "montecarlo.hpp"
#include <memory>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>
#include <ctime>
#include <App.h>

struct PerSocketData {};

int main(){
    auto portfolio = std::make_unique<Portfolio>();
    portfolio->loadTestCase();
    portfolio->computeCholeskyFactor();
    portfolio->verifyCholesky();

    uWS::App app;
    auto* loop = uWS::Loop::get();

    std::thread worker([&]() {
        monteCarloEngine<double> engine;

        while (true) {
            auto start = std::chrono::high_resolution_clock::now();
            auto res = engine.run(*portfolio, 500000);
            auto end = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "Time: " << ms << "ms | VaR: " << res.VaR << "\n";

            std::ostringstream json;
            json << "{\"var95\":" << res.VaR
                 << ",\"cvar95\":" << res.CVaR
                 << ",\"mean\":" << res.mean
                 << ",\"timestamp\":" << std::time(nullptr) << "}";

            std::string msg = json.str();
            loop->defer([&app, msg]() {
                app.publish("results", msg, uWS::OpCode::TEXT);
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    app.ws<PerSocketData>("/ws", {
        .open = [](auto* ws) {
            ws->subscribe("results");
            std::cout << "Client connected\n";
        },
        .message = [](auto* ws, std::string_view msg, uWS::OpCode opCode) {},
        .close = [](auto* ws, int code, std::string_view msg) {
            std::cout << "Client disconnected\n";
        }
    }).listen(9001, [](auto* listenSocket) {
        if (listenSocket) std::cout << "Listening on port 9001\n";
        else std::cout << "Failed to listen on port 9001\n";
    }).run();

    worker.join();
    return 0;
}

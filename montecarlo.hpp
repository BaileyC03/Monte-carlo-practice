#pragma once 
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <random>
#include <thread>
#include "third_party/json.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>

using json = nlohmann::json;

class stock
{
public:
    std::string name;
    double stockPrice;
    double mu;
};

class Portfolio
{
public:
    std::vector<stock> stocks;
    std::vector<double> weights;
    std::vector<std::vector<double>> covariance;
    std::vector<std::vector<double>> choleskyFactor;
    double totalValue;

    void computeCholeskyFactor()
    {
        int n = stocks.size();
        choleskyFactor.assign(n, std::vector<double>(n, 0.0));

        for (int i = 0; i < (int)choleskyFactor.size(); i++)
        {
            for (int j = 0; j <= i; j++)
            {
                double sum = 0.0;
                for (int k = 0; k < j; k++)
                {
                    sum = sum + (choleskyFactor[i][k] * choleskyFactor[j][k]);
                }
                if (i == j)
                {
                    double val = covariance[i][i] - sum;
                    if (val <= 0.0)
                        throw std::runtime_error("Something fucked up");
                    choleskyFactor[i][j] = std::sqrt(val);
                }
                else
                {
                    choleskyFactor[i][j] = (covariance[i][j] - sum) / choleskyFactor[j][j];
                }
            }
        }
    };

    bool verifyCholesky()
    {
        int n = stocks.size();
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < n; j++)
            {
                double sum = 0.0;
                for (int k = 0; k < n; k++)
                {
                    sum += choleskyFactor[i][k] * choleskyFactor[j][k];
                }
                if (std::abs(sum - covariance[i][j]) > 1e-9)
                {
                    std::cout << "Mismatch at [" << i << "][" << j << "]: "
                              << "got " << sum << " expected " << covariance[i][j] << "\n";
                    return false;
                }
            }
        }
        std::cout << "Cholesky verification passed!\n";
        return true;
    }

    void loadJSON() {
       std::ifstream input_file("data.json");
       json j;
       input_file >> j;

       for (const auto& stockJson : j["stocks"]) {
           stock s;
           s.name = stockJson["name"];
           s.stockPrice = stockJson["stockPrice"];
           s.mu = stockJson["mu"];
           stocks.push_back(s);
       }

       covariance = j["covariance"].get<std::vector<std::vector<double>>>();
       weights = j["weights"].get<std::vector<double>>();
       totalValue = j["totalValue"];
    }
};

struct results {
    std::vector<double> PnL;
    double VaR;
    double CVaR;
    double mean;
};

template <typename T>
class monteCarloEngine {
public:
    results run(Portfolio& portfolio, int simulationNumber) {
        results res;
        res.PnL.resize(simulationNumber);

        int numThreads = 8; //pick number of processes you want to use
        int sliceSize = simulationNumber / numThreads; //Slicing the workload between them
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; i++) {
            std::cout << "Starting thread " << i << "\n";
            threads.push_back(std::thread(&monteCarloEngine::simulateBatch, //creating the vector of threads
                this, i * sliceSize, (i + 1) * sliceSize, std::ref(res.PnL), std::ref(portfolio), i, rand()));
                // start, end, output, portfolio, thread index, seed
        }

        for (auto& thread : threads) {
            thread.join();
        }

        std::sort(res.PnL.begin(), res.PnL.end());

        int varIndex = static_cast<int>(0.05 * simulationNumber);
        res.VaR = res.PnL[varIndex];

        double cvarSum = 0.0;
        for (int i = 0; i < varIndex; i++) {
            cvarSum += res.PnL[i];
        }
        res.CVaR = cvarSum / varIndex;

        double total = 0.0;
        for (double v : res.PnL) total += v;
        res.mean = total / simulationNumber;

        return res;
    }

    void simulateBatch(int start, int end, std::vector<double>& pnlOut, Portfolio& portfolio, int index, int seed) {
        int n = portfolio.stocks.size();
        double dt = 1.0 / 252.0;
        std::mt19937 gen(seed + index);

        for (int i = start; i < end; i++) {
            std::vector<T> z(n);
            for (int j = 0; j < n; j++) {
                z[j] = std::normal_distribution<T>{}(gen);
            }

            std::vector<T> eps(n, 0.0);
            for (int j = 0; j < n; j++) {
                eps[j] = 0.0;
                for (int k = 0; k < n; k++) {
                    eps[j] += portfolio.choleskyFactor[j][k] * z[k];
                }
            }

            double pnl = 0.0;
            for (int j = 0; j < n; j++) {
                double sigma = std::sqrt(portfolio.covariance[j][j]);
                double oldPrice = portfolio.stocks[j].stockPrice;
                double newPrice = oldPrice * std::exp(
                    (portfolio.stocks[j].mu - 0.5 * sigma * sigma) * dt +
                    sigma * std::sqrt(dt) * eps[j]
                );
                pnl += (newPrice - oldPrice) * portfolio.weights[j] * portfolio.totalValue;
            }

            pnlOut[i] = pnl;
        }
    }
};
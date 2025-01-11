#include "serial.hpp"
#include <iostream>
#include <string>
#include <fstream>
#include <numeric>
#include <chrono>
#include <vector>
#include <iomanip> 
#include <ctime>
#include <sstream>
#include <regex>

#if !defined(WIN32)
    #include <unistd.h>
    #include <time.h>
#endif


const std::string logFileHour = "hour.log";
const std::string logFileDay = "day.log";
const std::string logFileAll = "all.log"; 

using namespace std::chrono_literals;

struct Log {
    std::chrono::system_clock::time_point timestamp;
    std::string data;
};

Log parseLog(const std::string &line) {
    std::istringstream iss(line);
    std::tm tm = {};

    char delim;
    iss >> std::get_time(&tm, "[%d.%m.%Y %H:%M:%S]") >> delim;

    std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));

    std::string data;
    std::getline(iss >> std::ws, data);
    data = std::regex_replace(data, std::regex("^\\s+|\\s+$"), "");

    return {timestamp, data};
}

void clearLogFile(const std::string &fileName, const std::chrono::hours &duration) {
    std::ifstream inFile(fileName);
    std::vector<Log> logs;
    std::string line;

    while (std::getline(inFile, line)) {
        Log log = parseLog(line);
        if (std::chrono::system_clock::now() - log.timestamp <= duration)
            logs.push_back(log);
    }

    inFile.close();

    std::ofstream logFile(fileName, std::ios_base::trunc);
    if (logFile.is_open()) {
        for (const auto &log: logs) {
            std::time_t t = std::chrono::system_clock::to_time_t(log.timestamp);
            std::tm *tmNow = std::localtime(&t);
            logFile << std::put_time(tmNow, "[%d.%m.%Y %H:%M:%S]") << " - " << log.data << std::endl;
        }
    } 
}

void logAll(const std::string &message) {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const std::tm *tmNow = std::localtime(&now);
    
    std::stringstream ss;
    ss << std::put_time(tmNow, "[%d.%m.%Y %H:%M:%S]") << " - " << message;
    std::ofstream logFile(logFileAll, std::ios_base::app);
    if (logFile.is_open())
        logFile << ss.str() << std::endl;
}

void logAverage(const std::string &logFileName, const std::vector<float> &temperatures) {
    if (!temperatures.empty()) {
        const float sum = std::accumulate(temperatures.begin(), temperatures.end(), 0.0f);
        const float average = sum / static_cast<float>(temperatures.size());

        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const std::tm *tmNow = std::localtime(&now);

        std::stringstream ss;
        ss << std::put_time(tmNow, "[%d.%m.%Y %H:%M:%S]") << " - " << average;
        std::ofstream logFile(logFileName, std::ios_base::app);
        if (logFile.is_open())
            logFile << ss.str() << std::endl;
        }
}

int main(int argc, char **argv) {

    if (argc < 2) {
        std::cout << "Usage: main [port]" << std::endl;
        return -1;
    }

    cplib::SerialPort smport(std::string(argv[1]), cplib::SerialPort::BAUDRATE_115200);
    if (!smport.IsOpen()) {
        std::cout << "Failed to open port '" << argv[1] << "'! Terminating..." << std::endl;
        return -2;
    }

    std::vector<float> hourlyTemperatures;
    std::vector<float> dailyTemperatures;
    std::string message{};

    smport.SetTimeout(1.0);

    auto start = std::chrono::system_clock::now();

    while (true) {
        smport >> message;
        if (message.empty())
            continue;

        std::cout << message << std::endl;
        logAll(message);

        hourlyTemperatures.push_back(std::stof(message));
        dailyTemperatures.push_back(std::stof(message));

        auto now = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = now - start;
        if (elapsed.count() >= (60 * 60) && static_cast<int>(elapsed.count()) % (60 * 60) == 0) {
            logAverage(logFileHour, hourlyTemperatures);
        }

        if (elapsed.count() >= 24 * 60 * 60) {
            logAverage(logFileDay, dailyTemperatures);
            start = now;
        }

        clearLogFile(logFileAll, std::chrono::hours(24));
        clearLogFile(logFileHour, std::chrono::hours(24 * 30));
        clearLogFile(logFileDay, std::chrono::hours(24 * 365));
    }

    smport.Close();
    return 0;
}
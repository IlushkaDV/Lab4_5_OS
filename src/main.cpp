#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "../include/circular_buffer.h"
#include "../include/database.h"
#include "httplib.h"

const char* DB_FILE = "temperature.db";
const int HTTP_PORT = 8080;
const char* WEB_DIR = "../web";

Database* db;
CircularBuffer raw_buffer(24 * 3600);
CircularBuffer hourly_buffer(3600);
CircularBuffer daily_buffer(24 * 3600);

time_t last_hour = 0;
time_t last_day = 0;

std::string get_timestamp(time_t t = time(nullptr)) {
    std::tm tm;
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

bool setup_serial(int fd, int baudrate) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "Ошибка tcgetattr" << std::endl;
        return false;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    tty.c_cc[VTIME] = 1;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "Ошибка tcsetattr" << std::endl;
        return false;
    }

    return true;
}

void calculate_and_save_hourly() {
    if (hourly_buffer.size() == 0) return;
    
    double sum = 0.0, min_temp = 1000.0, max_temp = -1000.0;
    size_t count = hourly_buffer.size();
    
    std::vector<TemperatureRecord> records = hourly_buffer.get_all();
    for (const auto& r : records) {
        sum += r.temperature;
        min_temp = std::min(min_temp, r.temperature);
        max_temp = std::max(max_temp, r.temperature);
    }
    
    double avg = sum / count;
    db->insert_hourly(avg, min_temp, max_temp, count);
    
    std::cout << "[" << get_timestamp() << "]  Часовая статистика: avg=" << avg 
              << "°C, min=" << min_temp << "°C, max=" << max_temp << "°C (" << count << " изм.)" << std::endl;
}

void calculate_and_save_daily() {
    if (daily_buffer.size() == 0) return;
    
    double sum = 0.0, min_temp = 1000.0, max_temp = -1000.0;
    size_t count = daily_buffer.size();
    
    std::vector<TemperatureRecord> records = daily_buffer.get_all();
    for (const auto& r : records) {
        sum += r.temperature;
        min_temp = std::min(min_temp, r.temperature);
        max_temp = std::max(max_temp, r.temperature);
    }
    
    double avg = sum / count;
    db->insert_daily(avg, min_temp, max_temp, count);
    
    std::cout << "[" << get_timestamp() << "]  Дневная статистика: avg=" << avg 
              << "°C, min=" << min_temp << "°C, max=" << max_temp << "°C (" << count << " изм.)" << std::endl;
}

void http_server_thread() {
    httplib::Server svr;

    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}});

    svr.Get("/api/current", [](const httplib::Request&, httplib::Response& res) {
        double temp = db->get_current_temperature();
        std::ostringstream json;
        json << "{\"temperature\":" << temp << ",\"timestamp\":" << time(nullptr) << "}";
        res.set_content(json.str(), "application/json");
    });

    svr.Get("/api/raw", [](const httplib::Request& req, httplib::Response& res) {
        auto from_param = req.get_param_value("from");
        auto to_param = req.get_param_value("to");
        time_t from = from_param.empty() ? (time(nullptr) - 3600) : std::stoll(from_param); 
        time_t to = to_param.empty() ? time(nullptr) : std::stoll(to_param);
        
        auto data = db->get_raw_data(from, to);
        std::ostringstream json;
        json << "{\"data\":[";
        for (size_t i = 0; i < data.size(); ++i) {
            json << "{\"timestamp\":" << data[i].timestamp 
                 << ",\"temperature\":" << data[i].temperature << "}";
            if (i < data.size() - 1) json << ",";
        }
        json << "]}";
        res.set_content(json.str(), "application/json");
    });

    svr.Get("/api/hourly", [](const httplib::Request& req, httplib::Response& res) {
        auto from_param = req.get_param_value("from");
        auto to_param = req.get_param_value("to");
        time_t from = from_param.empty() ? (time(nullptr) - 7200) : std::stoll(from_param);
        time_t to = to_param.empty() ? time(nullptr) : std::stoll(to_param);
        
        auto data = db->get_hourly_stats(from, to);
        std::ostringstream json;
        json << "{\"data\":[";
        for (size_t i = 0; i < data.size(); ++i) {
            json << "{\"timestamp\":" << data[i].timestamp 
                 << ",\"avg\":" << data[i].avg
                 << ",\"min\":" << data[i].min
                 << ",\"max\":" << data[i].max
                 << ",\"count\":" << data[i].count << "}";
            if (i < data.size() - 1) json << ",";
        }
        json << "]}";
        res.set_content(json.str(), "application/json");
    });

    svr.Get("/api/daily", [](const httplib::Request& req, httplib::Response& res) {
        auto from_param = req.get_param_value("from");
        auto to_param = req.get_param_value("to");
        time_t from = from_param.empty() ? (time(nullptr) - 86400) : std::stoll(from_param); 
        time_t to = to_param.empty() ? time(nullptr) : std::stoll(to_param);
        
        auto data = db->get_daily_stats(from, to);
        std::ostringstream json;
        json << "{\"data\":[";
        for (size_t i = 0; i < data.size(); ++i) {
            json << "{\"timestamp\":" << data[i].timestamp 
                 << ",\"avg\":" << data[i].avg
                 << ",\"min\":" << data[i].min
                 << ",\"max\":" << data[i].max
                 << ",\"count\":" << data[i].count << "}";
            if (i < data.size() - 1) json << ",";
        }
        json << "]}";
        res.set_content(json.str(), "application/json");
    });

    svr.set_mount_point("/", WEB_DIR);

    std::cout << "🌐 HTTP-сервер запущен на http://localhost:" << HTTP_PORT << std::endl;
    svr.listen("0.0.0.0", HTTP_PORT);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <порт> [скорость=9600]" << std::endl;
        std::cerr << "Пример: " << argv[0] << " /dev/pts/5 9600" << std::endl;
        return 1;
    }

    db = new Database(DB_FILE);

    const char* port_name = argv[1];
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        std::cerr << "Ошибка открытия порта " << port_name << std::endl;
        return 1;
    }

    if (!setup_serial(fd, 9600)) {
        close(fd);
        return 1;
    }

    std::cout << " Подключено к " << port_name << " на 9600 бод" << std::endl;
    std::cout << " Данные сохраняются в базу данных: " << DB_FILE << std::endl;
    std::cout << " HTTP API доступен на порту " << HTTP_PORT << std::endl;
    std::cout << " Веб-интерфейс: http://localhost:" << HTTP_PORT << "/" << std::endl;
    std::cout << "Нажмите Ctrl+C для остановки..." << std::endl;

    std::thread server_thread(http_server_thread);
    server_thread.detach();

    char buffer[256];
    while (true) {
        int received = read(fd, buffer, sizeof(buffer) - 1);
        if (received > 0) {
            buffer[received] = '\0';
            char* endptr;
            double temp = std::strtod(buffer, &endptr);
            if (endptr != buffer && (*endptr == '\0' || *endptr == '\n' || *endptr == '\r')) {
                std::cout << "[" << get_timestamp() << "]   Получено: " << temp << " °C" << std::endl;
                
                db->insert_raw(temp);
                db->cleanup_old_raw_data();
                db->cleanup_old_hourly_stats();
                
                raw_buffer.add(temp);
                hourly_buffer.add(temp);
                daily_buffer.add(temp);

                time_t now = time(nullptr);
                time_t current_hour = now - (now % 3600);
                if (current_hour > last_hour && hourly_buffer.size() > 0) {
                    calculate_and_save_hourly();
                    hourly_buffer = CircularBuffer(3600);
                    last_hour = current_hour;
                }

                time_t current_day = now - (now % (24*3600));
                if (current_day > last_day && daily_buffer.size() > 0) {
                    calculate_and_save_daily();
                    daily_buffer = CircularBuffer(24 * 3600);
                    last_day = current_day;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(fd);
    delete db;
    return 0;
}

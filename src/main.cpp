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
#include "../include/circular_buffer.h"

const char* RAW_LOG = "temperature_raw.log";
const char* HOURLY_LOG = "temperature_hourly.log";
const char* DAILY_LOG = "temperature_daily.log";

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

void write_to_file(const char* filename, const std::string& line) {
    std::ofstream file(filename, std::ios::app);
    if (file) {
        file << line << std::endl;
        file.close();
    }
}

void rotate_hourly_log() {
    std::ifstream in(HOURLY_LOG);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    in.close();

    if (lines.size() > 720) {
        lines.erase(lines.begin(), lines.end() - 720);
        std::ofstream out(HOURLY_LOG);
        for (const auto& l : lines) out << l << std::endl;
    }
}

void rotate_daily_log() {
    time_t now = time(nullptr);
    std::tm tm;
    localtime_r(&now, &tm);
    std::ostringstream yearly_name;
    yearly_name << "temperature_daily_" << (1900 + tm.tm_year) << ".log";

    if (std::string(DAILY_LOG) != yearly_name.str()) {
        std::rename(DAILY_LOG, yearly_name.str().c_str());
    }
}

void log_raw(double temp) {
    raw_buffer.add(temp);
    std::string line = get_timestamp() + " | " + std::to_string(temp) + " °C";
    write_to_file(RAW_LOG, line);

    std::ifstream in(RAW_LOG);
    std::vector<std::string> lines;
    std::string l;
    while (std::getline(in, l)) lines.push_back(l);
    in.close();

    if (!lines.empty()) {
        time_t cutoff = time(nullptr) - 24 * 3600;
        size_t first_valid = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].find('|') != std::string::npos) {
                std::string ts = lines[i].substr(0, 19);
                std::tm tm = {};
                std::istringstream ss(ts);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                if (mktime(&tm) >= cutoff) {
                    first_valid = i;
                    break;
                }
            }
        }
        if (first_valid > 0) {
            lines.erase(lines.begin(), lines.begin() + first_valid);
            std::ofstream out(RAW_LOG);
            for (const auto& line : lines) out << line << std::endl;
        }
    }
}

void check_hourly(time_t now) {
    std::tm tm;
    localtime_r(&now, &tm);
    time_t current_hour = now - (now % 3600);

    if (current_hour > last_hour && hourly_buffer.size() > 0) {
        double avg = hourly_buffer.calculate_average();
        std::string line = get_timestamp(current_hour) + " | " + std::to_string(avg) + " °C (hourly avg)";
        write_to_file(HOURLY_LOG, line);
        hourly_buffer = CircularBuffer(3600);
        rotate_hourly_log();
        last_hour = current_hour;
    }
}

void check_daily(time_t now) {
    std::tm tm;
    localtime_r(&now, &tm);
    time_t current_day = now - (now % (24 * 3600));

    if (current_day > last_day && daily_buffer.size() > 0) {
        double avg = daily_buffer.calculate_average();
        std::string line = get_timestamp(current_day) + " | " + std::to_string(avg) + " °C (daily avg)";
        write_to_file(DAILY_LOG, line);
        daily_buffer = CircularBuffer(24 * 3600);
        rotate_daily_log();
        last_day = current_day;
    }
}

// Настройка последовательного порта
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <порт> [скорость=9600]" << std::endl;
        std::cerr << "Примеры портов:" << std::endl;
        std::cerr << "  Linux:   /dev/ttyUSB0, /dev/ttyACM0, /dev/pts/5" << std::endl;
        return 1;
    }

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

    std::cout << "✅ Подключено к " << port_name << " на 9600 бод" << std::endl;
    std::cout << "Логи записываются в текущую папку:" << std::endl;
    std::cout << "  • " << RAW_LOG << " (последние 24ч)" << std::endl;
    std::cout << "  • " << HOURLY_LOG << " (последний месяц)" << std::endl;
    std::cout << "  • " << DAILY_LOG << " (текущий год)" << std::endl;
    std::cout << "Нажмите Ctrl+C для остановки..." << std::endl;

    char buffer[256];
    while (true) {
        int received = read(fd, buffer, sizeof(buffer) - 1);
        if (received > 0) {
            buffer[received] = '\0';
            char* endptr;
            double temp = std::strtod(buffer, &endptr);
            if (endptr != buffer && (*endptr == '\0' || *endptr == '\n' || *endptr == '\r')) {
                std::cout << "[" << get_timestamp() << "] Получено: " << temp << " °C" << std::endl;
                log_raw(temp);
                hourly_buffer.add(temp);
                daily_buffer.add(temp);

                time_t now = time(nullptr);
                check_hourly(now);
                check_daily(now);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    close(fd);
    return 0;
}

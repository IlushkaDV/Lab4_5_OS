#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string get_timestamp() {
    time_t now = time(nullptr);
    std::tm tm;
    localtime_r(&now, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
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

    std::cout << "✅ Симулятор запущен на " << port_name << " (9600 бод)" << std::endl;
    std::cout << "Генерация температуры от 18.0 до 28.0 °C каждые 5 секунд..." << std::endl;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(18.0, 28.0);

    while (true) {
        double temp = dis(gen);
        char buf[32];
        int len = std::snprintf(buf, sizeof(buf), "%.1f\n", temp);
	(void)write(fd, buf, len);  // Игнорируем возвращаемое значение
        std::cout << "[" << get_timestamp() << "] Отправлено: " << temp << " °C" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    close(fd);
    return 0;
}

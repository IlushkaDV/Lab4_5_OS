
#pragma once
#include <vector>
#include <string>
#include <ctime>
#include <algorithm>

struct TemperatureRecord {
    time_t timestamp;
    double temperature;
};

class CircularBuffer {
private:
    std::vector<TemperatureRecord> data;
    time_t retention_seconds;

public:
    explicit CircularBuffer(time_t retention) : retention_seconds(retention) {}

    void add(double temp) {
        cleanup_old();
        data.push_back({time(nullptr), temp});
    }

    double calculate_average() const {
        if (data.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& r : data) sum += r.temperature;
        return sum / data.size();
    }

    void cleanup_old() {
        time_t now = time(nullptr);
        auto it = std::remove_if(data.begin(), data.end(),
            [now, this](const TemperatureRecord& r) {
                return (now - r.timestamp) > retention_seconds;
            });
        data.erase(it, data.end());
    }

   size_t size() const { return data.size(); }

    // МЕТОД get_all ДОБАВЛЕН
    std::vector<TemperatureRecord> get_all() const {
        return data;
    }
};

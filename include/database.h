#pragma once
#include <sqlite3.h>
#include <string>
#include <vector>
#include <iostream>
#include <ctime>

class Database {
private:
    sqlite3* db;
    char* errMsg;

public:
    Database(const char* filename = "temperature.db") {
        if (sqlite3_open(filename, &db) != SQLITE_OK) {
            std::cerr << "Ошибка открытия БД: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
        } else {
            std::cout << " База данных открыта: " << filename << std::endl;
            create_tables();
        }
    }

    ~Database() {
        sqlite3_close(db);
    }

    void create_tables() {
        const char* sql_raw = 
            "CREATE TABLE IF NOT EXISTS raw_data ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp INTEGER NOT NULL,"
            "temperature REAL NOT NULL"
            ");";
        
        const char* sql_hourly = 
            "CREATE TABLE IF NOT EXISTS hourly_stats ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp INTEGER NOT NULL,"
            "avg_temperature REAL NOT NULL,"
            "min_temperature REAL NOT NULL,"
            "max_temperature REAL NOT NULL,"
            "sample_count INTEGER NOT NULL"
            ");";
        
        const char* sql_daily = 
            "CREATE TABLE IF NOT EXISTS daily_stats ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp INTEGER NOT NULL,"
            "avg_temperature REAL NOT NULL,"
            "min_temperature REAL NOT NULL,"
            "max_temperature REAL NOT NULL,"
            "sample_count INTEGER NOT NULL"
            ");";

        sqlite3_exec(db, sql_raw, nullptr, nullptr, &errMsg);
        sqlite3_exec(db, sql_hourly, nullptr, nullptr, &errMsg);
        sqlite3_exec(db, sql_daily, nullptr, nullptr, &errMsg);
    }

    bool insert_raw(double temp) {
        time_t now = time(nullptr);
        std::string sql = "INSERT INTO raw_data (timestamp, temperature) VALUES (" + 
                          std::to_string(now) + ", " + std::to_string(temp) + ");";
        return sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg) == SQLITE_OK;
    }

    bool insert_hourly(double avg, double min, double max, int count) {
        time_t now = time(nullptr);
    	std::string sql = "INSERT INTO hourly_stats (timestamp, avg_temperature, min_temperature, max_temperature, sample_count) VALUES (" +
                          std::to_string(now) + ", " + std::to_string(avg) + ", " + std::to_string(min) + ", " + 
                          std::to_string(max) + ", " + std::to_string(count) + ");";
    
    	int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    	if (rc != SQLITE_OK) {
            std::cerr << "❌ Ошибка вставки в hourly_stats: " << errMsg << std::endl;
            return false;
        }
    	return true;
    }

bool insert_daily(double avg, double min, double max, int count) {
    time_t now = time(nullptr);
    std::string sql = "INSERT INTO daily_stats (timestamp, avg_temperature, min_temperature, max_temperature, sample_count) VALUES (" +
                      std::to_string(now) + ", " + std::to_string(avg) + ", " + std::to_string(min) + ", " + 
                      std::to_string(max) + ", " + std::to_string(count) + ");";
    
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "❌ Ошибка вставки в daily_stats: " << errMsg << std::endl;
        return false;
    }
    return true;
}
    struct Reading {
        time_t timestamp;
        double temperature;
    };

    struct Stat {
        time_t timestamp;
        double avg;
        double min;
        double max;
        int count;
    };

    std::vector<Reading> get_raw_data(time_t from, time_t to) {
        std::vector<Reading> result;
        char sql[256];
        snprintf(sql, sizeof(sql), 
            "SELECT timestamp, temperature FROM raw_data WHERE timestamp BETWEEN %ld AND %ld ORDER BY timestamp ASC;",
            from, to);
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Reading r;
                r.timestamp = sqlite3_column_int64(stmt, 0);
                r.temperature = sqlite3_column_double(stmt, 1);
                result.push_back(r);
            }
            sqlite3_finalize(stmt);
        }
        return result;
    }

    std::vector<Stat> get_hourly_stats(time_t from, time_t to) {
        std::vector<Stat> result;
        char sql[256];
        snprintf(sql, sizeof(sql),
            "SELECT timestamp, avg_temperature, min_temperature, max_temperature, sample_count FROM hourly_stats WHERE timestamp BETWEEN %ld AND %ld ORDER BY timestamp ASC;",
            from, to);
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Stat s;
                s.timestamp = sqlite3_column_int64(stmt, 0);
                s.avg = sqlite3_column_double(stmt, 1);
                s.min = sqlite3_column_double(stmt, 2);
                s.max = sqlite3_column_double(stmt, 3);
                s.count = sqlite3_column_int(stmt, 4);
                result.push_back(s);
            }
            sqlite3_finalize(stmt);
        }
        return result;
    }

    std::vector<Stat> get_daily_stats(time_t from, time_t to) {
        std::vector<Stat> result;
        char sql[256];
        snprintf(sql, sizeof(sql),
            "SELECT timestamp, avg_temperature, min_temperature, max_temperature, sample_count FROM daily_stats WHERE timestamp BETWEEN %ld AND %ld ORDER BY timestamp ASC;",
            from, to);
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Stat s;
                s.timestamp = sqlite3_column_int64(stmt, 0);
                s.avg = sqlite3_column_double(stmt, 1);
                s.min = sqlite3_column_double(stmt, 2);
                s.max = sqlite3_column_double(stmt, 3);
                s.count = sqlite3_column_int(stmt, 4);
                result.push_back(s);
            }
            sqlite3_finalize(stmt);
        }
        return result;
    }

    double get_current_temperature() {
        double temp = 0.0;
        const char* sql = "SELECT temperature FROM raw_data ORDER BY timestamp DESC LIMIT 1;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                temp = sqlite3_column_double(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        return temp;
    }

    void cleanup_old_raw_data() {
        time_t cutoff = time(nullptr) - 24 * 3600;
        char sql[128];
        snprintf(sql, sizeof(sql), "DELETE FROM raw_data WHERE timestamp < %ld;", cutoff);
        sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    }

    void cleanup_old_hourly_stats() {
        time_t cutoff = time(nullptr) - 30 * 24 * 3600; // 30 дней
        char sql[128];
        snprintf(sql, sizeof(sql), "DELETE FROM hourly_stats WHERE timestamp < %ld;", cutoff);
        sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    }
};

#pragma once

class Logger {
    private:
    public:
        const char *name;
        Logger(const char *name);
        ~Logger();
        void info(const char *msg, ...);
        void error(const char *msg, ...);
        void debug(const char *msg, ...);
        void warn(const char *msg, ...);
};

#define log_with_line(log_call, text, ...) log_call("%s:%d: " text, __FILE__, __LINE__, __VA_ARGS__)
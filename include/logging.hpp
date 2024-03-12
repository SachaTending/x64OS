#pragma once

class Logger {
    private:
    public:
        const char *name;
        Logger(const char *name);
        void info(const char *msg, ...);
        void error(const char *msg, ...);
        void debug(const char *msg, ...);
};
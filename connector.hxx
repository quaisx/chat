#pragma once
#include <string>
#include <strstream>
#include <sw/redis++/redis++.h>

namespace kuna {
    namespace chat {
    const std::string TCP = "tcp";
    const std::string LOCAL_IP = "127.0.0.1";
    const std::string DEFAULT_PORT = "6379";
    class Connector
    {
        private:
        sw::redis::Redis* connection;
        static Connector *s_instance;
        std::ostringstream conn_str;
        Connector() : Connector(TCP, LOCAL_IP, DEFAULT_PORT)
        {

        }
        Connector(const std::string &transport, const std::string &ip, const std::string &port) {
            conn_str << transport << "://" << ip << ":" << port;
            connection = new sw::redis::Redis(sw::redis::Redis(conn_str.str().c_str()));
            
        }
        ~Connector() {
            if (connection) {
                delete connection;
            }
        }
        public:
        sw::redis::Redis& redis()
        {
            return *connection;
        }
        static Connector *instance()
        {
            if (!s_instance)
                s_instance = new Connector;
            return s_instance;
        }
        static void destroy() {
            if (s_instance) {
                delete s_instance;
            }
        }
    };
    }
}


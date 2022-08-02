#pragma once
#include <vector>
#include <string>
#include <sw/redis++/redis++.h>
#include "connector.hxx"
namespace kuna {
    namespace chat {
        template <typename T>
        class Entity {
            private:
                std::string _entity_;
            public:
                Entity(const std::string &entity) : _entity_{entity} {};
                virtual sw::redis::Redis & getRedis() {
                    return Connector::instance()->redis();
                };
                virtual std::string getEntity() { return _entity_; };
                virtual bool isValue(const T &) = 0;
                virtual std::vector<T> getValues() = 0;
                virtual bool storeValues(const std::vector<T> &) = 0;
                virtual bool delValue(const T &) = 0;
                virtual bool addValue(const T &) = 0;
                virtual std::vector<T> delEntity() = 0;
        };
    }
}
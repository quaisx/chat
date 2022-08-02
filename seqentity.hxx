#pragma once
#include "entity.hxx"

using namespace sw;
namespace kuna {
    namespace chat {
        template<typename T>
        class SeqEntity: public Entity<T> {
            public:
                SeqEntity(const std::string &entity) : Entity<T>(entity){};
                bool isValue(const T &val) override {
                    try {
                        std::vector<T> all_vals{};
                        this->getRedis().lrange(this->getEntity().c_str(), 0, -1, back_inserter(all_vals));
                        if (all_vals.size() > 0) {
                            auto it = find(all_vals.begin(), all_vals.end(), val);
                            if (it != all_vals.end()) return true;
                        }
                    } catch (redis::Error &e) {
                        std::cerr << e.what() << std::endl;
                    }
                    return false;
                }
                std::vector<T> delEntity() override {
                    auto vals = getValues();
                    if (vals.size()) {
                        this->getRedis().del(this->getEntity().c_str());
                    }
                    return vals;
                };
                std::vector<T> getValues() override {
                    std::vector<T> vals{};
                    try {
                        this->getRedis().lrange(this->getEntity().c_str(), 0, -1, std::back_inserter(vals));
                    } catch (redis::Error &e) {
                        std::cerr << e.what() << std::endl;
                    }
                    return vals;
                };
                bool storeValues(const std::vector<T> &vals) override {
                    bool flag = false;
                    try {
                        delEntity();
                        this->getRedis().rpush(this->getEntity().c_str(), vals.begin(), vals.end());
                        flag = true;
                    } catch (redis::Error &e) {
                        std::cerr << e.what() << std::endl;
                    }
                    return flag;
                };
                bool delValue(const T &val) override {
                    bool result = false;
                    try {
                        if (isValue(val)) {
                            auto all_vals = getValues();
                            all_vals.erase(find(all_vals.begin(), all_vals.end(), val));
                            if (!all_vals.size()) {
                                this->getRedis().del(this->getEntity().c_str());
                            } else {
                                storeValues(all_vals);
                            }
                        }
                        result = true;
                    } catch (redis::Error &e) {
                        std::cerr << e.what() << std::endl;
                    }
                    return result;
                };
                bool addValue(const T &val) override {
                    bool result = false;
                    try {
                        if (!isValue(val)) {
                            auto all_vals = getValues();
                            all_vals.push_back(val);
                            storeValues(all_vals);
                        }
                        result = true;
                    } catch (redis::Error &e) {
                        std::cerr << e.what() << std::endl;
                    }
                    return result;
                };
        };
    }
}
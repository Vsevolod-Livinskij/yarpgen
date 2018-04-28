/*
Copyright (c) 2018, Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "json.hpp"

#include <iostream>
#include <fstream>
#include <utility>

#include "utils.hpp"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This file implements the support of option parsing
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace yarpgen {

// It contains all data, which are necessary for Enum-String conversions
template<typename T>
struct EnumStrings {
    explicit EnumStrings (std::string init_data) {
       get_data(std::move(init_data));
    }

    static std::vector<std::string>& get_data (std::string init_data = "") {
        static std::vector<std::string> data;
        if (!init_data.empty()) {
            // Extract all enum values, separated by ", "
            std::string delimiter = ", ";
            size_t pos = 0;
            std::string token;
            while ((pos = init_data.find(delimiter)) != std::string::npos) {
                // Find, push to data and remove from string
                token = init_data.substr(0, pos);
                data.push_back(token);
                init_data.erase(0, pos + delimiter.length());
            }
            // Handle the last one
            data.push_back(init_data);
        }
        return data;
    }
};

// This function converts string to enum class value
template<typename T>
inline T enumFromString (const std::string &value) {
    // If we don't have any info about data in enum, process it
    std::vector<std::string>& data = EnumStrings<T>::get_data();

    // Find enum value by string key
    static auto begin = std::begin(data);
    static auto end   = std::end(data);

    auto find = std::find(begin, end, value);
    if (find != end)
        return static_cast<T>(std::distance(begin, find));
    ERROR("Can't find enum value");
}

// This function converts enum class value to string
template<typename T>
inline std::string stringFromEnum (T value) {
    return EnumStrings<T>::get_data().at(static_cast<size_t>(value));
}

// this is the only desired way to declare enum class with string conversion support
#define ENUM_CLASS_WITH_STR(name, ...) enum class name {__VA_ARGS__}; \
    namespace { \
        EnumStrings<name> enum_strings_##name (#__VA_ARGS__); \
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Possible Distribution types
enum class DistrType {UNIFORM, NORMAL};

// Common ancestor for all supported distributions
struct Distribution {
    DistrType distr_type;

    explicit Distribution(DistrType _distr) : distr_type(_distr) {}
};

// Class which represents Uniform Distribution
struct UniformDistribution : public Distribution {
    UniformDistribution() : Distribution(DistrType::UNIFORM) {}
};

// Class which represents Normal Distribution and holds its parameters
struct NormalDistribution : public Distribution {
    NormalDistribution(uint32_t _mean, uint32_t _dev) :
                       Distribution(DistrType::NORMAL), mean(_mean), deviation(_dev) {}
    NormalDistribution() : NormalDistribution (0, 0) {}

    uint32_t mean, deviation; // Both of these values are in percent of (max - min)
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Add variable to specified json
#define ADD_VAR(name, value, descr, json_obj, idx_str_map) do { \
                                        (name) = static_cast<decltype(name)>(value); \
                                        (json_obj) [#name] ["value"] = value; \
                                        (json_obj) [#name] ["descr"] = descr; \
                                        (idx_str_map).addIdx(#name); \
                                    } \
                                    while (false)

// Add probability variant to specified json
#define ADD_PROB(var, id, value, descr, json_obj, idx_str_map) do { \
                                          (json_obj) [#var] [#id] ["value"] = value; \
                                          (json_obj) [#var] [#id] ["descr"] = descr; \
                                          (idx_str_map).addIdx(#var); \
                                      } \
                                      while (false)

// Add uniform distribution to specified json
#define ADD_UNIF_DISTR(name, descr, json_obj, idx_str_map) do { \
                                         (json_obj) [#name] ["value"] = "uniform"; \
                                         (json_obj) [#name] ["descr"] = descr; \
                                         (idx_str_map).addIdx(#name); \
                                     } \
                                     while (false)

// Add normal distribution to specified json
#define ADD_NORM_DISTR(name, mean, dev, descr, json_obj, idx_str_map) do { \
                                        (json_obj) [#name] ["value"] = "normal"; \
                                        (json_obj) [#name] ["mean"] = mean; \
                                        (json_obj) [#name] ["dev"] = dev; \
                                        (json_obj) [#name] ["descr"] = descr; \
                                        (idx_str_map).addIdx(#name); \
                                    } \
                                    while (false)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Process json and modify variable accordingly
template<typename T>
inline void parseVariable(T& var, nlohmann::json::iterator& iter) {
    using ValueType = typename std::remove_cv<typename std::remove_reference<decltype(var)>::type>::type;
    var = iter.value().at("value").get<ValueType>();
}

#define PARSE_VAR(var, json_iter, missed) do { \
                                      if ((json_iter).key() == #var) { \
                                          parseVariable(var, json_iter); \
                                          (missed) = false; \
                                      } \
                                  } \
                                  while (false)

// Process json and add enum values to vector
template<typename T>
inline void addEnumToVec(std::vector<std::pair<T, uint64_t>>& vec, const std::string &enum_val, uint64_t prob_val) {
    std::string key ("::");
    std::size_t pos = enum_val.find(key);
    std::string shrt_enum_val = enum_val.substr(pos + key.length());
    vec.push_back(std::make_pair(enumFromString<T>(shrt_enum_val), prob_val));
}

template<>
inline void addEnumToVec(std::vector<std::pair<bool, uint64_t>>& vec, const std::string &enum_val, uint64_t prob_val) {
    bool id = (enum_val == "true");
    vec.emplace_back(id, prob_val);
}

// Process json and modify vector accordingly
template<typename T>
inline void parseVector(std::vector<std::pair<T, uint64_t>>& vec, nlohmann::json::iterator& iter) {
    for (nlohmann::json::iterator vec_json_iter = iter->begin(); vec_json_iter != iter->end(); ++vec_json_iter)
        addEnumToVec(vec, vec_json_iter.key(), vec_json_iter->at("value").get<uint64_t>());
}

#define PARSE_VEC(vec, json_iter, missed) do { \
                                      if ((json_iter).key() == #vec) { \
                                          parseVector(vec, json_iter); \
                                          (missed) = false; \
                                      } \
                                  } \
                                  while (false)

// Process json and modify probability variable accordingly
void parseDistribution(std::shared_ptr<Distribution> &distr, nlohmann::json::iterator& iter);

#define PARSE_DISTR(distr, json_iter, missed) do { \
                                      if ((json_iter).key() == #distr) { \
                                          parseDistribution(distr, json_iter); \
                                          (missed) = false; \
                                      } \
                                  } \
                                  while (false)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Special structure which holds info about idx-name relation of all options' variables
struct IdxStrConverter {
    IdxStrConverter() : idx_counter(0) {}
    void addIdx (std::string name);

    std::map<uint32_t, std::string> idx_to_str;
    std::map<std::string, uint32_t> str_to_idx;

private:
    uint32_t idx_counter;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string compressPatch (nlohmann::json patch, IdxStrConverter& idx_str_conv);

nlohmann::json decompressPatch (std::string str_patch, IdxStrConverter& idx_str_conv);

}

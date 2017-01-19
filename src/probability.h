/*
Copyright (c) 2015-2016, Intel Corporation

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

#include <random>
//#include <algorithm>
//#include <iostream>
//#include <memory>

///////////////////////////////////////////////////////////////////////////////

namespace rl {

template<typename T>
class Probability {
    public:
        Probability (T _id, int _prob) : id(_id), prob (_prob) {}
        T get_id () { return id; }
        uint64_t get_prob () { return prob; }

    private:
        T id;
        uint64_t prob;
};

class RandValGen {
    public:
        RandValGen (uint64_t _seed);
        template<typename T>
        T get_rand_value (T from, T to) {
            std::uniform_int_distribution<T> dis(from, to);
            return dis(rand_gen);
        }

        template<typename T>
        T get_rand_id (std::vector<Probability<T>> vec) {
            uint64_t max_prob = 0;
            for (auto i = vec.begin(); i != vec.end(); ++i)
                max_prob += (*i).get_prob();
            uint64_t rand_num = get_rand_value<uint64_t> (0, max_prob);
            for (auto i = vec.begin(); i != vec.end(); ++i) {
                max_prob -= (*i).get_prob();
                if (rand_num >= max_prob)
                    return (*i).get_id();
            }
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unable to select any id." << std::endl;
            exit (-1);
        }

        std::string get_struct_type_name() { return "struct_" + std::to_string(++struct_type_num); }
        uint64_t get_struct_type_num() { return struct_type_num; }
        std::string get_scalar_var_name() { return "var_" + std::to_string(++scalar_var_num); }
        std::string get_struct_var_name() { return "struct_obj_" + std::to_string(++struct_var_num); }

    private:
        uint64_t seed;
        std::mt19937_64 rand_gen;
        static uint64_t struct_type_num;
        static uint64_t scalar_var_num;
        static uint64_t struct_var_num;
};

extern std::shared_ptr<RandValGen> rand_val_gen;

}

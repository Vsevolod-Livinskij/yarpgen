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

#pragma once

#include "typedefs.h"

namespace rl {

class ItNameGen {
public:
    static std::vector<char> pieces;
    static std::vector<uint64_t> cnt;

/*
    static std::string getName () {
        std::stringstream ss;
        uint64_t shortest = get_shortest_it ();
        for (uint64_t i = 0; i < cnt.at(shortest) + 1; ++i)
            ss << pieces.at(shortest);

        cnt.at(shortest) ++;
        return ss.str();
    }
*/
    static std::string getName () {
        std::stringstream ss;
        uint64_t shortest = get_shortest_it ();
        ss << pieces.at(shortest) << cnt.at(shortest);
        cnt.at(shortest) ++;
        return ss.str();
    }

    static uint64_t get_shortest_it () {
        uint64_t ret = 0;
        uint64_t shortest = cnt.at(ret);
        for (uint64_t i = 0; i < pieces.size(); ++i) {
            if (cnt.at(i) < shortest) {
                ret = i;
                shortest = cnt.at(i);
            }
        }
        return ret;
    }
};


class SclNameGen {
public:
    static std::vector<char> pieces;
    static std::vector<uint64_t> cnt;

    static std::string getName () {
        std::stringstream ss;
        uint64_t shortest = get_shortest_it ();
        ss << pieces.at(shortest) << cnt.at(shortest);
        cnt.at(shortest) ++;
        return ss.str();
    }

    static uint64_t get_shortest_it () {
        uint64_t ret = 0;
        uint64_t shortest = cnt.at(ret);
        for (uint64_t i = 0; i < pieces.size(); ++i) {
            if (cnt.at(i) < shortest) {
                ret = i;
                shortest = cnt.at(i);
            }
        }
        return ret;
    }
};


class VecNameGen {
public:
    static std::vector<char> pieces;
    static std::vector<uint64_t> cnt;

    static std::string getName () {
        std::stringstream ss;
        uint64_t shortest = get_shortest_it ();
        ss << pieces.at(shortest) << cnt.at(shortest);
        cnt.at(shortest) ++;
        return ss.str();
    }

    static uint64_t get_shortest_it () {
        uint64_t ret = 0;
        uint64_t shortest = cnt.at(ret);
        for (uint64_t i = 0; i < pieces.size(); ++i) {
            if (cnt.at(i) < shortest) {
                ret = i;
                shortest = cnt.at(i);
            }
        }
        return ret;
    }
};


class ScopeIdGen {
protected:
    static uint64_t cnt;

public:
    static uint64_t getNewID() {cnt++; return cnt;}

};
}

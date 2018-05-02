/*
Copyright (c) 2017-2018, Intel Corporation

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

#include "options.hpp"

#include <algorithm>

#include "json.hpp"
#include "utils.hpp"

using namespace yarpgen;

GeneratorOptions::GeneratorOptions () {
    // Add user-defined parameters to JSON configuration
    ADD_VAR(standard_id, StandardID::CXX11, "selected std", default_json, idx_str_conv);
    ADD_VAR(mode_64bit, true, "is 64-bit mode on", default_json, idx_str_conv);

    json = default_json;

    // Set auxiliary parameters
    include_valarray = false;
    include_vector = false;
    include_array = false;

    // Initialize plane_yarpgen_version (delete all delimiters in yarpgen_version)
    plane_yarpgen_version = yarpgen_version;
    plane_yarpgen_version.erase(std::remove(plane_yarpgen_version.begin(), plane_yarpgen_version.end(), '.'),
                                plane_yarpgen_version.end());
}

GeneratorOptions& GeneratorOptions::getInstance () {
    static GeneratorOptions generator_options;
    return generator_options;
}

const std::map<std::string, StandardID> GeneratorOptions::str_to_standard = {
    {"c99", StandardID::C99},
    {"c11", StandardID::C11},

    {"c++98", StandardID::CXX98},
    {"c++03", StandardID::CXX03},
    {"c++11", StandardID::CXX11},
    {"c++14", StandardID::CXX14},
    {"c++17", StandardID::CXX17},

    /*
    {"opencl_1_0", StandardID::OpenCL_1_0},
    {"opencl_1_1", StandardID::OpenCL_1_1},
    {"opencl_1_2", StandardID::OpenCL_1_2},
    {"opencl_2_0", StandardID::OpenCL_2_0},
    {"opencl_2_1", StandardID::OpenCL_2_1},
    {"opencl_2_2", StandardID::OpenCL_2_2},
    */
};


void GeneratorOptions::setStandard (StandardID std_id) {
    standard_id = std_id;
    //TODO: is it better to do it through ADD_VAR?
    json ["standard_id"] ["value"] = standard_id;
}

void GeneratorOptions::set64BitMode (bool is_64_bit_mode) {
    mode_64bit = is_64_bit_mode;
    //TODO: is it better to do it through ADD_VAR?
    json ["mode_64bit"] ["value"] = mode_64bit;
}


bool GeneratorOptions::isC() {
    return StandardID::C99 <= standard_id && standard_id < StandardID::MAX_CStandardID;
}

bool GeneratorOptions::isCXX() {
    return StandardID::CXX98 <= standard_id && standard_id < StandardID::MAX_CXXStandardID;
}

/*
bool GeneratorOptions::isOpenCL() {
    return StandardID::OpenCL_1_0 <= standard_id && standard_id < StandardID::MAX_OpenCLStandardID;
}
*/

void GeneratorOptions::jsonToObj () {
    bool ever_missed = false;
    for (nlohmann::json::iterator it = json.begin(); it != json.end(); ++it) {
        bool missed = true;
        PARSE_VAR(standard_id, it, missed);
        PARSE_VAR(mode_64bit, it, missed);

        // Some error message and move forward
        ever_missed |= missed;
        if (missed)
            WARNING("Missed option in JSON configuration");
    }
    if (ever_missed)
        ERROR("One or more options in JSON configuration were missed");
}

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

#pragma once

#include <map>
#include <string>

#include "config_parser.hpp"

namespace yarpgen {

// Common ancestor for all options and configurations in generator
struct Options {
    // Object that helps to convert index to string and vice versa.
    IdxStrConverter idx_str_conv;
    // JSON object to store configuration
    nlohmann::json json;
    // JSON object to store default configuration
    nlohmann::json default_json;

    virtual ~Options () = default;
    // This function converts JSON to internal representation in generator
    virtual void jsonToObj () = 0;
    // This functions creates patch from internal JSON and new JSON configuration
    nlohmann::json createPatch () { return nlohmann::json::diff(default_json, json); }
    // This functions applies JSON patch to internal JSON configuration object
    void applyPatch (nlohmann::json& patch) { json = default_json; json.patch(patch); }
};


// IDs for all supported language standards
ENUM_CLASS_WITH_STR(StandardID, C99, C11, MAX_CStandardID,
                                CXX98, CXX03, CXX11, CXX14, CXX17, MAX_CXXStandardID)
                                // OpenCL_1_0, OpenCL_1_1, OpenCL_1_2,
                                // OpenCL_2_0, OpenCL_2_1, OpenCL_2_2, MAX_OpenCLStandardID



// This structure stores all of the options, required for YARPGen.
// Generator options are represented by singleton, but in a weird way
struct GeneratorOptions : public Options {
// YARPGen version
    // Yarpgen version supposed to be changed every time the generation algorithm is changed,
    // so version+seed should unambiguously correspond to generated test.
    // TODO: with more extra parameters taken into account, like target platform properties,
    //       limits, generation policies, and output language, we may want to encode all this in the seed.
    std::string yarpgen_version = "2.0";
    // YARPGen version without any delimiters (required for simple version  comparison)
    std::string plane_yarpgen_version;


// User-defined parameters
    // Selected language standard
    StandardID standard_id;
    // Determines if selected bit-mode is 64 bit-mode
    bool mode_64bit;


// Functions
    GeneratorOptions();
    static GeneratorOptions& getInstance();
    bool isC ();
    bool isCXX ();
    //bool isOpenCL ();
    void jsonToObj () override;
    void setStandard (StandardID std_id);
    void set64BitMode (bool is_64_bit_mode);


// Auxiliary parameters
    // This map matches StandardIDs to beautiful string literals for them
    static const std::map<std::string, StandardID> str_to_standard;
    // Determines if any of the selected headers are used and
    // therefore is it necessary to include them in test.
    bool include_valarray;
    bool include_vector;
    bool include_array;
};
}

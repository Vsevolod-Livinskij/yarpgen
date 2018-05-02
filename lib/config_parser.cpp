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

#include "config_parser.hpp"

#include <iostream>
#include <fstream>

#include "json.hpp"

using namespace yarpgen;

// Extract distribution info from json and add it to object
void parseDistribution(std::shared_ptr<Distribution> &distr, nlohmann::json::iterator& iter ) {
    std::string distr_kind = iter.value().at("value").get<std::string>();
    if (distr_kind == "uniform") {
        distr = std::make_shared<UniformDistribution>();
    }
    else if (distr_kind == "normal") {
        auto mean = iter.value().at("mean").get<uint32_t>();
        auto dev = iter.value().at("dev").get<uint32_t>();
        distr = std::make_shared<NormalDistribution>(mean, dev);
    }
    else
        ERROR("Can't define distribution type.");
}

// Add missing info about idx and its corresponding name
void IdxStrConverter::addIdx (std::string name) { 
    if (str_to_idx.count(name))
        return;

    str_to_idx [name] = idx_counter; 
    idx_to_str [idx_counter] = name;
    idx_counter++;
}

// All possible patch actions
ENUM_CLASS_WITH_STR(PatchActions, add, replace, remove)

static std::string outer_delimiter = "|";
static std::string inner_delimiter = "#";

// This function represents patch as a plain string and decreases its plain string size
// Compressed patch has the following form:
// operator_id#variable_id</optional_path>#<optional value>|
std::string compressPatch (nlohmann::json patch, IdxStrConverter& idx_str_conv) {
    std::stringstream out;
    for (nlohmann::json::iterator it = patch.begin(); it != patch.end(); ++it) {
        // Scan through json object, find "op" and replace it with its idx
        auto patch_action = enumFromString<PatchActions>(it.value().at("op").get<std::string>());
        out << std::to_string(static_cast<int32_t>(patch_action)) + inner_delimiter;

        // Scan through json object, find "path" and replace it with its idx +
        // all successors
        std::string path = it.value().at("path").get<std::string>();
        std::size_t delim_pos = path.find('/', 1);
        std::string opt_name;
        if (delim_pos != std::string::npos)
            // If path consists of several levels, replace with idx only first of them
            opt_name = path.substr(1, delim_pos - 1);
        else
            // If path consists of single level, just replace it all at once
            opt_name = path.substr(1, std::string::npos);
        out << std::to_string(idx_str_conv.str_to_idx.at(opt_name));

        // Add all leftover levels of path if they exist
        if (delim_pos != std::string::npos)
            out << inner_delimiter << path.substr(delim_pos + 1, std::string::npos);

        // Add value to patch if it exists
        if (it.value().find("value") != it.value().end())
            out << inner_delimiter << it.value().at("value");

        out << outer_delimiter;
    }

    return out.str();
}

// Extract info from inner patch, transform it to json patch format and add to patch object
static void processInnerPatch (std::string str_patch, nlohmann::json& patch, IdxStrConverter& idx_str_conv) {
    // Separate patch string into individual patches
    std::string delimiter = inner_delimiter;
    size_t pos = 0;
    std::string token;
    std::vector<std::string> patch_elems;
    while ((pos = str_patch.find(delimiter)) != std::string::npos) {
        token = str_patch.substr(0, pos);
        str_patch.erase(0, pos + delimiter.length());
        patch_elems.push_back(token);
    }
    patch_elems.push_back(str_patch);

    nlohmann::json result_patch;

    // Decode "op"
    auto action_id = static_cast<PatchActions>(std::stoi(patch_elems.front()));
    result_patch ["op"] = stringFromEnum<PatchActions>(action_id);

    // Decode path
    std::string path = "/" + idx_str_conv.idx_to_str[std::stoi(patch_elems.at(1))];
    if (patch_elems.size() > 2)
        path += "/" + patch_elems.at(2);
    result_patch ["path"] = path;

    // Decode value
    if (patch_elems.size() == 4)
        result_patch ["value"] = nlohmann::json::parse(patch_elems.back());

    // Add decoded result to global patch
    patch.emplace_back(result_patch);
}

// Construct json patch from custom plain string representation
nlohmann::json decompressPatch (std::string str_patch, IdxStrConverter& idx_str_conv) {
    nlohmann::json patch;
    str_patch.pop_back();

    // Separate string by outer delimiter and pass individual patches to processInnerPatch
    std::string delimiter = outer_delimiter;
    size_t pos = 0;
    std::string token;
    while ((pos = str_patch.find(delimiter)) != std::string::npos) {
        token = str_patch.substr(0, pos);
        str_patch.erase(0, pos + delimiter.length());
        processInnerPatch(token, patch, idx_str_conv);
    }
    processInnerPatch(str_patch, patch, idx_str_conv);

    return patch;
}

/*
Copyright (c) 2015-2018, Intel Corporation

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

#include <cassert>
#include <type.hpp>

#include "options.hpp"

#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif

#ifndef BUILD_VERSION
#define BUILD_VERSION ""
#endif

using namespace yarpgen;

void printVersion () {
    GeneratorOptions& generator_options = GeneratorOptions::getInstance();
    std::cout << "yarpgen version " + generator_options.yarpgen_version +
                 " (build " << BUILD_VERSION << " on " << BUILD_DATE << ")" << std::endl;
}

// This function checks if options start with specified string
bool option_starts_with (char* option, const char* test) {
    return !strncmp(option, test, strlen(test));
}

// This function prints out optional error_message, help and exits
void print_usage_and_exit (const std::string& error_msg = "") {
    // Print error message
    int exit_code = 0;
    if (!error_msg.empty()) {
        std::cerr << error_msg << std::endl;
        exit_code = -1;
    }

    // Print version and help
    std::cout << std::endl;
    printVersion();
    std::cout << "usage: yarpgen\n";
    std::cout << "\t-q                        Quiet mode\n";
    std::cout << "\t-v, --version             Print yarpgen version\n";
    std::cout << "\t-d, --out-dir=<out-dir>   Output directory\n";
    std::cout << "\t-s, --seed=<seed>         Predefined seed (it is accepted in form of SSS or VV_SSS)\n";
    std::cout << "\t--std=<standard>          Generated test's language standard\n";

    // Print all supported language standards
    GeneratorOptions& generator_options = GeneratorOptions::getInstance();

    std::cout << "Two:" << static_cast<int>(generator_options.standard_id) << std::endl;

    // Find and print string for selected standard
    auto search_for_default_std = [&generator_options] (const std::pair<std::string, StandardID> &pair) {
        return pair.second == generator_options.standard_id;
    };
    auto search_res = std::find_if(GeneratorOptions::str_to_standard.begin(),
                                   GeneratorOptions::str_to_standard.end(),
                                   search_for_default_std);
    assert(search_res != GeneratorOptions::str_to_standard.end() && "Can't match default standard_id and string");
    std::cout << "\t\t\t\t  Default: " << search_res->first << "\n";

    // Handle the rest
    std::string all_standards = "\t\t\t\t  Possible variants are:";
    for (const auto &iter : GeneratorOptions::str_to_standard)
        all_standards += " " + iter.first + ",";
    all_standards.pop_back();
    std::cout << all_standards << std::endl;

    exit (exit_code);
}

// This function handles command-line options in form of "-short_arg <value>" and performs action(<value>)
bool parse_short_args (int argc, int &argv_iter, char** &argv, const std::string &short_arg,
                       const std::function<void(char*)> &action, const std::string &error_msg) {
    if (!strcmp(argv[argv_iter], short_arg.c_str())) {
        if (++argv_iter == argc)
            print_usage_and_exit(error_msg);
        else {
            action(argv[argv_iter]);
            return true;
        }
    }
    return false;
}

// This function handles command-line options in form of "--long_arg=<value>" and performs action(<value>)
bool parse_long_args (int &argv_iter, char** &argv, const std::string &long_arg,
                      const std::function<void(char*)> &action, const std::string &error_msg) {
    if (option_starts_with(argv[argv_iter], (long_arg + "=").c_str())) {
        size_t option_end = strlen((long_arg + "=").c_str());
        if (strlen(argv[argv_iter]) == option_end)
            print_usage_and_exit(error_msg);
        else {
            action(argv[argv_iter] + strlen((long_arg + "=").c_str()));
            return true;
        }
    }
    return false;
}

bool parse_long_and_short_args (int argc, int &argv_iter, char** &argv, const std::string &short_arg,
                                const std::string &long_arg, const std::function<void(char*)> &action,
                                const std::string &error_msg) {
    return parse_long_args (      argv_iter, argv, long_arg , action, error_msg) ||
           parse_short_args(argc, argv_iter, argv, short_arg, action, error_msg);
}

void type_test();
void ir_value_test();

int main (int argc, char* argv[128]) {
    GeneratorOptions& generator_options = GeneratorOptions::getInstance();
    uint64_t seed = 0;
    std::string out_dir = "./";
    bool quiet = false;

    //TODO: maybe it is better to move all of this functions to separate file?
    // Utility functions. They are necessary for copy-paste reduction. They perform main actions during option parsing.
    // Detects output directory
    auto out_dir_action = [&out_dir] (std::string arg) {
        out_dir = arg;
    };

    // Detects predefined seed
    auto seed_action = [&seed, &generator_options] (std::string arg) {
        // Separate seed string to substrings (version and seed itself)
        size_t *pEnd = nullptr;
        std::stringstream arg_ss(arg);
        std::string segment;
        std::vector<std::string> seed_list;
        while(std::getline(arg_ss, segment, '_'))
            seed_list.push_back(segment);

        // Compare version
        if ((seed_list.size() > 1 && seed_list.at(0) != generator_options.plane_yarpgen_version) ||
            seed_list.size() > 2)
            ERROR("Incompatible yarpgen version in seed: " + arg);

        // Recognize seed
        try {
            seed = std::stoull(seed_list.back(), pEnd, 10);
        }
        catch (std::invalid_argument& e) {
            print_usage_and_exit("Can't recognize seed: " + arg);
        }
    };

    // Detects desired language standard
    auto standard_action = [&generator_options] (std::string arg) {
        auto search_res = GeneratorOptions::str_to_standard.find(arg);
        if (search_res != GeneratorOptions::str_to_standard.end())
            generator_options.setStandard(search_res->second);
        else {
            print_usage_and_exit("Can't recognize language standard: --std=" + arg + "\n");
        }
    };

    // Main loop for parsing command-line options
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            print_usage_and_exit();
        }
        else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-v")) {
            printVersion();
            exit(0);
        }
        else if (!strcmp(argv[i], "-q")) {
            quiet = true;
        }
        else if (parse_long_args(i, argv, "--std", standard_action,
                                 "Can't recognize language standard:")) {}
        else if (parse_long_and_short_args(argc, i, argv, "-d", "--out-dir", out_dir_action,
                                           "Output directory wasn't specified.")) {}
        else if (parse_long_and_short_args(argc, i, argv, "-s", "--seed", seed_action,
                                           "Seed wasn't specified.")) {}
        else if (argv[i][0] == '-') {
            print_usage_and_exit("Unknown option: " + std::string(argv[i]));
        }
    }

    if (argc == 1 && !quiet) {
        std::cerr << "Using default options" << std::endl;
        std::cerr << "For help type " << argv [0] << " -h" << std::endl;
    }

    // Apply chosen options
    generator_options.jsonToObj();

//    type_test();
    ir_value_test();

    return 0;
}

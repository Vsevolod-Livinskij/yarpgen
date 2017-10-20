/*
Copyright (c) 2017, Intel Corporation

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

#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <cmath>
#include <cfloat>
#include <algorithm>

struct TestResult {
    explicit TestResult(std::string _test_name, std::vector<std::string>& all_names) :
            test_name(_test_name), is_finished(false), result(0.0) {
        inp_file = std::ifstream(test_name);
        if (!inp_file.is_open()) {
            std::cout << test_name << std::endl;
            exit(0);
        }
        for (auto const& i : all_names)
            if (i != test_name)
                similar_results.push_back(i);
    }

    void remove_from_similar(const std::string &name) {
        auto cmp_names = [name] (std::string elem_name) {
            return name == elem_name;
        };
        similar_results.erase(std::remove_if(similar_results.begin(), similar_results.end(), cmp_names),
                              similar_results.end());
    }

    std::string test_name;
    std::ifstream inp_file;
    bool is_finished;
    long double result;
    std::vector<std::string> similar_results;
};

std::vector<std::string> check_if_all_has_finished(std::vector<TestResult> &test_results) {
    std::vector<std::string> ret;
    for (auto const& test_result : test_results)
        if (test_result.is_finished)
            ret.push_back(test_result.test_name);
    return ret;
}

bool are_results_equal(TestResult &result1, TestResult &result2) {
    return std::signbit(result1.result) == std::signbit(result2.result) &&
           std::abs(std::abs(result1.result / result2.result) - 1) < 0.01;
}

void print_results(std::vector<TestResult>& test_results, std::vector<std::string> also_failed = std::vector<std::string>()) {
    std::vector<std::string> already_processed;
    auto is_elem_in_vec = [] (std::vector<std::string> vec, std::string elem) {
        return std::find(vec.begin(), vec.end(), elem) != vec.end();
    };

    for (auto const& test_name : also_failed) {
        if (is_elem_in_vec(already_processed, test_name))
            continue;
        std::cout << test_name << " ";
        already_processed.push_back(test_name);
    }
    if (!already_processed.empty())
        std::cout << std::endl;

    for (auto const& test_result : test_results) {
        if (is_elem_in_vec(already_processed, test_result.test_name))
            continue;
        std::cout << test_result.test_name << " ";
        for (auto const &similar : test_result.similar_results) {
            if (is_elem_in_vec(already_processed, similar))
                continue;
            std::cout << similar << " ";
            already_processed.push_back(similar);
        }
        std::cout << std::endl;
    }
}

int main (int argc, char* argv[]) {
    // Open all inp files
    std::vector<std::string> all_test_names(&(argv[1]), argv + argc);
    std::vector<TestResult> test_results;
    for (int i = 1; i < argc; ++i)
        test_results.emplace_back(argv[i], all_test_names);

    std::vector<std::string> finished_files;
    std::vector<std::string> false_finish;
    std::string line;
    while (true) {
        // Read one line from all files
        for (auto& test_result : test_results) {
            std::getline(test_result.inp_file, line);
            if (test_result.is_finished)
                continue;
            std::istringstream iss(line);
            //TODO: need to print names
            if (!(iss >> test_result.result))
                test_result.is_finished = true;
        }

        // Check if files have different length
        finished_files = check_if_all_has_finished(test_results);
        //TODO: print difference
        if (!finished_files.empty() && finished_files.size() == test_results.size()) {
            print_results(test_results);
            exit(0);
        }
        if (!finished_files.empty() && finished_files.size() != test_results.size()) {
            for (auto const& i : finished_files)
                false_finish.push_back(i);
            print_results(test_results, false_finish);
            exit(0);
        }

        // Compare obtained results
        for (int i = 0; i < test_results.size() - 1; ++i)
            for (int j = i + 1; j < test_results.size(); ++j)
                if (!are_results_equal(test_results.at(i), test_results.at(j))) {
                    test_results.at(i).remove_from_similar(test_results.at(j).test_name);
                    test_results.at(j).remove_from_similar(test_results.at(i).test_name);
                }

    }
}
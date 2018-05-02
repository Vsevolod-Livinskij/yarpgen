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

#include "type_enums.hpp"
#include "type.hpp"

using namespace yarpgen;

void type_test() {
    for (auto i = static_cast<int>(IntTypeID::BOOL); i <= static_cast<int>(IntTypeID::ULLONG); ++i)
        for (auto j = static_cast<int>(CVQualifier::NTHG); j <=static_cast<int>(CVQualifier::CONST_VOLAT); ++j)
            for (int k = false; k <= true; ++k) {
                std::shared_ptr<IntegralType> ptr_to_type = IntegralType::init(static_cast<IntTypeID>(i),
                                                                               static_cast<bool>(k),
                                                                               static_cast<CVQualifier>(j));
        }
    for (auto i = static_cast<int>(IntTypeID::BOOL); i <= static_cast<int>(IntTypeID::ULLONG); ++i)
        for (auto j = static_cast<int>(CVQualifier::NTHG); j <=static_cast<int>(CVQualifier::CONST_VOLAT); ++j)
            for (int k = false; k <= true; ++k) {
                std::shared_ptr<IntegralType> ptr_to_type = IntegralType::init(static_cast<IntTypeID>(i),
                                                                               static_cast<bool>(k),
                                                                               static_cast<CVQualifier>(j));
                ptr_to_type->dbgDump();
                std::cout << "-------------------" << std::endl;
            }
}
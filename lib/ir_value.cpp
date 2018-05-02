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

#include "ir_value.hpp"

using namespace yarpgen;

template <>
bool& IRValue::get_value () {
    return value.bool_val;
}

template <>
signed char & IRValue::get_value () {
    return value.schar_val;
}

template <>
unsigned char & IRValue::get_value () {
    return value.uchar_val;
}

template <>
short & IRValue::get_value () {
    return value.shrt_val;
}

template <>
unsigned short & IRValue::get_value () {
    return value.ushrt_val;
}

template <>
int & IRValue::get_value () {
    return value.int_val;
}

template <>
unsigned int & IRValue::get_value () {
    return value.uint_val;
}

template <>
long long int & IRValue::get_value () {
    return value.llong_val;
}

template <>
unsigned long long int & IRValue::get_value () {
    return value.ullong_val;
}
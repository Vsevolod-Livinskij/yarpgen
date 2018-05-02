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

#include "type_enums.hpp"

namespace yarpgen {

ENUM_CLASS_WITH_STR(UB,     NoUB,
                            NullPtr, // nullptr ptr dereferencing
    SignOvf, // Signed overflow
    SignOvfMin, // Special case of signed overflow: INT_MIN * (-1)
    ZeroDiv, // FPE
    ShiftRhsNeg, // Shift by negative value
    ShiftRhsLarge, // // Shift by large value
    NegShift, // Shift of negative value
    NoMemeber, // Can't find member of structure
    MaxUB)

// This class represents all scalar values in Intermediate Representation
class IRValue {
public:
    union Value {
        bool bool_val;
        signed char schar_val;
        unsigned char uchar_val;
        short shrt_val;
        unsigned short ushrt_val;
        int int_val;
        unsigned int uint_val;
        long long int llong_val;
        unsigned long long int ullong_val;
    };

    template <typename T>
    T& getValueRef();

    IntTypeID getIntTypeID() { return type_id; }

private:
    Value value;
    IntTypeID type_id;
    UB ub_code;
};

template <>
bool& IRValue::getValueRef();

template <>
signed char & IRValue::getValueRef();

template <>
unsigned char & IRValue::getValueRef();

template <>
short & IRValue::getValueRef();

template <>
unsigned short & IRValue::getValueRef();

template <>
int & IRValue::getValueRef();

template <>
unsigned int & IRValue::getValueRef();

template <>
long long int & IRValue::getValueRef();

template <>
unsigned long long int & IRValue::getValueRef();
}


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

#pragma once

#include <cstring>
#include <climits>

#include "type_enums.hpp"
#include "ir_value.hpp"

#ifndef BIT_MODE64
#define BIT_MODE64 true
#endif

namespace yarpgen {

// Abstract class, serves as a common ancestor for all types.
class Type {
    virtual std::string getName() = 0;
};

class ArithmeticType : public Type {
    virtual std::string getLiteralSuffix() { return ""; };
};

class FPType : public ArithmeticType {
    //TODO: it is a stub for now
};

class IntegralType : public ArithmeticType {
public:
    virtual uint32_t getBitSize() = 0;
    virtual bool getIsSigned() = 0;
    virtual IntTypeID getIntTypeId() = 0;
    virtual IRValue getMin() = 0;
    virtual IRValue getMax() = 0;

protected:
    virtual void init(IRValue& min, IRValue& max) = 0;
};

template <typename T>
class IntegralTypeHelper : public IntegralType {
public:
    uint32_t getBitSize() override { return sizeof(T) * CHAR_BIT; }
    bool getIsSigned() override { return std::is_signed<T>::value; }
    void init(IRValue& min, IRValue& max) override {
        min.get_value<T>() = std::numeric_limits<T>::min();
        max.get_value<T>() = std::numeric_limits<T>::max();
    }
};

class TypeBool : public IntegralTypeHelper<bool> {
public:
    typedef bool value_type;
    TypeBool() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::BOOL; }
    std::string getName() final { return "bool"; }

    // For bool signedness is not defined, so std::is_signed and std::is_unsigned return true.
    // We treat them as unsigned
    bool getIsSigned() final { return false; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeSChar : public IntegralTypeHelper<signed char> {
public:
    typedef signed char value_type;
    TypeSChar() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::SCHAR; }
    std::string getName() final { return "signed char"; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeUChar : public IntegralTypeHelper<unsigned char> {
public:
    typedef unsigned char value_type;
    TypeUChar() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::UCHAR; }
    std::string getName() final { return "unsigned char"; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeSShort : public IntegralTypeHelper<short> {
public:
    typedef short value_type;
    TypeSShort() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::SHORT; }
    std::string getName() final { return "short"; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeUShort : public IntegralTypeHelper<unsigned short> {
public:
    typedef unsigned short value_type;
    TypeUShort() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::USHORT; }
    std::string getName() final { return "unsigned short"; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeSInt : public IntegralTypeHelper<int> {
public:
    typedef int value_type;
    TypeSInt() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::INT; }
    std::string getName() final { return "int"; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeUInt : public IntegralTypeHelper<unsigned int> {
public:
    typedef unsigned int value_type;
    TypeUInt() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::UINT; }
    std::string getName() final { return "unsigned int"; }
    std::string getLiteralSuffix() final { return "U"; }

private:
    static IRValue min;
    static IRValue max;
};

//TODO: bit mode
#if BIT_MODE64==true
class TypeSLong : public IntegralTypeHelper<long long int> {
public:
    typedef long long int value_type;
#else
class TypeSLong : public IntegralTypeHelper<int> {
public:
    typedef int value_type;
#endif
    TypeSLong() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::LONG; }
    std::string getName() final { return "long int"; }
    std::string getLiteralSuffix() final { return "L"; }

private:
    static IRValue min;
    static IRValue max;
};

#if BIT_MODE64==true
class TypeULong : public IntegralTypeHelper<unsigned long long int> {
public:
    typedef unsigned long long int value_type;
#else
    class TypeULong : public IntegralTypeHelper<unsigned int> {
public:
    typedef unsigned int value_type;
#endif
    TypeULong() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::ULONG; }
    std::string getName() final { return "unsigned long int"; }
    std::string getLiteralSuffix() final { return "UL"; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeSLLong : public IntegralTypeHelper<long long int> {
public:
    typedef long long int value_type;
    TypeSLLong() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::LLONG; }
    std::string getName() final { return "long long int"; }
    std::string getLiteralSuffix() final { return "LL"; }

private:
    static IRValue min;
    static IRValue max;
};

class TypeULLong : public IntegralTypeHelper<unsigned long long int> {
public:
    typedef unsigned long long int value_type;
    TypeULLong() { init(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::ULONG; }
    std::string getName() final { return "unsigned long long int"; }
    std::string getLiteralSuffix() final { return "ULL"; }

private:
    static IRValue min;
    static IRValue max;
};

}
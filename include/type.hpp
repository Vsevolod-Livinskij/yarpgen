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
#include <unordered_map>

#include "type_enums.hpp"
#include "ir_value.hpp"

#ifndef BIT_MODE64
#define BIT_MODE64 true
#endif

namespace yarpgen {

// Abstract class, serves as a common ancestor for all types.
class Type {
public:
    Type () : is_static (false), cv_qualifier(CVQualifier::NTHG) {}
    Type (bool _is_static, CVQualifier _cv_qual) : is_static(_is_static), cv_qualifier(_cv_qual) {}
    virtual ~Type() {}

    virtual std::string getName() = 0;
    virtual void dbgDump() = 0;

    bool getIsStatic() { return is_static; }
    void setIsStatic(bool _is_static) { is_static = _is_static; }
    CVQualifier getCVQualifier() { return cv_qualifier; }
    void setCVQualifier(CVQualifier _cv_qual) { cv_qualifier = _cv_qual; }

protected:
    bool is_static;
    CVQualifier cv_qualifier;
};

class ArithmeticType : public Type {
public:
    ArithmeticType() : Type() {}
    ArithmeticType (bool _is_static, CVQualifier _cv_qual) : Type(_is_static, _cv_qual) {}
    virtual std::string getLiteralSuffix() { return ""; };
};

class FPType : public ArithmeticType {
    //TODO: it is a stub for now
};

// Custom Hash for Key in unordered_map.
// Special thanks to
//  https://stackoverflow.com/questions/17016175

struct IntTypeKey {
    IntTypeKey (IntTypeID _int_type_id, bool _is_static, CVQualifier _cv_qualifier) :
                int_type_id(_int_type_id), is_static(_is_static), cv_qualifier(_cv_qualifier) {}

    bool operator== (const IntTypeKey &other) const {
        return (int_type_id == other.int_type_id &&
                is_static == other.is_static &&
                cv_qualifier == other.cv_qualifier);
    }

    IntTypeID int_type_id;
    bool is_static;
    CVQualifier cv_qualifier;
};

struct IntTypeKeyHasher {
    std::size_t operator() (const IntTypeKey& key) const {
        std::size_t hash_seed = 17;
        using under_type_of_id = std::underlying_type<decltype(key.int_type_id)>::type;
        using under_type_of_cv_qual = std::underlying_type<decltype(key.cv_qualifier)>::type;
        hash_seed = hash_seed * 31 + std::hash<under_type_of_id>()(static_cast<under_type_of_id>(key.int_type_id));
        hash_seed = hash_seed * 31 + std::hash<decltype(key.is_static)>()(key.is_static);
        hash_seed = hash_seed * 31 + std::hash<under_type_of_cv_qual >()(
                                     static_cast<under_type_of_cv_qual>(key.cv_qualifier));
        return hash_seed;
    }
};

class IntegralType : public ArithmeticType {
public:
    IntegralType() : ArithmeticType() {}
    IntegralType(bool _is_static, CVQualifier _cv_qual) : ArithmeticType(_is_static, _cv_qual) {}
    virtual IntTypeID getIntTypeId() = 0;
    virtual uint32_t getBitSize() = 0;
    virtual bool getIsSigned() = 0;
    virtual IRValue getMin() = 0;
    virtual IRValue getMax() = 0;

    // This utility functions take IntegerTypeID and return shared pointer to corresponding type
    static std::shared_ptr<IntegralType> init (IntTypeID _type_id);
    static std::shared_ptr<IntegralType> init (IntTypeID _type_id, bool _is_static, CVQualifier _cv_qual);

protected:
    virtual void initType(IRValue &min, IRValue &max) = 0;

private:
    static std::unordered_map<IntTypeKey, std::shared_ptr<IntegralType>, IntTypeKeyHasher> int_type_buffer;
};

template <typename T>
class IntegralTypeHelper : public IntegralType {
public:
    IntegralTypeHelper(bool _is_static, CVQualifier _cv_qual) : IntegralType(_is_static, _cv_qual) {}
    uint32_t getBitSize() override { return sizeof(T) * CHAR_BIT; }
    bool getIsSigned() override { return std::is_signed<T>::value; }
    void initType(IRValue &min, IRValue &max) override {
        min.getValueRef<T>() = std::numeric_limits<T>::min();
        max.getValueRef<T>() = std::numeric_limits<T>::max();
    }
};

class TypeBool : public IntegralTypeHelper<bool> {
public:
    typedef bool value_type;
    TypeBool(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::BOOL; }
    std::string getName() final { return "bool"; }

    // For bool signedness is not defined, so std::is_signed and std::is_unsigned return true.
    // We treat them as unsigned
    bool getIsSigned() final { return false; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeSChar : public IntegralTypeHelper<signed char> {
public:
    typedef signed char value_type;
    TypeSChar(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::SCHAR; }
    std::string getName() final { return "signed char"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeUChar : public IntegralTypeHelper<unsigned char> {
public:
    typedef unsigned char value_type;
    TypeUChar(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::UCHAR; }
    std::string getName() final { return "unsigned char"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeSShort : public IntegralTypeHelper<short> {
public:
    typedef short value_type;
    TypeSShort(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::SHORT; }
    std::string getName() final { return "short"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeUShort : public IntegralTypeHelper<unsigned short> {
public:
    typedef unsigned short value_type;
    TypeUShort(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::USHORT; }
    std::string getName() final { return "unsigned short"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeSInt : public IntegralTypeHelper<int> {
public:
    typedef int value_type;
    TypeSInt(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::INT; }
    std::string getName() final { return "int"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeUInt : public IntegralTypeHelper<unsigned int> {
public:
    typedef unsigned int value_type;
    TypeUInt(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::UINT; }
    std::string getName() final { return "unsigned int"; }
    std::string getLiteralSuffix() final { return "U"; }

    void dbgDump();

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
    TypeSLong(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::LONG; }
    std::string getName() final { return "long int"; }
    std::string getLiteralSuffix() final { return "L"; }

    void dbgDump();

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
    TypeULong(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::ULONG; }
    std::string getName() final { return "unsigned long int"; }
    std::string getLiteralSuffix() final { return "UL"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeSLLong : public IntegralTypeHelper<long long int> {
public:
    typedef long long int value_type;
    TypeSLLong(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::LLONG; }
    std::string getName() final { return "long long int"; }
    std::string getLiteralSuffix() final { return "LL"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

class TypeULLong : public IntegralTypeHelper<unsigned long long int> {
public:
    typedef unsigned long long int value_type;
    TypeULLong(bool _is_static, CVQualifier _cv_qual) : IntegralTypeHelper(_is_static, _cv_qual) { initType(min, max); }
    IRValue getMin() final { return min; }
    IRValue getMax() final { return max; }

    IntTypeID getIntTypeId() final { return IntTypeID::ULONG; }
    std::string getName() final { return "unsigned long long int"; }
    std::string getLiteralSuffix() final { return "ULL"; }

    void dbgDump();

private:
    static IRValue min;
    static IRValue max;
};

}
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

#include "type.hpp"

#include <memory>
#include <unordered_map>

using namespace yarpgen;

IRValue yarpgen::TypeBool::min;
IRValue yarpgen::TypeBool::max;

IRValue yarpgen::TypeSChar::min;
IRValue yarpgen::TypeSChar::max;

IRValue yarpgen::TypeUChar::min;
IRValue yarpgen::TypeUChar::max;

IRValue yarpgen::TypeSShort::min;
IRValue yarpgen::TypeSShort::max;

IRValue yarpgen::TypeUShort::min;
IRValue yarpgen::TypeUShort::max;

IRValue yarpgen::TypeSInt::min;
IRValue yarpgen::TypeSInt::max;

IRValue yarpgen::TypeUInt::min;
IRValue yarpgen::TypeUInt::max;

IRValue yarpgen::TypeSLong::min;
IRValue yarpgen::TypeSLong::max;

IRValue yarpgen::TypeULong::min;
IRValue yarpgen::TypeULong::max;

IRValue yarpgen::TypeSLLong::min;
IRValue yarpgen::TypeSLLong::max;

IRValue yarpgen::TypeULLong::min;
IRValue yarpgen::TypeULLong::max;


std::unordered_map<IntTypeKey, std::shared_ptr<IntegralType>, IntTypeKeyHasher> yarpgen::IntegralType::int_type_buffer;

std::shared_ptr<IntegralType> IntegralType::init(IntTypeID _type_id) {
    return init(_type_id, false, CVQualifier::NTHG);
}

std::shared_ptr<IntegralType> IntegralType::init (IntTypeID _type_id, bool _is_static, CVQualifier _cv_qual) {
    IntTypeKey key (_type_id, _is_static, _cv_qual);
    auto search = int_type_buffer.find(key);
    if (search != int_type_buffer.end())
        return search->second;

    std::shared_ptr<IntegralType> ret;
    switch (_type_id) {
        case IntTypeID::BOOL:
            ret = std::make_shared<TypeBool>(TypeBool(_is_static, _cv_qual));
            break;
        case IntTypeID::SCHAR:
            ret = std::make_shared<TypeSChar>(TypeSChar(_is_static, _cv_qual));
            break;
        case IntTypeID::UCHAR:
            ret = std::make_shared<TypeUChar>(TypeUChar(_is_static, _cv_qual));
            break;
        case IntTypeID::SHORT:
            ret = std::make_shared<TypeSShort>(TypeSShort(_is_static, _cv_qual));
            break;
        case IntTypeID::USHORT:
            ret = std::make_shared<TypeUShort>(TypeUShort(_is_static, _cv_qual));
            break;
        case IntTypeID::INT:
            ret = std::make_shared<TypeSInt>(TypeSInt(_is_static, _cv_qual));
            break;
        case IntTypeID::UINT:
            ret = std::make_shared<TypeUInt>(TypeUInt(_is_static, _cv_qual));
            break;
        case IntTypeID::LONG:
            ret = std::make_shared<TypeSLong>(TypeSLong(_is_static, _cv_qual));
            break;
        case IntTypeID::ULONG:
            ret = std::make_shared<TypeULong>(TypeULong(_is_static, _cv_qual));
            break;
        case IntTypeID::LLONG:
            ret = std::make_shared<TypeSLLong>(TypeSLLong(_is_static, _cv_qual));
            break;
        case IntTypeID::ULLONG:
            ret = std::make_shared<TypeULLong>(TypeULLong(_is_static, _cv_qual));
            break;
        case IntTypeID::MAX_INT_TYPE_ID:
            ERROR("Unsupported IntTypeID");
    }

    int_type_buffer[key] = ret;
    return ret;
}

template <typename T>
static void dbgDumpHelper(IntTypeID id, std::string name, std::string suffix, uint32_t bit_size, bool is_signed,
                          T& min, T& max, bool is_static, CVQualifier cv_qual) {
    std::cout << "int type id:  " << static_cast<std::underlying_type<IntTypeID>::type>(id) << std::endl;
    std::cout << "name:         " << name << std::endl;
    std::cout << "bit_size:     " << bit_size << std::endl;
    std::cout << "is_signed:    " << is_signed << std::endl;
    std::cout << "min:          " << min << suffix << std::endl;
    std::cout << "max:          " << max << suffix << std::endl;
    std::cout << "is_static:    " << is_static << std::endl;
    std::cout << "cv_qualifier: " << static_cast<std::underlying_type<CVQualifier>::type>(cv_qual) << std::endl;
}

#define DBG_DUMP_MACROS(type_name) void type_name::dbgDump() { \
    dbgDumpHelper(getIntTypeId(), getName(), getLiteralSuffix(), getBitSize(), getIsSigned(), \
                  min.getValueRef<value_type>(), max.getValueRef<value_type >(), \
                  getIsStatic(), getCVQualifier()); \
}

DBG_DUMP_MACROS(TypeBool)
DBG_DUMP_MACROS(TypeSChar)
DBG_DUMP_MACROS(TypeUChar)
DBG_DUMP_MACROS(TypeSShort)
DBG_DUMP_MACROS(TypeUShort)
DBG_DUMP_MACROS(TypeSInt)
DBG_DUMP_MACROS(TypeUInt)
DBG_DUMP_MACROS(TypeSLong)
DBG_DUMP_MACROS(TypeULong)
DBG_DUMP_MACROS(TypeSLLong)
DBG_DUMP_MACROS(TypeULLong)

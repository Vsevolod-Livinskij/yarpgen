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

#include <climits>
#include <functional>

#include "type_enums.hpp"

namespace yarpgen {

ENUM_CLASS_WITH_STR(UB, NoUB,
                        NullPtr, // nullptr ptr dereference
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

    IRValue() : type_id(IntTypeID::MAX_INT_TYPE_ID), ub_code(UB::NoUB) { value.ullong_val = 0; }

    explicit IRValue(IntTypeID _type_id) : type_id(_type_id), ub_code(UB::NoUB) { value.ullong_val = 0; }

    template<typename T>
    T &getValueRef();

    IntTypeID getIntTypeID() { return type_id; }

    UB getUBCode() { return ub_code; }

    void setUBCode(UB _ub_code) { ub_code = _ub_code; }

private:
    IntTypeID type_id;
    Value value;
    UB ub_code;
};

template<>
bool &IRValue::getValueRef();

template<>
signed char &IRValue::getValueRef();

template<>
unsigned char &IRValue::getValueRef();

template<>
short &IRValue::getValueRef();

template<>
unsigned short &IRValue::getValueRef();

template<>
int &IRValue::getValueRef();

template<>
unsigned int &IRValue::getValueRef();

template<>
long long int &IRValue::getValueRef();

template<>
unsigned long long int &IRValue::getValueRef();

//////////////////////////////////////////////////////////////////////////////

#define OperatorWrapperCase(__id__, __foo__) \
        case (__id__): return __foo__;

#define OperatorWrapper(__foo__) \
    switch (type_id) { \
        OperatorWrapperCase(IntTypeID::INT, __foo__<TypeSInt::value_type>) \
        OperatorWrapperCase(IntTypeID::UINT, __foo__<TypeUInt::value_type>) \
        OperatorWrapperCase(IntTypeID::LONG, __foo__<TypeSLong::value_type>) \
        OperatorWrapperCase(IntTypeID::ULONG, __foo__<TypeULong::value_type>) \
        OperatorWrapperCase(IntTypeID::LLONG, __foo__<TypeSLLong::value_type>) \
        OperatorWrapperCase(IntTypeID::ULLONG, __foo__<TypeULLong::value_type>) \
        default: \
            ERROR("Bad IntTypeID value!"); \
    }

//////////////////////////////////////////////////////////////////////////////

IRValue plusOperator(IRValue &operand);

std::function<IRValue(IRValue&)> plusOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue minusOperator(IRValue &operand) {
    IRValue ret(operand.getIntTypeID());
    if (std::is_signed<T>::value && operand.getValueRef<T>() == std::numeric_limits<T>::min())
        ret.setUBCode(UB::SignOvf);
    else
        ret.getValueRef<T>() = -operand.getValueRef<T>();
    return ret;
}

std::function<IRValue(IRValue&)> minusOperatorWrapper(IntTypeID type_id);

std::function<IRValue(IRValue&)> logicalNegationOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue bitwiseNegationOperator(IRValue &operand) {
    IRValue ret(operand.getIntTypeID());
    ret.getValueRef<T>() = ~operand.getValueRef<T>();
    return ret;
}

std::function<IRValue(IRValue&)> bitwiseNegationOperatorWrapper(IntTypeID type_id);

//////////////////////////////////////////////////////////////////////////////

template<typename T>
typename std::enable_if<std::is_unsigned<T>::value, IRValue>::type addOperator(IRValue &lhs, IRValue &rhs) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());
    ret.getValueRef<T>() = lhs.getValueRef<T>() + rhs.getValueRef<T>();
    assert(ret.getUBCode() == UB::NoUB && "Addition of two unsigned values can't cause UB");
    return ret;
}

template<typename T>
typename std::enable_if<!std::is_unsigned<T>::value, IRValue>::type addOperator(IRValue &lhs, IRValue &rhs) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());
    using unsigned_T = typename std::make_unsigned<T>::type;
    auto ua = static_cast<unsigned_T>(lhs.getValueRef<T>());
    auto ub = static_cast<unsigned_T>(rhs.getValueRef<T>());
    unsigned_T u_tmp = ua + ub;
    ua = (ua >> std::numeric_limits<T>::digits) + std::numeric_limits<T>::max();
    if (static_cast<T>((ua ^ ub) | ~(ub ^ u_tmp)) >= 0)
        ret.setUBCode(UB::SignOvf);
    else
        ret.getValueRef<T>() = lhs.getValueRef<T>() + rhs.getValueRef<T>();
    return ret;
}

std::function<IRValue(IRValue&, IRValue&)> addOperatorWrapper(IntTypeID type_id);

template<typename T>
typename std::enable_if<std::is_unsigned<T>::value, IRValue>::type subOperator(IRValue &lhs, IRValue &rhs) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());
    ret.getValueRef<T>() = lhs.getValueRef<T>() - rhs.getValueRef<T>();
    assert(ret.getUBCode() == UB::NoUB && "Subtraction of two unsigned values can't cause UB");
    return ret;
}

template<typename T>
typename std::enable_if<!std::is_unsigned<T>::value, IRValue>::type subOperator(IRValue &lhs, IRValue &rhs) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());
    using unsigned_T = typename std::make_unsigned<T>::type;
    auto ua = static_cast<unsigned_T>(lhs.getValueRef<T>());
    auto ub = static_cast<unsigned_T>(rhs.getValueRef<T>());
    unsigned_T u_tmp = ua - ub;
    ua = (ua >> std::numeric_limits<T>::digits) + std::numeric_limits<T>::max();
    if (static_cast<T>((ua ^ ub) & (ua ^ u_tmp)) < 0)
        ret.setUBCode(UB::SignOvf);
    else
        ret.getValueRef<T>() = static_cast<T>(u_tmp);
    return ret;
}

std::function<IRValue(IRValue&, IRValue&)> subOperatorWrapper(IntTypeID type_id);

template <typename T>
static bool checkMulIsOk(T a, T b, IRValue& res) {
    // Special thanks to http://www.fefe.de/intof.html

    using unsigned_T = typename std::make_unsigned<T>::type;
    unsigned_T ret = 0;

    T sign = (((a > 0) && (b > 0)) || ((a < 0) && (b < 0))) ? 1 : -1;
    unsigned_T a_abs = std::abs(a);
    unsigned_T b_abs = std::abs(b);

    using unsigned_half_T = std::conditional_t<std::is_same<T, int32_t>::value, uint16_t, uint32_t>;
    unsigned_half_T half_all_one = (std::is_same<unsigned_half_T, uint32_t>::value) ? 0xFFFFFFFF : 0xFFFF;
    int32_t half_bit_size = sizeof(unsigned_half_T) * CHAR_BIT;
    unsigned_half_T a_low  = a_abs & half_all_one;
    unsigned_half_T b_low  = b_abs & half_all_one;
    unsigned_half_T a_high = a_abs >> half_bit_size;
    unsigned_half_T b_high = b_abs >> half_bit_size;

    if ((a_high != 0) && (b_high != 0))
        return false;

    unsigned_T tmp = (static_cast<unsigned_T>(a_high) * b_low) +
                     (static_cast<unsigned_T>(b_high) * a_low);
    if (tmp > half_all_one)
        return false;

    ret = (tmp << half_bit_size) + (static_cast<unsigned_T>(a_low) * b_low);
    if (ret < (tmp << half_bit_size))
        return false;

    if ((sign < 0) && (ret > static_cast<unsigned_T>(std::abs(std::numeric_limits<T>::min()))))
        return false;
    else if ((sign > 0) && (ret > std::numeric_limits<T>::max()))
        return false;
    else
        res.getValueRef<T>() = ret * sign;

    return true;
}

template<typename T>
typename std::enable_if<std::is_unsigned<T>::value, IRValue>::type mulOperator(IRValue &lhs, IRValue &rhs) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());
    // GCC doesn't like bool * bool. This function should be never called with T = bool,
    // so we need this kludge to suppress warning
    auto foo = std::multiplies<T>();
    ret.getValueRef<T>() = foo(lhs.getValueRef<T>(), rhs.getValueRef<T>());
    assert(ret.getUBCode() == UB::NoUB && "Multiplication of two unsigned values can't cause UB");
    return ret;
}

template<typename T>
typename std::enable_if<!std::is_unsigned<T>::value, IRValue>::type mulOperator(IRValue &lhs, IRValue &rhs) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());

    auto special_check = [] (IRValue &a, IRValue &b) -> bool{
        return a.getValueRef<T>() == std::numeric_limits<T>::min() && b.getValueRef<T>() == -1 ;
    };

    if (special_check(lhs, rhs) || special_check(rhs, lhs))
        ret.setUBCode(UB::SignOvfMin);
    else if (!checkMulIsOk<T>(lhs.getValueRef<T>(), rhs.getValueRef<T>(), ret))
        ret.setUBCode(UB::SignOvf);
    return ret;
}

std::function<IRValue(IRValue&, IRValue&)> mulOperatorWrapper(IntTypeID type_id);

template<typename T>
static typename std::enable_if<std::is_unsigned<T>::value, IRValue>::type
divModImpl(IRValue &lhs, IRValue &rhs, std::function<T(T&, T&)> op) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());
    if (rhs.getValueRef<T>() == 0)
        ret.setUBCode(UB::ZeroDiv);
    else
        ret.getValueRef<T>() = op(lhs.getValueRef<T>(), rhs.getValueRef<T>());
    return ret;
}

template<typename T>
static typename std::enable_if<!std::is_unsigned<T>::value, IRValue>::type
divModImpl(IRValue &lhs, IRValue &rhs, std::function<T(T&, T&)> op) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());

    auto special_check = [] (IRValue &a, IRValue &b) -> bool {
        return a.getValueRef<T>() == std::numeric_limits<T>::min() && b.getValueRef<T>() == -1 ;
    };

    if (rhs.getValueRef<T>() == 0)
        ret.setUBCode(UB::ZeroDiv);
    else if (special_check(lhs, rhs) || special_check(rhs, lhs))
        ret.setUBCode(UB::SignOvf);
    else
        ret.getValueRef<T>() = op(lhs.getValueRef<T>(), rhs.getValueRef<T>());
    return ret;
}

template<typename T>
IRValue divOperator(IRValue &lhs, IRValue &rhs) {
    return divModImpl<T>(lhs, rhs, std::divides<T>());
}

std::function<IRValue(IRValue&, IRValue&)> divOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue modOperator(IRValue &lhs, IRValue &rhs) {
    return divModImpl<T>(lhs, rhs, std::modulus<T>());
}

std::function<IRValue(IRValue&, IRValue&)> modOperatorWrapper(IntTypeID type_id);

template<typename T>
static IRValue cmpEqImpl(IRValue &lhs, IRValue &rhs, std::function<T(T&, T&)> op) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    //TODO: what about C?
    IRValue ret(IntTypeID::BOOL);
    ret.getValueRef<bool>() = op(lhs.getValueRef<T>(), rhs.getValueRef<T>());
    return ret;
}

template<typename T>
IRValue lessOperator(IRValue &lhs, IRValue &rhs) {
    return cmpEqImpl<T>(lhs, rhs, std::less<T>());
}

std::function<IRValue(IRValue&, IRValue&)> lessOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue greaterOperator(IRValue &lhs, IRValue &rhs) {
    return cmpEqImpl<T>(lhs, rhs, std::greater<T>());
}

std::function<IRValue(IRValue&, IRValue&)> greaterOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue lessEqualOperator(IRValue &lhs, IRValue &rhs) {
    return cmpEqImpl<T>(lhs, rhs, std::less_equal<T>());
}

std::function<IRValue(IRValue&, IRValue&)> lessEqualOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue greaterEqualOperator(IRValue &lhs, IRValue &rhs) {
    return cmpEqImpl<T>(lhs, rhs, std::greater_equal<T>());
}

std::function<IRValue(IRValue&, IRValue&)> greaterEqualOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue equalOperator(IRValue &lhs, IRValue &rhs) {
    return cmpEqImpl<T>(lhs, rhs, std::equal_to<T>());
}

std::function<IRValue(IRValue&, IRValue&)> equalOperatorWrapper(IntTypeID type_id);

template<typename T>
IRValue notEqualOperator(IRValue &lhs, IRValue &rhs) {
    return cmpEqImpl<T>(lhs, rhs, std::not_equal_to<T>());
}

std::function<IRValue(IRValue&, IRValue&)> notEqualOperatorWrapper(IntTypeID type_id);

std::function<IRValue(IRValue&, IRValue&)> logicalAndOperatorWrapper(IntTypeID type_id);

std::function<IRValue(IRValue&, IRValue&)> logicalOrOperatorWrapper(IntTypeID type_id);

template <typename T>
static IRValue bitwiseAndOrXorImpl(IRValue &lhs, IRValue &rhs, std::function<T(T&, T&)> op) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");

    IRValue ret(rhs.getIntTypeID());
    ret.getValueRef<T>() = op(lhs.getValueRef<T>(), rhs.getValueRef<T>());
    return ret;
}

template <typename T>
IRValue bitwiseAndOperator(IRValue &lhs, IRValue &rhs) {
    return bitwiseAndOrXorImpl<T>(lhs, rhs, std::bit_and<T>());
}

std::function<IRValue(IRValue&, IRValue&)> bitwiseAndOperatorWrapper(IntTypeID type_id);

template <typename T>
IRValue bitwiseOrOperator(IRValue &lhs, IRValue &rhs) {
    return bitwiseAndOrXorImpl<T>(lhs, rhs, std::bit_or<T>());
}

std::function<IRValue(IRValue&, IRValue&)> bitwiseOrOperatorWrapper(IntTypeID type_id);

template <typename T>
IRValue bitwiseXorOperator(IRValue &lhs, IRValue &rhs) {
    return bitwiseAndOrXorImpl<T>(lhs, rhs, std::bit_xor<T>());
}

std::function<IRValue(IRValue&, IRValue&)> bitwiseXorOperatorWrapper(IntTypeID type_id);

#define ShiftOperatorWrapperCaseCase(__id__, __foo__, __lhs_value_type__, __rhs_value_type__) \
        case (__id__): return __foo__<__lhs_value_type__, __rhs_value_type__>;

#define ShiftOperatorWrapperCase(__id__, __foo__, __lhs_value_type__) \
    case __id__: \
        switch (rhs_type_id) { \
            ShiftOperatorWrapperCaseCase(IntTypeID::INT, __foo__, __lhs_value_type__, TypeSInt::value_type) \
            ShiftOperatorWrapperCaseCase(IntTypeID::UINT, __foo__, __lhs_value_type__, TypeUInt::value_type) \
            ShiftOperatorWrapperCaseCase(IntTypeID::LONG, __foo__, __lhs_value_type__, TypeSLong::value_type) \
            ShiftOperatorWrapperCaseCase(IntTypeID::ULONG, __foo__, __lhs_value_type__, TypeULong::value_type) \
            ShiftOperatorWrapperCaseCase(IntTypeID::LLONG, __foo__, __lhs_value_type__, TypeSLLong::value_type) \
            ShiftOperatorWrapperCaseCase(IntTypeID::ULLONG, __foo__, __lhs_value_type__, TypeULLong::value_type) \
            default: \
                ERROR("Bad IntTypeID value!"); \
        }

#define ShiftOperatorWrapper(__foo__) \
    switch (lhs_type_id) { \
        ShiftOperatorWrapperCase(IntTypeID::INT, __foo__, TypeSInt::value_type) \
        ShiftOperatorWrapperCase(IntTypeID::UINT, __foo__, TypeUInt::value_type) \
        ShiftOperatorWrapperCase(IntTypeID::LONG, __foo__, TypeSLong::value_type) \
        ShiftOperatorWrapperCase(IntTypeID::ULONG, __foo__, TypeULong::value_type) \
        ShiftOperatorWrapperCase(IntTypeID::LLONG, __foo__, TypeSLLong::value_type) \
        ShiftOperatorWrapperCase(IntTypeID::ULLONG, __foo__, TypeULLong::value_type) \
        default: \
            ERROR("Bad IntTypeID value!"); \
    }

template <typename T, typename U>
IRValue shiftOperatorCommonChecks(IRValue&lhs, IRValue &rhs) {
    IRValue ret(lhs.getIntTypeID());

    if (std::is_signed<U>::value && rhs.getValueRef<U>() < 0) {
        ret.setUBCode(UB::ShiftRhsNeg);
        return ret;
    }

    size_t lhs_bit_size = sizeof(T) * CHAR_BIT;
    if (rhs.getValueRef<U>() >= static_cast<U>(lhs_bit_size)) {
        ret.setUBCode(UB::ShiftRhsLarge);
        return ret;
    }

    return ret;
}

// Find the most significant bit
template <typename T>
static inline size_t findMSB(T x) {
    //TODO: implementation-defined!
    if (std::is_signed<T>::value && x < 0)
        return sizeof(T) * CHAR_BIT;
    size_t ret = 0;
    while (x != 0) {
        ret++;
        x = x >> 1;
    }
    return ret;
}

template <typename T, typename U>
IRValue leftShiftOperator(IRValue&lhs, IRValue &rhs) {
    IRValue ret = shiftOperatorCommonChecks<T, U>(lhs, rhs);

    if (ret.getUBCode() != UB::NoUB)
        return ret;

    if (std::is_signed<T>::value && lhs.getValueRef<T>() < 0) {
        ret.setUBCode(UB::NegShift);
        return ret;
    }

    size_t lhs_bit_size = sizeof(T) * CHAR_BIT;
    if (std::is_signed<T>::value) {
        size_t max_avail_shft = lhs_bit_size - findMSB(lhs.getValueRef<T>());
        if (rhs.getValueRef<U>() > static_cast<U>(max_avail_shft)) {
            ret.setUBCode(UB::ShiftRhsLarge);
            return ret;
        }
    }

    assert(ret.getUBCode() == UB::NoUB && "Ret can't has a UB. All of the cases should be handled earlier.");
    ret.getValueRef<T>() = lhs.getValueRef<T>() << rhs.getValueRef<U>();
    return ret;
}

std::function<IRValue(IRValue&, IRValue&)> leftShiftOperatorWrapper(IntTypeID lhs_type_id, IntTypeID rhs_type_id);

template <typename T, typename U>
IRValue rightShiftOperator(IRValue&lhs, IRValue &rhs) {
    IRValue ret = shiftOperatorCommonChecks<T, U>(lhs, rhs);

    if (ret.getUBCode() != UB::NoUB)
        return ret;

    if (std::is_signed<T>::value && lhs.getValueRef<T>() < 0) {
        //TODO: it is implementation-defined!
        ret.setUBCode(UB::NegShift);
        return ret;
    }

    assert(ret.getUBCode() == UB::NoUB && "Ret can't has a UB. All of the cases should be handled earlier.");
    ret.getValueRef<T>() = lhs.getValueRef<T>() >> rhs.getValueRef<U>();
    return ret;
}

std::function<IRValue(IRValue&, IRValue&)> rightShiftOperatorWrapper(IntTypeID lhs_type_id, IntTypeID rhs_type_id);

}


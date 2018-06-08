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

#include <random>

#include "type_enums.hpp"
#include "type.hpp"
#include "ir_value.hpp"

using namespace yarpgen;

void type_test() {
    // Check for type initialization
    for (auto i = static_cast<int>(IntTypeID::BOOL); i < static_cast<int>(IntTypeID::MAX_INT_TYPE_ID); ++i)
        for (auto j = static_cast<int>(CVQualifier::NTHG); j <=static_cast<int>(CVQualifier::CONST_VOLAT); ++j)
            for (int k = false; k <= true; ++k) {
                std::shared_ptr<IntegralType> ptr_to_type = IntegralType::init(static_cast<IntTypeID>(i),
                                                                               static_cast<bool>(k),
                                                                               static_cast<CVQualifier>(j));
        }
    for (auto i = static_cast<int>(IntTypeID::BOOL); i < static_cast<int>(IntTypeID::MAX_INT_TYPE_ID); ++i)
        for (auto j = static_cast<int>(CVQualifier::NTHG); j <=static_cast<int>(CVQualifier::CONST_VOLAT); ++j)
            for (int k = false; k <= true; ++k) {
                std::shared_ptr<IntegralType> ptr_to_type = IntegralType::init(static_cast<IntTypeID>(i),
                                                                               static_cast<bool>(k),
                                                                               static_cast<CVQualifier>(j));
                ptr_to_type->dbgDump();
                std::cout << "-------------------" << std::endl;
            }
}

//static std::random_device rd;  //Will be used to obtain a seed for the random number engine
static std::mt19937 generator(0);

template <typename T>
void checkBinaryNoUB(IRValue &ret, IRValue &a, IRValue &b,
                     std::function<T(T &, T &)> foo, const std::string &test_name) {
    if (ret.getUBCode() != UB::NoUB ||
        ret.getValueRef<T>() != foo(a.getValueRef<T>(), b.getValueRef<T>())) {
        std::cout << "ERROR: " << test_name << typeid(T).name();
        std::cout << " ERROR NoUB: " << a.getValueRef<T>() << " | " << b.getValueRef<T>() << std::endl;
    }
}

template <typename T>
void checkBinaryForUB(IRValue &ret, IRValue &a, IRValue &b, const std::string &test_name) {
    if (ret.getUBCode() == UB::NoUB || ret.getValueRef<T>() != static_cast<T>(0)) {
        std::cout << "ERROR: " << test_name << typeid(T).name();
        std::cout << " ERROR UB: " << a.getValueRef<T>() << " | " << b.getValueRef<T>() << std::endl;
    }
}

template <typename T>
void checkUnaryNoUB(IRValue &ret, IRValue &a, std::function<T(T&)> foo, const std::string &test_name) {
    if (ret.getUBCode() != UB::NoUB ||
        ret.getValueRef<T>() != foo(a.getValueRef<T>())) {
        std::cout << "ERROR: " << test_name << typeid(T).name();
        std::cout << " ERROR NoUB: " << a.getValueRef<T>() << std::endl;
    }
}

template <typename T>
void checkUnaryForUB(IRValue &ret, IRValue &a, const std::string &test_name) {
    if (ret.getUBCode() == UB::NoUB || ret.getValueRef<T>() != static_cast<T>(0)) {
        std::cout << "ERROR: " << test_name << typeid(T).name();
        std::cout << " ERROR UB: " << a.getValueRef<T>() << std::endl;
    }
}

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, void>::type singleAddTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);
    auto distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    a.getValueRef<T>() = distr(generator);
    b.getValueRef<T>() = distr(generator);
    auto add_func = addOperatorWrapper(type_id);
    IRValue ret = add_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::plus<T>(), __FUNCTION__);
}

template <typename T>
typename std::enable_if<std::is_signed<T>::value, void>::type singleAddTest(IntTypeID type_id) {
    IRValue a (type_id);
    IRValue b (type_id);

    auto a_distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    a.getValueRef<T>() = a_distr(generator);

    // Determine if we want to test UB
    auto bool_distr = std::uniform_int_distribution<int>(0, 1);
    auto test_ub = static_cast<bool>(bool_distr(generator));

    T b_min = 0;
    T b_max = 0;

    if (a.getValueRef<T>() > 0) {
        if (test_ub) {
            b_min = std::numeric_limits<T>::max() - a.getValueRef<T>() + 1;
            b_max = std::numeric_limits<T>::max();
        }
        else {
            b_min = std::numeric_limits<T>::min();
            b_max = std::numeric_limits<T>::max() - a.getValueRef<T>();
        }
    }
    else if (a.getValueRef<T>() < 0) {
        if (test_ub) {
            b_min = std::numeric_limits<T>::min();
            b_max = std::numeric_limits<T>::min() - a.getValueRef<T>() - 1;
        }
        else {
            b_min = std::numeric_limits<T>::min() - a.getValueRef<T>() ;
            b_max = std::numeric_limits<T>::max();
        }
    }
    else {
        b_min = std::numeric_limits<T>::min();
        b_max = std::numeric_limits<T>::max();
    }

    assert (b_min < b_max);
    auto b_distr = std::uniform_int_distribution<T>(b_min, b_max);
    b.getValueRef<T>() = b_distr(generator);

    auto add_func = addOperatorWrapper(type_id);
    IRValue ret = add_func(a, b);
    if (test_ub)
        checkBinaryForUB<T>(ret, a, b, __FUNCTION__);
    else
        checkBinaryNoUB<T>(ret, a, b, std::plus<T>(), __FUNCTION__) ;
}

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, void>::type singleSubTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);
    auto distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    a.getValueRef<T>() = distr(generator);
    b.getValueRef<T>() = distr(generator);
    auto sub_func = subOperatorWrapper(type_id);
    IRValue ret = sub_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::minus<T>(), __FUNCTION__);
}

template <typename T>
typename std::enable_if<std::is_signed<T>::value, void>::type singleSubTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);

    auto a_distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    a.getValueRef<T>() = a_distr(generator);

    // Determine if we want to test UB
    auto bool_distr = std::uniform_int_distribution<int>(0, 1);
    auto test_ub = static_cast<bool>(bool_distr(generator));

    T b_min = 0;
    T b_max = 0;

    if (a.getValueRef<T>() > 0) {
        if (test_ub) {
            b_min = std::numeric_limits<T>::min();
            b_max = a.getValueRef<T>() - std::numeric_limits<T>::max() - 1;
        }
        else {
            b_min = a.getValueRef<T>() - std::numeric_limits<T>::max();
            b_max = std::numeric_limits<T>::max();
        }
    }
    else if (a.getValueRef<T>() < 0) {
        if (test_ub) {
            b_min = a.getValueRef<T>() - std::numeric_limits<T>::min() + 1;
            b_max = std::numeric_limits<T>::max();
        }
        else {
            b_min = std::numeric_limits<T>::min();
            b_max = a.getValueRef<T>() - std::numeric_limits<T>::min();
        }
    }
    else {
        b_min = std::numeric_limits<T>::min();
        b_max = std::numeric_limits<T>::max();
    }


    assert (b_min < b_max);
    auto b_distr = std::uniform_int_distribution<T>(b_min, b_max);
    b.getValueRef<T>() = b_distr(generator);

    auto sub_func = subOperatorWrapper(type_id);
    IRValue ret = sub_func(a, b);
    if (test_ub)
        checkBinaryForUB<T>(ret, a, b, __FUNCTION__);
    else
        checkBinaryNoUB<T>(ret, a, b, std::minus<T>(), __FUNCTION__);
}

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, void>::type singleMulTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);
    auto distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    a.getValueRef<T>() = distr(generator);
    b.getValueRef<T>() = distr(generator);
    auto mul_func = mulOperatorWrapper(type_id);
    IRValue ret = mul_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::multiplies<T>(), __FUNCTION__);
}

template <typename T>
typename std::enable_if<std::is_signed<T>::value, void>::type singleMulTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);

    auto min_max_distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    a.getValueRef<T>() = min_max_distr(generator);

    // Determine if we want to test UB
    auto bool_distr = std::uniform_int_distribution<int>(0, 1);
    auto test_ub = static_cast<bool>(bool_distr(generator));

    if (a.getValueRef<T>() == static_cast<T>(0)) {
        b.getValueRef<T>() = min_max_distr(generator);
        test_ub = false;
    }
    else if (a.getValueRef<T>() == static_cast<T>(-1)) {
        if (test_ub) {
            b.getValueRef<T>() = std::numeric_limits<T>::min();
        }
        else {
            auto distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min() + 1,
                                                          std::numeric_limits<T>::max());
            b.getValueRef<T>() = distr(generator);
        }
    }
    else {
        if (test_ub) {
            b.getValueRef<T>() = min_max_distr(generator);
        }
        else {
            b.getValueRef<T>() = min_max_distr(generator) / a.getValueRef<T>();
        }
    }

    auto mul_func = mulOperatorWrapper(type_id);
    IRValue ret = mul_func(a, b);
    if (test_ub)
        checkBinaryForUB<T>(ret, a, b, __FUNCTION__);
    else
        checkBinaryNoUB<T>(ret, a, b, std::multiplies<T>(), __FUNCTION__);
}

template <typename T>
void singleDivModTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);

    // Determine if we want to test UB
    auto bool_distr = std::uniform_int_distribution<int>(0, 1);
    auto test_ub = static_cast<bool>(bool_distr(generator));

    auto distr = std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());


    if (test_ub) {
        if (std::is_signed<T>::value && bool_distr(generator)) {
            a.getValueRef<T>() = std::numeric_limits<T>::min();
            b.getValueRef<T>() = -1;
        }
        else {
            a.getValueRef<T>() = distr(generator);
            b.getValueRef<T>() = 0;
        }
    }
    else {
        a.getValueRef<T>() = distr(generator);
        b.getValueRef<T>() = distr(generator);
        if ((std::is_signed<T>::value && a.getValueRef<T>() == std::numeric_limits<T>::min() &&
            b.getValueRef<T>() == static_cast<T>(-1)) || b.getValueRef<T>() == 0)
            test_ub = true;
    }

    auto div_func = divOperatorWrapper(type_id);
    IRValue ret = div_func(a, b);
    if (test_ub)
        checkBinaryForUB<T>(ret, a, b, __FUNCTION__);
    else
        checkBinaryNoUB<T>(ret, a, b, std::divides<T>(), __FUNCTION__);

    auto mod_func = modOperatorWrapper(type_id);
    ret = mod_func(a, b);
    if (test_ub)
        checkBinaryForUB<T>(ret, a, b, __FUNCTION__);
    else
        checkBinaryNoUB<T>(ret, a, b, std::modulus<T>(), __FUNCTION__);
}

template <typename T>
static typename std::enable_if<std::is_same<T, bool>::value, std::uniform_int_distribution<int>>::type getDistribution () {
    return std::uniform_int_distribution<int>(0, 1);
}

template <typename T>
static typename std::enable_if<!std::is_same<T, bool>::value, std::uniform_int_distribution<T>>::type getDistribution () {
    return std::uniform_int_distribution<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
}

template <typename T>
void singleCmpTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);

    auto bool_distr = std::uniform_int_distribution<int>(0, 1);
    auto distr = getDistribution<T>();
    a.getValueRef<T>() = static_cast<T>(distr(generator));
    b.getValueRef<T>() = static_cast<T>(distr(generator));

    if (bool_distr(generator))
        b.getValueRef<T>() = a.getValueRef<T>();

    auto less_func = lessOperatorWrapper(type_id);
    IRValue ret = less_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::less<T>(), __FUNCTION__);

    auto greater_func = greaterOperatorWrapper(type_id);
    ret = greater_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::greater<T>(), __FUNCTION__);

    auto less_equal_func = lessEqualOperatorWrapper(type_id);
    ret = less_equal_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::less_equal<T>(), __FUNCTION__);

    auto greater_equal_func = greaterEqualOperatorWrapper(type_id);
    ret = greater_equal_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::greater_equal<T>(), __FUNCTION__);

    auto equal_func = equalOperatorWrapper(type_id);
    ret = equal_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::equal_to<T>(), __FUNCTION__);

    auto not_equal_func = notEqualOperatorWrapper(type_id);
    ret = not_equal_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::not_equal_to<T>(), __FUNCTION__);
}

void singleLogicalAndOrTest() {
    IRValue a(IntTypeID::BOOL);
    IRValue b(IntTypeID::BOOL);

    auto bool_distr = std::uniform_int_distribution<int>(0, 1);
    a.getValueRef<bool>() = static_cast<bool>(bool_distr(generator));
    b.getValueRef<bool>() = static_cast<bool>(bool_distr(generator));

    auto logical_and_func = logicalAndOperatorWrapper(IntTypeID::BOOL);
    IRValue ret = logical_and_func(a, b);
    checkBinaryNoUB<bool>(ret, a, b, std::logical_and<>(), __FUNCTION__);

    auto logical_or_func = logicalOrOperatorWrapper(IntTypeID::BOOL);
    ret = logical_or_func(a, b);
    checkBinaryNoUB<bool>(ret, a, b, std::logical_or<>(), __FUNCTION__);
}

template <typename T>
void singleBitwiseAndOrXorTest(IntTypeID type_id) {
    IRValue a(type_id);
    IRValue b(type_id);

    auto distr = getDistribution<T>();
    a.getValueRef<T>() = static_cast<T>(distr(generator));
    b.getValueRef<T>() = static_cast<T>(distr(generator));

    auto bit_and_func = bitwiseAndOperatorWrapper(type_id);
    IRValue ret = bit_and_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::bit_and<T>(), __FUNCTION__);

    auto bit_or_func = bitwiseOrOperatorWrapper(type_id);
    ret = bit_or_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::bit_or<T>(), __FUNCTION__);

    auto bit_xor_func = bitwiseXorOperatorWrapper(type_id);
    ret = bit_xor_func(a, b);
    checkBinaryNoUB<T>(ret, a, b, std::bit_xor<T>(), __FUNCTION__);
}

template <typename LT, typename RT>
void singleLeftRightShiftTest(IntTypeID lhs_type_id, IntTypeID rhs_type_id) {
    IRValue a(lhs_type_id);
    IRValue b(rhs_type_id);

    auto a_distr = getDistribution<LT>();
    auto b_distr = getDistribution<RT>();

    auto check_func = [] (IRValue& a, IRValue& b, IRValue& ret) {
        if (ret.getUBCode() == UB::NoUB || ret.getValueRef<LT>() != static_cast<LT>(0)) {
            std::cout << "ERROR: " << __FUNCTION__ << typeid(LT).name();
            std::cout << "ERROR 1: " << a.getValueRef<LT>() << " | " << b.getValueRef<RT>() << std::endl;
        }
    };

    // Rhs is too large (exceeds size of type)
    a.getValueRef<LT>() = a_distr(generator);
    b_distr = std::uniform_int_distribution<RT>(sizeof(LT) * CHAR_BIT, std::numeric_limits<RT>::max());
    b.getValueRef<RT>() = b_distr(generator);
    auto left_shift_func = leftShiftOperatorWrapper(lhs_type_id, rhs_type_id);
    IRValue ret = left_shift_func(a, b);
    check_func (a, b, ret);
    auto right_shift_func = rightShiftOperatorWrapper(lhs_type_id, rhs_type_id);
    ret = right_shift_func(a, b);
    check_func (a, b, ret);

    // Rhs is negative
    if (std::is_signed<RT>::value) {
        a.getValueRef<LT>() = a_distr(generator);
        b_distr = std::uniform_int_distribution<RT>(std::numeric_limits<RT>::min(), -1);
        b.getValueRef<RT>() = b_distr(generator);
        ret = left_shift_func(a, b);
        check_func (a, b, ret);
        ret = right_shift_func(a, b);
        check_func (a, b, ret);
    }

    // Lhs is negative
    if (std::is_signed<LT>::value) {
        a_distr = std::uniform_int_distribution<LT>(std::numeric_limits<LT>::min(), -1);
        a.getValueRef<LT>() = a_distr(generator);
        b.getValueRef<RT>() = b_distr(generator);
        ret = left_shift_func(a, b);
        check_func (a, b, ret);
        ret = right_shift_func(a, b);
        check_func (a, b, ret);
    }

    size_t lhs_bit_size = sizeof(LT) * CHAR_BIT;
    // Lhs is signed and Rhs is too large (moves msb(lhs) out of type representation)
    if (std::is_signed<LT>::value) {
        a.getValueRef<LT>() = a_distr(generator);
        size_t max_avail_shft = lhs_bit_size - findMSB(a.getValueRef<LT>());
        b_distr = std::uniform_int_distribution<RT>(static_cast<RT>(max_avail_shft), std::numeric_limits<RT>::max());
        b.getValueRef<RT>() = b_distr(generator);
        ret = left_shift_func(a, b);
        check_func (a, b, ret);
    }

    a_distr = std::uniform_int_distribution<LT>(0, std::numeric_limits<LT>::max());
    a.getValueRef<LT>() = a_distr(generator);
    RT b_max = static_cast<RT>(lhs_bit_size) - 1;
    b_distr = std::uniform_int_distribution<RT>(0, b_max);
    b.getValueRef<RT>() = b_distr(generator);
    ret = right_shift_func(a, b);
    if (ret.getUBCode() != UB::NoUB ||
        ret.getValueRef<LT>() != (a.getValueRef<LT>() >> b.getValueRef<RT>())) {
        std::cout << "ERROR: " << __FUNCTION__ << typeid(LT).name();
        std::cout << "ERROR 2: " << a.getValueRef<LT>() << " | " << b.getValueRef<RT>() << std::endl;
    }


    if (std::is_signed<LT>::value)
        b_max = lhs_bit_size - findMSB(a.getValueRef<LT>());
    b_distr = std::uniform_int_distribution<RT>(0, b_max);
    b.getValueRef<RT>() = b_distr(generator);

    ret = left_shift_func(a, b);
    if (ret.getUBCode() != UB::NoUB ||
        ret.getValueRef<LT>() != (a.getValueRef<LT>() << b.getValueRef<RT>())) {
        std::cout << "ERROR: " << __FUNCTION__ << typeid(LT).name();
        std::cout << "ERROR 2: " << a.getValueRef<LT>() << " | " << b.getValueRef<RT>() << std::endl;
    }
}

template <typename T>
void singlePlusBitwiseNegateTest(IntTypeID type_id) {
    IRValue a(type_id);

    auto distr = getDistribution<T>();
    a.getValueRef<T>() = static_cast<T>(distr(generator));

    auto plus_func = plusOperatorWrapper(type_id);
    IRValue ret = plus_func(a);
    auto check_plus_foo = [](T &a) -> T { return +a; };
    checkUnaryNoUB<T>(ret, a, check_plus_foo, __FUNCTION__);

    auto bitwise_not_func = bitwiseNegationOperatorWrapper(type_id);
    ret = bitwise_not_func(a);
    auto check_bit_neg_foo = [](T &a) -> T { return ~a; };
    checkUnaryNoUB<T>(ret, a, check_bit_neg_foo, __FUNCTION__);
}

template <typename T>
void singleMinusTest(IntTypeID type_id) {
    IRValue a(type_id);

    auto bool_distr = getDistribution<bool>();
    auto distr = getDistribution<T>();
    a.getValueRef<T>() = static_cast<T>(distr(generator));

    auto test_ub = std::is_signed<T>::value && bool_distr(generator);

    if (test_ub)
        a.getValueRef<T>() = std::numeric_limits<T>::min();
    else
        a.getValueRef<T>() = static_cast<T>(distr(generator));

    auto minus_func = minusOperatorWrapper(type_id);
    IRValue ret = minus_func(a);

    if (test_ub)
        checkUnaryForUB<T>(ret, a, __FUNCTION__);
    else
        checkUnaryNoUB<T>(ret, a, std::negate<T>(), __FUNCTION__);
}

void singleLogicalNegateTest() {
    IRValue a(IntTypeID::BOOL);

    auto distr = getDistribution<bool>();
    a.getValueRef<bool>() = static_cast<bool>(distr(generator));

    auto logical_not_func = logicalNegationOperatorWrapper(IntTypeID::BOOL);
    IRValue ret = logical_not_func(a);
    auto check_plus_foo = [](bool &a) -> bool { return !a; };
    checkUnaryNoUB<bool>(ret, a, check_plus_foo, __FUNCTION__);
}

void ir_value_test() {
    int test_num = 10000000;

    for (int i = 0; i < test_num; ++i) {
        singleAddTest<TypeSInt::value_type>(IntTypeID::INT);
        singleAddTest<TypeUInt::value_type>(IntTypeID::UINT);
        singleAddTest<TypeSLong::value_type>(IntTypeID::LONG);
        singleAddTest<TypeULong::value_type>(IntTypeID::ULONG);
        singleAddTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singleAddTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleSubTest<TypeSInt::value_type>(IntTypeID::INT);
        singleSubTest<TypeUInt::value_type>(IntTypeID::UINT);
        singleSubTest<TypeSLong::value_type>(IntTypeID::LONG);
        singleSubTest<TypeULong::value_type>(IntTypeID::ULONG);
        singleSubTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singleSubTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleMulTest<TypeSInt::value_type>(IntTypeID::INT);
        singleMulTest<TypeUInt::value_type>(IntTypeID::UINT);
        singleMulTest<TypeSLong::value_type>(IntTypeID::LONG);
        singleMulTest<TypeULong::value_type>(IntTypeID::ULONG);
        singleMulTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singleMulTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleDivModTest<TypeSInt::value_type>(IntTypeID::INT);
        singleDivModTest<TypeUInt::value_type>(IntTypeID::UINT);
        singleDivModTest<TypeSLong::value_type>(IntTypeID::LONG);
        singleDivModTest<TypeULong::value_type>(IntTypeID::ULONG);
        singleDivModTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singleDivModTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleCmpTest<TypeSInt::value_type>(IntTypeID::INT);
        singleCmpTest<TypeUInt::value_type>(IntTypeID::UINT);
        singleCmpTest<TypeSLong::value_type>(IntTypeID::LONG);
        singleCmpTest<TypeULong::value_type>(IntTypeID::ULONG);
        singleCmpTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singleCmpTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleLogicalAndOrTest();
    }

    for (int i = 0; i < test_num; ++i) {
        singleBitwiseAndOrXorTest<TypeSInt::value_type>(IntTypeID::INT);
        singleBitwiseAndOrXorTest<TypeUInt::value_type>(IntTypeID::UINT);
        singleBitwiseAndOrXorTest<TypeSLong::value_type>(IntTypeID::LONG);
        singleBitwiseAndOrXorTest<TypeULong::value_type>(IntTypeID::ULONG);
        singleBitwiseAndOrXorTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singleBitwiseAndOrXorTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleLeftRightShiftTest<TypeSInt::value_type, TypeSInt::value_type>(IntTypeID::INT, IntTypeID::INT);
        singleLeftRightShiftTest<TypeSInt::value_type, TypeUInt::value_type>(IntTypeID::INT, IntTypeID::UINT);
        singleLeftRightShiftTest<TypeSInt::value_type, TypeSLong::value_type>(IntTypeID::INT, IntTypeID::LONG);
        singleLeftRightShiftTest<TypeSInt::value_type, TypeULong::value_type>(IntTypeID::INT, IntTypeID::ULONG);
        singleLeftRightShiftTest<TypeSInt::value_type, TypeSLLong::value_type>(IntTypeID::INT, IntTypeID::LLONG);
        singleLeftRightShiftTest<TypeSInt::value_type, TypeULLong::value_type>(IntTypeID::INT, IntTypeID::ULLONG);

        singleLeftRightShiftTest<TypeUInt::value_type, TypeSInt::value_type>(IntTypeID::UINT, IntTypeID::INT);
        singleLeftRightShiftTest<TypeUInt::value_type, TypeUInt::value_type>(IntTypeID::UINT, IntTypeID::UINT);
        singleLeftRightShiftTest<TypeUInt::value_type, TypeSLong::value_type>(IntTypeID::UINT, IntTypeID::LONG);
        singleLeftRightShiftTest<TypeUInt::value_type, TypeULong::value_type>(IntTypeID::UINT, IntTypeID::ULONG);
        singleLeftRightShiftTest<TypeUInt::value_type, TypeSLLong::value_type>(IntTypeID::UINT, IntTypeID::LLONG);
        singleLeftRightShiftTest<TypeUInt::value_type, TypeULLong::value_type>(IntTypeID::UINT, IntTypeID::ULLONG);

        singleLeftRightShiftTest<TypeSLong::value_type, TypeSInt::value_type>(IntTypeID::LONG, IntTypeID::INT);
        singleLeftRightShiftTest<TypeSLong::value_type, TypeUInt::value_type>(IntTypeID::LONG, IntTypeID::UINT);
        singleLeftRightShiftTest<TypeSLong::value_type, TypeSLong::value_type>(IntTypeID::LONG, IntTypeID::LONG);
        singleLeftRightShiftTest<TypeSLong::value_type, TypeULong::value_type>(IntTypeID::LONG, IntTypeID::ULONG);
        singleLeftRightShiftTest<TypeSLong::value_type, TypeSLLong::value_type>(IntTypeID::LONG, IntTypeID::LLONG);
        singleLeftRightShiftTest<TypeSLong::value_type, TypeULLong::value_type>(IntTypeID::LONG, IntTypeID::ULLONG);

        singleLeftRightShiftTest<TypeULong::value_type, TypeSInt::value_type>(IntTypeID::ULONG, IntTypeID::INT);
        singleLeftRightShiftTest<TypeULong::value_type, TypeUInt::value_type>(IntTypeID::ULONG, IntTypeID::UINT);
        singleLeftRightShiftTest<TypeULong::value_type, TypeSLong::value_type>(IntTypeID::ULONG, IntTypeID::LONG);
        singleLeftRightShiftTest<TypeULong::value_type, TypeULong::value_type>(IntTypeID::ULONG, IntTypeID::ULONG);
        singleLeftRightShiftTest<TypeULong::value_type, TypeSLLong::value_type>(IntTypeID::ULONG, IntTypeID::LLONG);
        singleLeftRightShiftTest<TypeULong::value_type, TypeULLong::value_type>(IntTypeID::ULONG, IntTypeID::ULLONG);

        singleLeftRightShiftTest<TypeSLLong::value_type, TypeSInt::value_type>(IntTypeID::LLONG, IntTypeID::INT);
        singleLeftRightShiftTest<TypeSLLong::value_type, TypeUInt::value_type>(IntTypeID::LLONG, IntTypeID::UINT);
        singleLeftRightShiftTest<TypeSLLong::value_type, TypeSLong::value_type>(IntTypeID::LLONG, IntTypeID::LONG);
        singleLeftRightShiftTest<TypeSLLong::value_type, TypeULong::value_type>(IntTypeID::LLONG, IntTypeID::ULONG);
        singleLeftRightShiftTest<TypeSLLong::value_type, TypeSLLong::value_type>(IntTypeID::LLONG, IntTypeID::LLONG);
        singleLeftRightShiftTest<TypeSLLong::value_type, TypeULLong::value_type>(IntTypeID::LLONG, IntTypeID::ULLONG);

        singleLeftRightShiftTest<TypeULLong::value_type, TypeSInt::value_type>(IntTypeID::ULLONG, IntTypeID::INT);
        singleLeftRightShiftTest<TypeULLong::value_type, TypeUInt::value_type>(IntTypeID::ULLONG, IntTypeID::UINT);
        singleLeftRightShiftTest<TypeULLong::value_type, TypeSLong::value_type>(IntTypeID::ULLONG, IntTypeID::LONG);
        singleLeftRightShiftTest<TypeULLong::value_type, TypeULong::value_type>(IntTypeID::ULLONG, IntTypeID::ULONG);
        singleLeftRightShiftTest<TypeULLong::value_type, TypeSLLong::value_type>(IntTypeID::ULLONG, IntTypeID::LLONG);
        singleLeftRightShiftTest<TypeULLong::value_type, TypeULLong::value_type>(IntTypeID::ULLONG, IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singlePlusBitwiseNegateTest<TypeSInt::value_type>(IntTypeID::INT);
        singlePlusBitwiseNegateTest<TypeUInt::value_type>(IntTypeID::UINT);
        singlePlusBitwiseNegateTest<TypeSLong::value_type>(IntTypeID::LONG);
        singlePlusBitwiseNegateTest<TypeULong::value_type>(IntTypeID::ULONG);
        singlePlusBitwiseNegateTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singlePlusBitwiseNegateTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleMinusTest<TypeSInt::value_type>(IntTypeID::INT);
        singleMinusTest<TypeUInt::value_type>(IntTypeID::UINT);
        singleMinusTest<TypeSLong::value_type>(IntTypeID::LONG);
        singleMinusTest<TypeULong::value_type>(IntTypeID::ULONG);
        singleMinusTest<TypeSLLong::value_type>(IntTypeID::LLONG);
        singleMinusTest<TypeULLong::value_type>(IntTypeID::ULLONG);
    }

    for (int i = 0; i < test_num; ++i) {
        singleLogicalNegateTest();
    }
}
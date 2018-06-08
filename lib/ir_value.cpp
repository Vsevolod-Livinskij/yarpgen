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

#include "type.hpp"

using namespace yarpgen;

template <>
bool& IRValue::getValueRef() {
    return value.bool_val;
}

template <>
signed char & IRValue::getValueRef() {
    return value.schar_val;
}

template <>
unsigned char & IRValue::getValueRef() {
    return value.uchar_val;
}

template <>
short & IRValue::getValueRef() {
    return value.shrt_val;
}

template <>
unsigned short & IRValue::getValueRef() {
    return value.ushrt_val;
}

template <>
int & IRValue::getValueRef() {
    return value.int_val;
}

template <>
unsigned int & IRValue::getValueRef() {
    return value.uint_val;
}

template <>
long long int & IRValue::getValueRef() {
    return value.llong_val;
}

template <>
unsigned long long int & IRValue::getValueRef() {
    return value.ullong_val;
}

IRValue yarpgen::plusOperator(IRValue &operand) {
    return {operand};
}

std::function<IRValue(IRValue&)> yarpgen::plusOperatorWrapper(IntTypeID type_id) {
    return plusOperator;
}

std::function<IRValue(IRValue&)> yarpgen::minusOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(minusOperator);
}

IRValue logicalNegationOperator(IRValue &operand) {
    IRValue ret(operand.getIntTypeID());
    ret.getValueRef<bool>() = !operand.getValueRef<bool>();
    return ret;
}

std::function<IRValue(IRValue&)> yarpgen::logicalNegationOperatorWrapper(IntTypeID type_id) {
    return logicalNegationOperator;
}

std::function<IRValue(IRValue&)> yarpgen::bitwiseNegationOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(bitwiseNegationOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::addOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(addOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::subOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(subOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::mulOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(mulOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::divOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(divOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::modOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(modOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::lessOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(lessOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::greaterOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(greaterOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::lessEqualOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(lessEqualOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::greaterEqualOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(greaterEqualOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::equalOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(equalOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::notEqualOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(notEqualOperator);
}

static IRValue logicalAndOrImpl(IRValue &lhs, IRValue &rhs, std::function<bool(bool&, bool&)> op) {
    if (rhs.getIntTypeID() != lhs.getIntTypeID())
        ERROR("Can perform operation only on IRValues with the same IntTypeID");
    if (lhs.getIntTypeID() != IntTypeID::BOOL)
        ERROR("Logical And/Or expressions are defined only for bool values");

    //TODO: what about C?
    IRValue ret(IntTypeID::BOOL);
    ret.getValueRef<bool>() = op(lhs.getValueRef<bool>(), rhs.getValueRef<bool>());
    return ret;
}

IRValue logicalAndOperator(IRValue &lhs, IRValue &rhs) {
    return logicalAndOrImpl(lhs, rhs, std::logical_and<>());
}

IRValue logicalOrOperator(IRValue &lhs, IRValue &rhs) {
    return logicalAndOrImpl(lhs, rhs, std::logical_or<>());
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::logicalAndOperatorWrapper(IntTypeID type_id) {
    return logicalAndOperator;
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::logicalOrOperatorWrapper(IntTypeID type_id) {
    return logicalOrOperator;
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::bitwiseAndOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(bitwiseAndOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::bitwiseOrOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(bitwiseOrOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::bitwiseXorOperatorWrapper(IntTypeID type_id) {
    OperatorWrapper(bitwiseXorOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::leftShiftOperatorWrapper(IntTypeID lhs_type_id,
                                                                             IntTypeID rhs_type_id) {
    ShiftOperatorWrapper(leftShiftOperator);
}

std::function<IRValue(IRValue&, IRValue&)> yarpgen::rightShiftOperatorWrapper(IntTypeID lhs_type_id,
                                                                              IntTypeID rhs_type_id) {
    ShiftOperatorWrapper(rightShiftOperator);
}


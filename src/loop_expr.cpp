/*
Copyright (c) 2015-2016, Intel Corporation

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

#include "loop_expr.h"

using namespace rl;


IDX::IDX (int64_t i) {
    this->shift = i;
}


IDX::IDX (std::shared_ptr<IterVar> it_) {
    this->shift = 0;
    this->it = it_;
    assert(this->it.use_count() != 0);
}


IDX::IDX (std::shared_ptr<IterVar> it_, int64_t i) {
    this->shift = i;
    this->it = it_;
}


std::shared_ptr<Expr> IDX::getExprPtr () {
    std::shared_ptr<Expr> to_return;
    if (this->it.use_count() == 0) {
        AtomicType::ScalarTypedVal val (Type::IntegerTypeID::INT);
        val.val.int_val = this->shift;
        to_return = std::make_shared<ConstExpr>(val);
    }
    else if (this->shift == 0)
        to_return = std::make_shared<VarUseExpr>(this->it);
    else {
        AtomicType::ScalarTypedVal val (Type::IntegerTypeID::INT);
        val.val.int_val = this->shift;
        to_return = std::make_shared<BinaryExpr>(BinaryExpr::Op::Add, std::make_shared<VarUseExpr>(this->it),
                                                 std::make_shared<ConstExpr>(val));
    }
    if (this->idx.use_count() == 0)
        return to_return;
    return std::make_shared<BinaryExpr>(this->idx_op, to_return, this->idx->getExprPtr());
}


IDX& IDX::operator+=(int64_t rhs) {
    this->shift += rhs;
    return *this;
}


IDX& IDX::operator-=(int64_t rhs) {
    this->shift -= rhs;
    return *this;
}


IDX& IDX::operator+=(IDX rhs) {
    this->shift += rhs.getShift();
    rhs -= rhs.getShift();
    if (this->idx.use_count() == 0){
        this->idx_op = BinaryExpr::Op::Add;
        this->idx = std::make_shared<IDX>(rhs.copy());
    }
    else {
        if(this->idx_op == BinaryExpr::Op::Add)
            *(this->idx) += rhs;
        if(this->idx_op == BinaryExpr::Op::Sub)
            *(this->idx) -= rhs;
    }
    return *this;
}


IDX& IDX::operator-=(IDX rhs) {
    this->shift -= rhs.getShift();
    rhs -= rhs.getShift();
    if (this->idx.use_count() == 0){
        this->idx_op = BinaryExpr::Op::Sub;
        this->idx = std::make_shared<IDX>(rhs.copy());
    }
    else {
        if(this->idx_op == BinaryExpr::Op::Add)
            *(this->idx) -= rhs;
        if(this->idx_op == BinaryExpr::Op::Sub)
            *(this->idx) += rhs;
    }
    return *this;
}


int64_t IDX::getStep(std::shared_ptr<Context> ctx) {
    int64_t step = 0;
    if (this->it.use_count() != 0)
        step += this->it->getStep(ctx);
    if (this->idx.use_count() != 0)
        if (this->idx_op == BinaryExpr::Op::Add)
            step += this->idx->getStep(ctx);
        if (this->idx_op == BinaryExpr::Op::Sub)
            step -= this->idx->getStep(ctx);
    return step;
}


IDX IDX::copy() {
    IDX ret(this->it, this->shift);
    if (this->idx.use_count() != 0) {
        if (this->idx_op == BinaryExpr::Op::Add)
            ret += this->idx->copy();
        if (this->idx_op == BinaryExpr::Op::Sub)
            ret -= this->idx->copy();
    }
    return ret;
}


void IDX::clearup_tree() {
    while ((this->idx.use_count() != 0) && this->idx->isDummy()){
        if (this->idx->getChildSign() == this->idx_op)
            this->idx_op = BinaryExpr::Op::Add;
        else
            this->idx_op = BinaryExpr::Op::Sub;
        this->idx = this->idx->getChild();
    }
    if (this->idx.use_count() != 0)
        this->idx->clearup_tree();
}

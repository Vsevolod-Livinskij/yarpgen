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

#pragma once

#include "typedefs.h"
#include "loop_types.h"
#include "expr.h"

namespace rl {


class IDX {
protected:
    // IDX = it + shift +/- IDX'
    std::shared_ptr<IDX> idx;
    BinaryExpr::Op idx_op;
    std::shared_ptr<IterVar> it;
    int64_t shift;

public:
    explicit IDX (int64_t i);
    explicit IDX (std::shared_ptr<IterVar> it_);
    explicit IDX (std::shared_ptr<IterVar> it_, int64_t i);

    virtual int64_t getShift() { return this->shift; }
    virtual std::shared_ptr<IDX> getChild() { return this->idx; }
    virtual BinaryExpr::Op getChildSign() { return this->idx_op; }

    // Some indexes do not have iterators in them, and some do
    // like a[0] and a[i + 3]
    virtual std::shared_ptr<Expr> getExprPtr ();

    IDX& operator+=(int64_t rhs);
    IDX& operator-=(int64_t rhs);
    IDX& operator+=(IDX rhs);
    IDX& operator-=(IDX rhs);

    virtual IDX copy();
    virtual int64_t getStep(std::shared_ptr<Context> ctx);
    virtual bool isDummy() { return ((this->it.use_count() == 0) && (this->shift == 0)); }

    virtual void clearup_tree();
};
}

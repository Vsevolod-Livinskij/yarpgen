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
#include "variable.h"
#include "probability.h"


namespace rl {

class IDX;
class GenPolicy;

class IterVar : public ScalarVariable {
protected:
    std::map<uint64_t, int64_t> step;

public:
    IterVar (IntegerType::IntegerTypeID ty_);
    IterVar (std::string n_, IntegerType::IntegerTypeID ty_);
    IterVar (std::shared_ptr<GenPolicy> gen_policy);
    virtual void setStep(std::shared_ptr<Context> ctx, int64_t step_);
    virtual int64_t getStep(std::shared_ptr<Context> ctx);
};


class VecElem {
public:
    enum Purpose {
        NONE,
        TO_INIT,
        RONLY,
        WONLY
    };

    enum Kind {
        C_ARR,
        STD_VEC,
        STD_VARR,
        STD_ARR
    };

    enum AccessKind {
        REGULAR,
        AT,
        PTR,
        ITERPTR
    };

protected:
    // Some data is common for every element in array (like type or name). So remember who is your
    // parent to restore the info and at the same time save some memory.
    VecElem *parent;

    // The list of indexes to access current cell (like [i+1], [j], [k-234], ...)
    std::vector<IDX> ids;

    // The kind of array we are ging ti emit
    // C_ARR: int a[42] ... - c - style array
    // STD_VEC:  std::vector
    // STD_VARR: std::valarray
    // STD::ARR: std::array
    Kind kind;

    // This ... solution will help to determine the purpose this vector exists for.
    // This is a major drawback of our speculation, that all input vectors have the same value.
    // Uninitialized read is UB. The TO_INIT vector is a vector bound to undergo *fuint64_t* initialization,
    // meaning that *all* cells are set with a specific value determined earlier at the creation of the vector.
    // After vector is initialized, it is set to be RONLY, so that noone mess with the data. RONLY cells
    // have the same value for all cells. The WONLY vecs are like TO_INIT, but there is no guarantee
    // that all cells are initialized with the same values. No cell shall be left uninitialized (FIXME?)
    // or in the end we cannot check the final result.
    Purpose purpose;

public:
    virtual Purpose getPurpose() { return this->purpose; }
    virtual Kind getKind() { return this->kind; }
    virtual void setPurpose(Purpose pps_) { this->purpose = pps_; }

    virtual std::string info() { return "none"; }
    virtual bool is_vector() = 0;
    virtual bool is_scalar() = 0;
    virtual bool equals(std::shared_ptr<VecElem> v) { return (this->get_name() == v->get_name()); }
    virtual std::shared_ptr<VecElem> copy() = 0;
    virtual std::shared_ptr<ScalarVariable> getData() = 0;

    virtual std::shared_ptr<IDX> getID (uint64_t level = 0);
    virtual void addID(IDX id_);

    virtual AccessKind getAccessKind () {
        assert(this->parent != NULL);
        return this->parent->getAccessKind();
    }

    virtual std::shared_ptr<GenPolicy> getGenPolicy () {
        assert(this->parent != NULL);
        return this->parent->getGenPolicy();
    }

    virtual IntegerType::IntegerTypeID get_type_id () {
        assert(this->parent != NULL);
        return this->parent->get_type_id();
    }

    virtual std::shared_ptr<Type> get_type () {
        assert(this->parent != NULL);
        return this->parent->get_type();
    }

    virtual std::string get_name () {
        assert(this->parent != NULL);
        return this->parent->get_name();
    }

    virtual std::shared_ptr<ScalarVariable> getRawVar() {
        assert(this->parent != NULL);
        return this->parent->getRawVar();
    }

    virtual AtomicType::ScalarTypedVal get_value() {
        assert(this->parent != NULL);
        return this->parent->get_value();
    }

    virtual AtomicType::ScalarTypedVal getValue() { return this->getRawVar()->get_cur_value(); }

    virtual VecElem* getParent() { return this->parent; }
    virtual void setParent(VecElem *p_) { this->parent = p_; }
};


class Cell : public VecElem {
public:
    Cell (VecElem *p_);
    Cell (IDX id_, VecElem *p_);

    virtual std::shared_ptr<VecElem> copy() override;
    virtual std::shared_ptr<ScalarVariable> getData() override;

    virtual bool is_vector() override { return false; }
    virtual bool is_scalar() override { return true; }
};


/** \brief A helper class to ease up passing vector ranges around.
 */
struct RANGE {
public:
    uint64_t l;
    uint64_t r;
    RANGE () {}
    RANGE (uint64_t l_, uint64_t r_) : l(l_), r(r_) {}
};

class Vector : public VecElem {
protected:
    std::string name;
    IntegerType::IntegerTypeID type_id;
    std::vector<std::shared_ptr<VecElem>> cells;

    // This variable stores values for all vector cells
    // If <purpose> is set to TO_INIT, the value equals to the desired vector value;
    // if <purpose> is set to RONLY, the correct init code have been emitted for all
    // cells in vector; if <purpose> is WONLY, the value does not matter
    std::shared_ptr<ScalarVariable> base_value;
    std::shared_ptr<GenPolicy> gen_policy;

    std::vector<Probability<AccessKind>> access_probabilities;

public:
    std::vector<uint64_t> dim;

    Vector (uint64_t n, VecElem *p_);
    Vector (std::vector<uint64_t> dim, VecElem *p_);
    Vector (uint64_t n, std::string n_, IntegerType::IntegerTypeID ty_,
            Kind knd_, Purpose pps_, AtomicType::ScalarTypedVal val);
    Vector (std::vector<uint64_t> dim, std::string n_, IntegerType::IntegerTypeID ty_,
            Kind knd_, Purpose pps_, AtomicType::ScalarTypedVal val);
    Vector (std::shared_ptr<GenPolicy> gen_policy_, Purpose pps_);

    virtual IntegerType::IntegerTypeID get_type_id () override { return this->type_id; }
    virtual std::shared_ptr<Type> get_type () override {
        return IntegerType::init(this->type_id);
    }
    virtual std::string get_name () override { return this->name; }

    virtual std::shared_ptr<VecElem> copy() override;
    virtual std::shared_ptr<ScalarVariable> getData() override;

    virtual bool is_vector() override { return true; }
    virtual bool is_scalar() override { return false; }

    virtual std::string info() override;
    virtual void addCell(std::shared_ptr<VecElem> c_) { this->cells.push_back(c_); }

    virtual std::vector<std::shared_ptr<VecElem>> range (uint64_t left, uint64_t rght);
    virtual std::vector<std::shared_ptr<VecElem>> range (RANGE rng) {
        return this->range(rng.l, rng.r);
    }

    virtual void setPurpose(Purpose pps_) override {
        for(auto cell : this->cells) cell->setPurpose(pps_);
        this->purpose = pps_;
    }

    virtual AccessKind getAccessKind () override {
        if(this->parent != NULL) return this->parent->getAccessKind();
        return rand_val_gen->get_rand_id(this->access_probabilities);
    }

    virtual AtomicType::ScalarTypedVal get_value() override {
        assert(this->base_value.use_count() != 0);
        return this->base_value->get_cur_value();
    }

    virtual std::shared_ptr<GenPolicy> getGenPolicy () override {
        return this->gen_policy;
    }

    virtual void register_iter (std::shared_ptr<IterVar> it);
    virtual void register_iter (IDX idx);
    virtual uint64_t size() {
        return this->cells.size();
    }

    virtual std::shared_ptr<ScalarVariable> getRawVar() override {
        assert(this->base_value.use_count() != 0);
        return this->base_value;
    }

    virtual std::shared_ptr<VecElem>& operator[](uint64_t idx) {
        assert(idx < this->size());
        return this->cells[idx];
    }

private:
    void init_multidim(std::vector<uint64_t> dim);
};


class Cluster : public Vector {
public:
    std::vector<uint64_t> overlaps;
    Cluster (std::vector<std::shared_ptr<VecElem>> data, 
             uint64_t soverlap = 0,
             std::string n_ = "noname cluster");

    virtual std::shared_ptr<IDX> getID (uint64_t level = 0) override;
};
}

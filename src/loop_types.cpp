#include "type.h"/*
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

#include "loop_types.h"
#include "namegen.h"
#include "loop_expr.h"
#include "sym_table.h"


using namespace rl;


IterVar::IterVar (IntegerType::IntegerTypeID ty_)
        : ScalarVariable ("", IntegerType::init(ty_)) {
    this->name = ItNameGen::getName();
}


IterVar::IterVar (std::string n_, IntegerType::IntegerTypeID ty_)
        : ScalarVariable (n_, IntegerType::init(ty_)) {}


IterVar::IterVar (std::shared_ptr<GenPolicy> gen_policy)
        : ScalarVariable ("", IntegerType::init(IntegerType::IntegerTypeID::INT)) {
    this->name = ItNameGen::getName();
    Context tmp_ctx (*gen_policy, NULL, Node::NodeID::MAX_STMT_ID, true);
    //TODO: originally, the type was INT. Is it important? //LOOP_MERGE
    std::shared_ptr<ScalarVariable> init_variable =
            ScalarVariable::generate(std::make_shared<Context>(tmp_ctx));
    this->type = init_variable->get_type();
    this->set_min(init_variable->get_min());
    this->set_max(init_variable->get_max());
    this->set_init_value(init_variable->get_init_value());
    this->set_cur_value(init_variable->get_cur_value());
}


void IterVar::setStep(std::shared_ptr<Context> ctx, int64_t step_) {
    this->step[ctx->getScopeId()] = step_;
}


int64_t IterVar::getStep(std::shared_ptr<Context> ctx) {
    auto search = this->step.find(ctx->getScopeId());
    if(search != this->step.end()) {
        return search->second;
    }
    return 0;
}


std::string Vector::info() {
    std::string tmp_ss;
    VecElem *p = this;
    while (p != NULL) {
        if (p->getID(0).use_count() != 0)
            tmp_ss = "[" + p->getID(0)->getExprPtr()->emit() + "]" + tmp_ss;
        p = p->getParent();
    }

    std::stringstream ss;
    ss << this->get_name() << tmp_ss << ": ";
    if (this->is_scalar())
        ss << "scalar";
    if (this->is_vector())
        ss << "vector";
        if (this->cells[0]->is_scalar())
            ss << " of scalars";
        if (this->cells[0]->is_vector())
            ss << " of vectors";
        ss << ", size = " << this->size();

    ss << ", PURPOSE: ";
    switch (this->purpose) {
        case TO_INIT: ss << "TO_INIT"; break;
        case RONLY:   ss << "RONLY"; break;
        case WONLY:   ss << "WONLY"; break;
        default: ss << "error";
    }

    ss << ", type: '" << this->base_value->get_type()->get_name();
    ss << "', val: ";

    //TODO: improve it!
    switch(this->type_id) {
        case IntegerType::IntegerTypeID::BOOL:
            ss << (bool) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::CHAR:
            ss << (signed char) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::UCHAR:
            ss << (unsigned char) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::SHRT:
            ss << (short) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::USHRT:
            ss << (unsigned short) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::INT:
            ss << (int) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::UINT:
            ss << (unsigned int) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::LINT:
            ss << (long int) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ss << (unsigned long int) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::LLINT:
            ss << (long long int) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            ss << (unsigned long long int) this->base_value->get_cur_value().get_abs_val();
            break;
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            default: ss << "faulty type id!";
    }

    return ss.str();
}


void VecElem::addID(IDX id_) {
    this->ids.push_back(id_);
}


std::shared_ptr<IDX> VecElem::getID (uint64_t level) {
    if (this->ids.size() == 0)
        return std::shared_ptr<IDX>();

    if (level > this->ids.size() - 1)
        return std::make_shared<IDX>(this->ids[0]);
    return std::make_shared<IDX>(this->ids[this->ids.size() - 1 - level]);
}


Cell::Cell (VecElem *p_) {
    assert(p_ != NULL);
    this->parent = p_;
    this->purpose = this->parent->getPurpose();
}


Cell::Cell (IDX id_, VecElem *p_) {
    assert(p_ != NULL);
    this->parent = p_;
    this->purpose = this->parent->getPurpose();
    this->addID(id_);
}


std::shared_ptr<VecElem> Cell::copy() {
    std::shared_ptr<Cell> new_cell = std::make_shared<Cell>(this->parent);
    for (auto id_ : this->ids)
        new_cell->addID(id_);
    return new_cell;
}


std::shared_ptr<ScalarVariable> Cell::getData() {
    assert(this->parent != NULL);
    std::string ss;

    std::shared_ptr<ScalarVariable> ret = std::make_shared<ScalarVariable>(*(this->parent->getData()));

    //TODO: move this part to separate function
    switch (this->getAccessKind ()) {
    case REGULAR : ss = ret->get_name() + "[" + this->getID(0)->getExprPtr()->emit() + "]"; break;
    case PTR     : ss = "(*(" + this->getID(0)->getExprPtr()->emit() + " + &(*" + ret->get_name() + ")))"; break;
    case AT      : ss = ret->get_name() + ".at(" + this->getID(0)->getExprPtr()->emit() + ")"; break;
    case ITERPTR : ss = "(*(" + this->getID(0)->getExprPtr()->emit() + " + " + ret->get_name() + ".begin()))"; break;
    }

    ret->set_name(ss);
    return ret;
}


Vector::Vector (uint64_t n, VecElem *p_) {
    assert(p_ != NULL);
    this->parent = p_;
    this->type_id = this->parent->get_type_id();
    this->name = this->parent->get_name();
    this->kind = this->parent->getKind();
    this->purpose = this->parent->getPurpose();
    this->base_value = this->parent->getRawVar();
    std::vector<uint64_t> dim_(1, n);
    this->init_multidim(dim_);
}


Vector::Vector (std::vector<uint64_t> dim_, VecElem *p_) {
    assert(p_ != NULL);
    this->parent = p_;
    this->type_id = this->parent->get_type_id();
    this->name = this->parent->get_name();
    this->kind = this->parent->getKind();
    this->purpose = this->parent->getPurpose();
    this->base_value = this->parent->getRawVar();
    this->init_multidim(dim_);
}


Vector::Vector (uint64_t n, std::string n_, IntegerType::IntegerTypeID ty_,
                Kind knd_, Purpose pps_, AtomicType::ScalarTypedVal val) {
    this->parent = NULL;
    this->type_id = ty_;
    this->name = n_;
    this->kind = knd_;
    this->purpose = pps_;
    this->base_value = std::make_shared<ScalarVariable> (this->name, IntegerType::init(this->get_type_id()));
    //TODO: it should be init value or current value? //LOOP_MERGE
    this->base_value->set_init_value(val);
    std::vector<uint64_t> dim_(1, n);
    this->init_multidim(dim_);
}


Vector::Vector (std::vector<uint64_t> dim_, std::string n_, IntegerType::IntegerTypeID ty_,
                Kind knd_, Purpose pps_, AtomicType::ScalarTypedVal val) {
    this->parent = NULL;
    this->type_id = ty_;
    this->name = n_;
    this->kind = knd_;
    this->purpose = pps_;
    this->base_value = std::make_shared<ScalarVariable> (this->name, IntegerType::init(this->get_type_id()));
    //TODO: it should be init value or current value? //LOOP_MERGE
    this->base_value->set_init_value(val);
    this->init_multidim(dim_);
}


Vector::Vector (std::shared_ptr<GenPolicy> gen_policy_, Purpose pps_) {
    this->parent = NULL;
    this->gen_policy = gen_policy_;
    assert(this->gen_policy.use_count() != 0);
    Context tmp_ctx (*gen_policy, NULL, Node::NodeID::MAX_STMT_ID, true);
    this->name = VecNameGen::getName();
    this->kind = Kind(rand_val_gen->get_rand_id(this->gen_policy->get_arr_kind_prob()));
    this->purpose = pps_;
    this->base_value = ScalarVariable::generate(std::make_shared<Context>(tmp_ctx));
    this->base_value->set_name(this->name);
    this->type_id = this->base_value->get_type()->get_int_type_id();

    uint64_t total_cells_left = rand_val_gen->get_rand_value<uint64_t>(this->gen_policy->get_min_array_size(),
                                                                 this->gen_policy->get_max_array_size());

    uint64_t min_dim_size = 2;
    uint64_t max_dim_size = this->gen_policy->get_max_array_size() / 100;
    assert(max_dim_size > 2);

    std::vector<uint64_t> dim_;
    uint64_t current_dim_cells = 0;
    while (total_cells_left > min_dim_size) {
        current_dim_cells = rand_val_gen->get_rand_value<uint64_t>(min_dim_size, std::min(total_cells_left, max_dim_size));
        dim_.push_back(current_dim_cells);
        total_cells_left = total_cells_left / current_dim_cells;
    }

    this->init_multidim(dim_);
}


void Vector::init_multidim(std::vector<uint64_t> dim_) {
    assert(dim_.size() > 0);
    this->dim = dim_;
    uint64_t mysize = dim_[0];

    if (this->getGenPolicy().use_count() != 0) {
        this->access_probabilities.push_back(Probability<AccessKind>(AccessKind::REGULAR,
                            this->getGenPolicy()->get_access_type_score(AccessKind::REGULAR)));
        if (this->getKind() == Kind::C_ARR) {
            this->access_probabilities.push_back(Probability<AccessKind>(AccessKind::PTR,
                                this->getGenPolicy()->get_access_type_score(AccessKind::PTR)));
        }
        else if ((this->getKind() == Kind::STD_VEC) || (this->getKind() == Kind::STD_ARR)) {
            this->access_probabilities.push_back(Probability<AccessKind>(AccessKind::AT,
                                this->getGenPolicy()->get_access_type_score(AccessKind::AT)));
            this->access_probabilities.push_back(Probability<AccessKind>(AccessKind::ITERPTR,
                                this->getGenPolicy()->get_access_type_score(AccessKind::ITERPTR)));
        }
    }
    else {
        this->access_probabilities.push_back(Probability<AccessKind>(AccessKind::REGULAR, 100));
    }

    if (mysize == 0) return;
    this->cells.reserve(mysize);

    if (dim_.size() == 1) {
        for (uint64_t i = 0; i < mysize; ++i) {
            this->cells.push_back(std::make_shared<Cell>(IDX(i), this));
        }
    }
    else {
        dim_.erase(dim_.begin());
        for (uint64_t i = 0; i < mysize; ++i) {
            this->cells.push_back(std::make_shared<Vector>(dim_, this));
            this->cells[i]->addID(IDX(i));
        }
    }
}


std::shared_ptr<VecElem> Vector::copy() {
    std::shared_ptr<Vector> new_vec = std::make_shared<Vector>(0, this->name, this->type_id,
                                                               this->kind, this->purpose, this->getValue());
    new_vec->setParent(this->parent);
    for (auto id_ : this->ids)
        new_vec->addID(id_);
    new_vec->dim = this->dim;
    for (auto c_ : this->cells) {
        std::shared_ptr<VecElem> new_subcell = c_->copy();
        new_subcell->setParent(new_vec.get());
        new_vec->addCell(new_subcell);
    }
    return new_vec;
}


std::shared_ptr<ScalarVariable> Vector::getData() {
    std::string ss;
    std::shared_ptr<ScalarVariable> ret;

    if (this->parent != NULL) ret = std::make_shared<ScalarVariable>(*(this->parent->getData()));
    else return this->getRawVar();

    //TODO: Move this part to separate function
    switch (this->getAccessKind ()) {
    case REGULAR : ss = ret->get_name() + "[" + this->getID(0)->getExprPtr()->emit() + "]"; break;
    case PTR     : ss = "(*(" + this->getID(0)->getExprPtr()->emit() + " + &(*" + ret->get_name() + ")))"; break;
    case AT      : ss = ret->get_name() + ".at(" + this->getID(0)->getExprPtr()->emit() + ")"; break;
    case ITERPTR : ss = "(*(" + this->getID(0)->getExprPtr()->emit() + " + " + ret->get_name() + ".begin()))"; break;
    }

    ret->set_name(ss);
    return ret;
}


std::vector<std::shared_ptr<VecElem>> Vector::range (uint64_t left, uint64_t rght) {
    std::vector<std::shared_ptr<VecElem>> rng;
    assert(left <= rght);
    assert(rght < this->cells.size());
    for (uint64_t i = left; i <= rght; ++i) {
        rng.push_back(this->cells[i]->copy());
    }
    return rng;
}


void Vector::register_iter (std::shared_ptr<IterVar> it) {
    for (uint64_t i = 0; i < this->cells.size(); ++i) {
        this->cells[i]->addID(IDX(it, i));
    }
}


void Vector::register_iter (IDX idx) {
    for (int64_t i = 0; i < this->cells.size(); ++i) {
        IDX tmp = idx;
        tmp += i;
        this->cells[i]->addID(tmp);
    }
}


Cluster::Cluster (std::vector<std::shared_ptr<VecElem>> data, uint64_t soverlap, std::string n_)
        : Vector(0, n_, data[0]->get_type_id(),
                 data[0]->getKind(), data[0]->getPurpose(), data[0]->getValue()) {
    this->cells = data;
    for (uint64_t i = 0; i < data.size(); ++i)
        this->overlaps.push_back(1);

    assert(soverlap <= data.size());
    for (uint64_t i = soverlap; i < data.size() - soverlap; ++i)
        this->overlaps[i] = 0;
}


std::shared_ptr<IDX> Cluster::getID (uint64_t level) {
    return this->cells[0]->getID(level);
}

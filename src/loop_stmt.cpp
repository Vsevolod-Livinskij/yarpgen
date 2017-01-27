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

#include "loop_stmt.h"
#include "namegen.h"
#include "master.h"

using namespace rl;

//TODO: Replace with vector of UseExpr
RInitStmtNVC::RInitStmtNVC(std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<ScalarVariable>> in,
                           std::shared_ptr<ScalarVariable> out)
            : Stmt(Node::NodeID::R_INIT_STMT_NVC) {
    std::vector<std::shared_ptr<Expr>> inp_exprs;
    for (auto var : in)
        inp_exprs.push_back(std::make_shared<VarUseExpr>(var));
    this->lhs = out;
    this->rhs = ArithExpr::generate(ctx, inp_exprs);
    assert(this->rhs->get_value()->get_class_id() == Data::VarClassID::VAR);
    AtomicType::ScalarTypedVal rhs_val = std::static_pointer_cast<ScalarVariable>(this->rhs->get_value())->get_cur_value();

    // Normalize rhs to 'out' value
    std::shared_ptr<ConstExpr> lhs_const = std::make_shared<ConstExpr>(this->lhs->get_cur_value());
    std::shared_ptr<ConstExpr> rhs_const = std::make_shared<ConstExpr>(rhs_val);
    this->rhs = std::make_shared<BinaryExpr>(BinaryExpr::Op::Sub, this->rhs, rhs_const);
    this->rhs = std::make_shared<BinaryExpr>(BinaryExpr::Op::Add, this->rhs, lhs_const);
    std::shared_ptr<VarUseExpr> lhs_expr = std::make_shared<VarUseExpr>(this->lhs);
    this->result = std::make_shared<ExprStmt>(std::make_shared<AssignExpr>(lhs_expr, this->rhs));
}


std::string RInitStmtNVC::info() {
    std::stringstream ss;
    ss << this->lhs->get_name() << " = ";
    assert(this->rhs->get_value()->get_class_id() == Data::VarClassID::VAR);
    AtomicType::ScalarTypedVal rhs_val = std::static_pointer_cast<ScalarVariable>(this->rhs->get_value())->get_cur_value();
    std::shared_ptr<ConstExpr> rhs_const = std::make_shared<ConstExpr>(rhs_val);
    ss << rhs_const->emit();
    ss << ";";
    return ss.str();
}


IterDeclStmt::IterDeclStmt (std::shared_ptr<Context> ctx, IntegerType::IntegerTypeID ty_, int64_t step_)
            : DeclStmt(NULL, NULL) {
    this->data = std::make_shared<IterVar>(ty_);
    this->data->setStep(ctx, step_);
}


IterDeclStmt::IterDeclStmt (std::shared_ptr<Context> ctx, IntegerType::IntegerTypeID ty_, int64_t step_,
                                                            std::shared_ptr<IDX> in_)
            : DeclStmt(NULL, NULL) {
    this->data = std::make_shared<IterVar>(ty_);
    this->data->setStep(ctx, step_);
    this->init = in_;
    data->set_init_value(std::static_pointer_cast<ScalarVariable>(init->getExprPtr()->get_value())->get_cur_value());
}


IterDeclStmt::IterDeclStmt (std::shared_ptr<Context> ctx, std::string n_, IntegerType::IntegerTypeID ty_, int64_t step_)
            : DeclStmt(NULL, NULL) {
    this->data = std::make_shared<IterVar>(n_, ty_);
    this->data->setStep(ctx, step_);
}


IterDeclStmt::IterDeclStmt (std::shared_ptr<Context> ctx, std::string n_, IntegerType::IntegerTypeID ty_, int64_t step_,
                                                            std::shared_ptr<IDX> in_)
            : DeclStmt(NULL, NULL) {
    this->data = std::make_shared<IterVar>(n_, ty_);
    this->data->setStep(ctx, step_);
    this->init = in_;
    data->set_init_value(std::static_pointer_cast<ScalarVariable>(init->getExprPtr()->get_value())->get_cur_value());
}


IterDeclStmt::IterDeclStmt (std::shared_ptr<Context> ctx, int64_t step_)
            : DeclStmt(NULL, NULL) {
    this->data = std::make_shared<IterVar>(ctx->get_gen_policy());
    this->data->setStep(ctx, step_);
    //TODO: What about init value and type of data? //LOOP_MERGE
    this->init = std::make_shared<IDX>(rand_val_gen->get_rand_value<int64_t>(0, 100));
    data->set_init_value(std::static_pointer_cast<ScalarVariable>(init->getExprPtr()->get_value())->get_cur_value());
}


IterDeclStmt::IterDeclStmt (std::shared_ptr<Context> ctx, int64_t step_, std::shared_ptr<IDX> in_)
            : DeclStmt(NULL, NULL) {
    this->data = std::make_shared<IterVar>(ctx->get_gen_policy());
    this->data->setStep(ctx, step_);
    this->init = in_;
    data->set_init_value(std::static_pointer_cast<ScalarVariable>(init->getExprPtr()->get_value())->get_cur_value());
}


std::shared_ptr<Expr> IterDeclStmt::getStepExpr (std::shared_ptr<Context> ctx) {
    std::shared_ptr<VarUseExpr> lhs = std::make_shared<VarUseExpr>(this->data);
    AtomicType::ScalarTypedVal rhs_const (Type::IntegerTypeID::INT);
    rhs_const.val.int_val = this->data->getStep(ctx);
    std::shared_ptr<ConstExpr> rhs = std::make_shared<ConstExpr>(rhs_const);
    std::shared_ptr<BinaryExpr> step_expr = std::make_shared<BinaryExpr>(BinaryExpr::Add, lhs, rhs);
    return step_expr;
}


std::string IterDeclStmt::emit (std::string offset) {
    std::stringstream ss;
    ss << offset;
    if (this->is_extern) ss << "extern ";
    ss << this->data->get_type()->get_name() << " " << this->data->get_name();

    if (this->init.use_count() != 0)
        ss << " = " << this->get_init()->emit();
    ss << ";";
    return ss.str();
}


VectorDeclStmt::VectorDeclStmt (std::shared_ptr<Context> ctx, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, uint64_t num_)
              : DeclStmt(NULL, NULL) {
    this->context = ctx;
    AtomicType::ScalarTypedVal val = AtomicType::ScalarTypedVal::generate(ctx, ty_);
    this->data = std::make_shared<Vector>(num_, VecNameGen::getName(), ty_, knd_, VecElem::Purpose::TO_INIT, val);
}


VectorDeclStmt::VectorDeclStmt (std::shared_ptr<Context> ctx, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, std::vector<uint64_t> dim)
              : DeclStmt(NULL, NULL) {
    this->context = ctx;
    AtomicType::ScalarTypedVal val = AtomicType::ScalarTypedVal::generate(ctx, ty_);
    this->data = std::make_shared<Vector>(dim, VecNameGen::getName(), ty_, knd_, VecElem::Purpose::TO_INIT, val);
}


VectorDeclStmt::VectorDeclStmt (std::shared_ptr<Context> ctx, std::string n_, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, uint64_t num_)
              : DeclStmt(NULL, NULL) {
    this->context = ctx;
    AtomicType::ScalarTypedVal val = AtomicType::ScalarTypedVal::generate(ctx, ty_);
    this->data = std::make_shared<Vector>(num_, n_, ty_, knd_, VecElem::Purpose::TO_INIT, val);
}


VectorDeclStmt::VectorDeclStmt (std::shared_ptr<Context> ctx, std::string n_, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, std::vector<uint64_t> dim)
              : DeclStmt(NULL, NULL) {
    this->context = ctx;
    AtomicType::ScalarTypedVal val = AtomicType::ScalarTypedVal::generate(ctx, ty_);
    this->data = std::make_shared<Vector>(dim, n_, ty_, knd_, VecElem::Purpose::TO_INIT, val);
}


VectorDeclStmt::VectorDeclStmt (std::shared_ptr<Context> ctx)
              : DeclStmt(NULL, NULL) {
    this->context = ctx;
    this->data = std::make_shared<Vector>(ctx->get_gen_policy(), VecElem::Purpose::TO_INIT);
}


std::shared_ptr<Stmt> VectorDeclStmt::getInitStmt() {
    // For the external init block any context will do
    GenPolicy gp;
    gp.set_lue_prob(0);
    Context ctx_obj (gp, NULL, Node::NodeID::MAX_STMT_ID, true);
    std::shared_ptr<Context> ctx = std::make_shared<Context>(ctx_obj);
    if ((this->data->getKind() == VecElem::Kind::C_ARR) || (this->data->getKind() == VecElem::Kind::STD_ARR)) {
        std::shared_ptr<ForEachStmt> ret = std::make_shared<ForEachStmt>(this->get_data());
        ret->add_single_stmt(std::make_shared<ExprStmt>(std::make_shared<AssignExpr>(
            std::make_shared<VarUseExpr>(ret->getVar()),
            std::make_shared<ConstExpr>(ret->getVar()->get_cur_value()))));
        return ret;
    }
    if ((this->data->getKind() == VecElem::Kind::STD_VEC) || (this->data->getKind() == VecElem::Kind::STD_VARR)) {
        std::shared_ptr<IterDeclStmt> it = std::make_shared<IterDeclStmt>(ctx, IntegerType::IntegerTypeID::ULLINT, 1, std::make_shared<IDX>(0));
        std::shared_ptr<HeaderStmt> ret = std::make_shared<HeaderStmt>(ctx, it, 1); // Do not have 'scope' class yet
        std::shared_ptr<HeaderStmt> hdr = ret;
        std::string var_name = this->data->get_name();

        for (uint64_t i = 0; i < this->data->dim.size() - 1; ++i) {
            std::stringstream ss;
            ss << var_name << ".resize(" << this->data->dim[i] << ");";
            hdr->add(std::make_shared<StubStmt>(ss.str()));
            it = std::make_shared<IterDeclStmt>(ctx, IntegerType::IntegerTypeID::ULLINT, 1, std::make_shared<IDX>(0));
            std::shared_ptr<HeaderStmt> new_hdr = std::make_shared<HeaderStmt>(ctx, it, this->data->dim[i]);
            hdr->add(new_hdr);
            hdr = new_hdr;
            var_name = var_name + "[" + it->get_name() + "]";
        }

        std::stringstream ss;
        ss << var_name << ".resize(" << this->data->dim.back() << ", ";
        ConstExpr val (this->data->get_value());
        ss << val.emit();
        ss << ");";
        hdr->add(std::make_shared<StubStmt>(ss.str()));
        return ret;
    }

    return std::make_shared<StubStmt>("Unreachable Error!");
}


std::string VectorDeclStmt::emit (std::string offset) {
    std::stringstream ss;
    ss << offset;
    if (this->is_extern) ss << "extern ";

    if (this->data->getKind() == VecElem::Kind::C_ARR) {
        ss << this->data->get_type()->get_name()
            << " " << this->data->get_name();
        for (uint64_t i = 0; i < this->data->dim.size(); ++i)
            ss << "[" << this->data->dim[i] << "]";
    }
    if (this->data->getKind() == VecElem::Kind::STD_VEC) {
        Master::include_vector();
        for (uint64_t i = 0; i < this->data->dim.size(); ++i)
            ss << "std::vector<";
        ss << this->data->get_type()->get_name();
        for (uint64_t i = 0; i < this->data->dim.size(); ++i)
            ss << " >";
        ss << " " << this->data->get_name();
    }
    if (this->data->getKind() == VecElem::Kind::STD_VARR) {
        Master::include_valarray();
        for (uint64_t i = 0; i < this->data->dim.size(); ++i)
            ss << "std::valarray<";
        ss << this->data->get_type()->get_name();
        for (uint64_t i = 0; i < this->data->dim.size(); ++i)
            ss << " >";
        ss << " " << this->data->get_name();
    }
    if (this->data->getKind() == VecElem::Kind::STD_ARR) {
        Master::include_std_array();
        for (uint64_t i = 0; i < this->data->dim.size(); ++i)
            ss << "std::array<";
        ss << this->data->get_type()->get_name();
        for (int64_t i = this->data->dim.size() - 1; i >= 0; i--)
            ss << ", " <<  this->data->dim[i] << ">";
        ss << " " << this->data->get_name();
    }

    ss << ";";
    return ss.str();
}


HeaderStmt::HeaderStmt (std::shared_ptr<Context> ctx, std::shared_ptr<IterDeclStmt> it, std::shared_ptr<IDX> start_e, uint64_t tripcount)
          : Stmt (Node::NodeID::LHDR) {
    // i = start
    this->start = std::make_shared<AssignExpr>(std::make_shared<VarUseExpr>(it->get_data()), start_e->getExprPtr());
    // i = i + step
    this->step = std::make_shared<AssignExpr>(std::make_shared<VarUseExpr>(it->get_data()), it->getStepExpr(ctx));

    // i < start + tc * step
    AtomicType::ScalarTypedVal end_val (Type::IntegerTypeID::INT);
    end_val.val.int_val = tripcount * it->get_data()->getStep(ctx);
    std::shared_ptr<Expr> end_expr = std::make_shared<ConstExpr>(end_val);

    if (ctx->get_gen_policy()->do_loop_unknown_end()) {
        std::shared_ptr<ScalarVariable> var = std::make_shared<ScalarVariable> ("le_" + SclNameGen::getName(),
                                                    IntegerType::init(Type::IntegerTypeID::INT));
        AtomicType::ScalarTypedVal var_val (Type::IntegerTypeID::INT);
        var_val.val.int_val = tripcount * it->get_data()->getStep(ctx);
        //TODO: should it be init value or current value?//LOOP_MERGE
        var->set_init_value(var_val);
        ctx->get_extern_inp_sym_table()->add_variable(var);
        end_expr = std::make_shared<VarUseExpr>(var);
    }

    this->exit = std::make_shared<BinaryExpr>
        (BinaryExpr::Lt, std::make_shared<VarUseExpr>(it->get_data()), std::make_shared<BinaryExpr>
        (BinaryExpr::Op::Add, start_e->getExprPtr(), end_expr));
}


HeaderStmt::HeaderStmt (std::shared_ptr<Context> ctx, std::shared_ptr<IterDeclStmt> it, uint64_t tripcount)
          : Stmt (Node::NodeID::LHDR) {

    // int i = start
    this->decl = it;

    // i = i + step
    this->step = std::make_shared<AssignExpr>(std::make_shared<VarUseExpr>(it->get_data()), it->getStepExpr(ctx));

    // i < start + tc * step
    AtomicType::ScalarTypedVal end_val (Type::IntegerTypeID::INT);
    end_val.val.int_val = tripcount * it->get_data()->getStep(ctx);
    std::shared_ptr<Expr> end_expr = std::make_shared<ConstExpr>(end_val);

    if (ctx->get_gen_policy()->do_loop_unknown_end()) {
        std::shared_ptr<ScalarVariable> var = std::make_shared<ScalarVariable> ("le_" + SclNameGen::getName(),
                                                    IntegerType::init(Type::IntegerTypeID::INT));
        AtomicType::ScalarTypedVal var_val (Type::IntegerTypeID::INT);
        var_val.val.int_val = tripcount * it->get_data()->getStep(ctx);
        //TODO: should it be init value or current value?//LOOP_MERGE
        var->set_init_value(var_val);
        ctx->get_extern_inp_sym_table()->add_variable(var);
        end_expr = std::make_shared<VarUseExpr>(var);
    }

    assert(this->decl->get_init().use_count() != 0);
    this->exit = std::make_shared<BinaryExpr>
        (BinaryExpr::Op::Lt, std::make_shared<VarUseExpr>(it->get_data()), std::make_shared<BinaryExpr>
        (BinaryExpr::Op::Add, this->decl->get_init(), end_expr));
}


std::string HeaderStmt::emit (std::string offset) {
    std::stringstream ss;
    ss << offset << "for (";
    if (this->start.use_count() != 0)
        ss << this->start->emit() << ";";
    else {
        assert(this->decl->get_init().use_count() != 0);
        ss << this->decl->emit();
    }
    ss << " " << this->exit->emit();
    ss << "; " << this->step->emit();
    ss << ")";
    if (this->module.size() == 1) ss << "\n";
    else ss << " {\n";

    for (auto e : this->module) {
        if ((e->get_id() == Node::NodeID::CMNT) &&
            (std::static_pointer_cast<CommentStmt>(e)->is_dead()))
            continue;
        ss << e->emit(offset + "    ") << "\n";
    }

    if (this->module.size() != 1) ss << offset << "}";
    return ss.str();
}


ForEachStmt::ForEachStmt(std::shared_ptr<Vector> v)
           : Stmt(Node::NodeID::FOR_EACH){
    this->vector = v;
    assert(this->vector.use_count() != 0);
    std::stringstream v_name;
    v_name << v->get_name();
    for(uint64_t i = 0; i < this->vector->dim.size(); ++i)
        v_name << "[" << v->get_name() << "_it" << i << "]";
    this->var = std::make_shared<ScalarVariable>(*(this->vector->getData()));
    this->var->set_name(v_name.str());
}


std::string ForEachStmt::emit (std::string offset) {
    assert(this->todostmt.use_count() != 0);

    std::stringstream ss;
    std::stringstream ss_offset;

    ss_offset << offset;
    for (uint64_t i = 0; i < this->vector->dim.size(); ++i) {
        std::string i_name = this->vector->get_name() + "_it";
        ss << ss_offset.str() << "for(unsigned long long int " << i_name << i
           << " = 0; " << i_name << i << " < " << this->vector->dim[i]
           << "; ++" << i_name << i << ")\n";
        ss_offset << "    ";
    }

    ss << this->todostmt->emit(ss_offset.str());
    return ss.str();
}

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

#ifndef STMT_H
#define STMT_H

#include "typedefs.h"
#include "loop_types.h"
#include "loop_expr.h"

#include "stmt.h"
#include "variable.h"
#include "sym_table.h"

namespace rl {


class RInitStmtNVC : public Stmt {
protected:
    std::shared_ptr<Stmt> result;
    std::shared_ptr<Expr> rhs;
    std::shared_ptr<ScalarVariable> lhs;

public:
    RInitStmtNVC(std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<ScalarVariable>> in,
                 std::shared_ptr<ScalarVariable> out);
    virtual std::string emit (std::string offset = "") { return this->result->emit(offset); }
    virtual std::string info();
};


class CommentStmt : public Stmt {
protected:
    std::string comment;
    bool dead;

public:
    CommentStmt (std::shared_ptr<Context> ctx, std::string s_, uint64_t level = 0) : Stmt (Node::NodeID::CMNT) {
        if (ctx->get_verbose_level() > level) { set_dead(); return; }
        if (s_ == "") this->comment = "";
        else this->comment = "/* " + s_ + " */";
    }

    virtual void set_dead() { this->dead = true; }
    virtual bool is_dead () { return this->dead; }

    virtual std::string emit(std::string offset = "") {
        int64_t current_pos = 0;
        while ((current_pos < this->comment.size()) && (current_pos != std::string::npos)) {
            current_pos ++;
            current_pos = this->comment.find_first_of("\n", current_pos);
            this->comment.insert(current_pos + 1, offset);
        }
        return this->comment;
    }
};


class IterDeclStmt : public DeclStmt {
private:
    std::shared_ptr<IterVar> data;
    std::shared_ptr<IDX> init;

public:
    IterDeclStmt (std::shared_ptr<Context> ctx, IntegerType::IntegerTypeID ty_, int64_t step_);
    IterDeclStmt (std::shared_ptr<Context> ctx, IntegerType::IntegerTypeID ty_, int64_t step_, std::shared_ptr<IDX> in_);

    IterDeclStmt (std::shared_ptr<Context> ctx, std::string n_, IntegerType::IntegerTypeID ty_, int64_t step_);
    IterDeclStmt (std::shared_ptr<Context> ctx, std::string n_, IntegerType::IntegerTypeID ty_, int64_t step_, std::shared_ptr<IDX> in_);

    IterDeclStmt (std::shared_ptr<Context> ctx, int64_t step_);
    IterDeclStmt (std::shared_ptr<Context> ctx, int64_t step_, std::shared_ptr<IDX> in_);

    std::shared_ptr<IterVar> get_data () { return this->data; }
    std::shared_ptr<Expr> get_init () { return this->init->getExprPtr(); }
    std::shared_ptr<IDX> get_init_idx () { return this->init; }
    virtual std::string get_name() { return this->data->get_name(); }

    virtual std::shared_ptr<Expr> getStepExpr (std::shared_ptr<Context> ctx);
    virtual std::string emit (std::string offset = "");
};


class VectorDeclStmt : public DeclStmt {
private:
    std::shared_ptr<Vector> data;
    std::shared_ptr<Expr> init;
    std::shared_ptr<Context> context;

public:
    VectorDeclStmt (std::shared_ptr<Context> ctx, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, uint64_t num_);
    VectorDeclStmt (std::shared_ptr<Context> ctx, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, std::vector<uint64_t> dim);

    VectorDeclStmt (std::shared_ptr<Context> ctx, std::string n_, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, uint64_t num_);
    VectorDeclStmt (std::shared_ptr<Context> ctx, std::string n_, VecElem::Kind knd_, IntegerType::IntegerTypeID ty_, std::vector<uint64_t> dim);

    VectorDeclStmt (std::shared_ptr<Context> ctx);

    void setPurpose(VecElem::Purpose p_) { this->data->setPurpose(p_); }
    std::shared_ptr<Vector> get_data () { return this->data; }

    virtual std::shared_ptr<Stmt> getInitStmt();
    virtual std::string emit (std::string offset = "");
};


class HeaderStmt : public Stmt {
protected:
    std::vector<std::shared_ptr<Stmt>> module;

private:
    std::shared_ptr<IterDeclStmt> decl;
    std::shared_ptr<AssignExpr> start;
    std::shared_ptr<Expr> exit;
    std::shared_ptr<Expr> step;

public:
    HeaderStmt (std::shared_ptr<Context> ctx, std::shared_ptr<IterDeclStmt> it, uint64_t tripcount);
    HeaderStmt (std::shared_ptr<Context> ctx, std::shared_ptr<IterDeclStmt> it,
                std::shared_ptr<IDX> start_e, uint64_t tripcount);

    virtual void add (std::shared_ptr<Stmt> e) {
        this->module.push_back(e);
    }

    virtual std::string emit (std::string offset = "");
};


class ForEachStmt : public Stmt {
protected:
    std::shared_ptr<ScalarVariable> var;
    std::shared_ptr<Stmt> todostmt;
    std::shared_ptr<Vector> vector;

public:
    ForEachStmt(std::shared_ptr<Vector> v);
    virtual std::shared_ptr<ScalarVariable> getVar () {return this->var; }
    virtual void add_single_stmt(std::shared_ptr<Stmt> s_) {this->todostmt = s_;}
    virtual std::string emit (std::string offset = "");
};
}

#endif

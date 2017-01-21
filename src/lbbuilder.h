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
#include "loop_expr.h"
#include "loop_stmt.h"
#include "loop_types.h"
#include "interface.h"
#include "namegen.h"

namespace rl {

class LBBuilder : public Stmt {
protected:
    LBBuilder *parent;
    std::shared_ptr<Context> context;
    uint64_t complexity_multiplier;
    std::vector<std::shared_ptr<Stmt>> module;

    std::vector<std::shared_ptr<IterDeclStmt>> idecls;

    // ivecs/iscls are guaranteed to have been initialized across all cells
    std::vector<std::shared_ptr<Vector>> ivecs;
    std::vector<std::shared_ptr<ScalarVariable>> iscls;

    // ovecs/oscls are vecs that are guaranteed to be written in by this loop
    std::vector<std::shared_ptr<Vector>> ovecs;
    std::vector<std::shared_ptr<ScalarVariable>> oscls;

    // the tripcount is randomly generated at the beginning of the loop generation
    uint64_t tripcount;

    virtual void add (std::shared_ptr<Stmt> e) {
        this->module.push_back(e);
    }

    virtual void add (std::vector<std::shared_ptr<VecElem>> e) {
        for (auto v : e) this->add(v);
    }

    virtual void add (std::shared_ptr<VecElem> e) {
        if (e->is_scalar()) {
            if (e->getPurpose() == VecElem::Purpose::TO_INIT){
                e->setPurpose(VecElem::Purpose::WONLY);
                this->oscls.push_back(std::static_pointer_cast<Cell>(e)->getData());
                return;
            }
            if (e->getPurpose() == VecElem::Purpose::WONLY) {
                this->oscls.push_back(std::static_pointer_cast<Cell>(e)->getData());
                return;
            }
            if (e->getPurpose() == VecElem::Purpose::RONLY) {
                this->iscls.push_back(std::static_pointer_cast<Cell>(e)->getData());
                return;
            }
        }
        if (e->is_vector()) {
            if (e->getPurpose() == VecElem::Purpose::TO_INIT) {
                this->ovecs.push_back(std::static_pointer_cast<Vector>(e));
                return;
            }
            if (e->getPurpose() == VecElem::Purpose::WONLY) {
                this->ovecs.push_back(std::static_pointer_cast<Vector>(e));
                return;
            }
            if (e->getPurpose() == VecElem::Purpose::RONLY) {
                this->ivecs.push_back(std::static_pointer_cast<Vector>(e));
                return;
            }
        }
    }

    virtual std::vector<std::shared_ptr<Vector>> cherryPick(std::vector<std::shared_ptr<Vector>> &vecs, VecElem::Purpose p_ = VecElem::Purpose::NONE);
    virtual std::vector<std::shared_ptr<ScalarVariable>> cherryPick(std::vector<std::shared_ptr<ScalarVariable>> &vars);

private:
    std::shared_ptr<IterDeclStmt> genCoreIter (std::shared_ptr<Vector> v, uint64_t A, uint64_t level);
    void add_me_some_iscls(std::shared_ptr<Vector> v_in);
    void add_me_some_oscls(std::shared_ptr<Vector> v_in);

public:
    uint64_t get_complexity_multiplier () { return this->complexity_multiplier; }
    LBBuilder (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Vector>> input);
    LBBuilder (LBBuilder *p, std::vector<std::shared_ptr<Vector>> input);
    virtual std::string emit (std::string offset = "");

private:
    void generate (std::vector<std::shared_ptr<Vector>> input);
};
}

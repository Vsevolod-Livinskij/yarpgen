/*
Copyright (c) 2015-2017, Intel Corporation

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

#include "cfg_gen.h"
#include "gen_policy.h"
#include "sym_table.h"

using namespace yarpgen;

void BasicBlock::split_block() {
    // First of all, we should split existing BasicBlock in two
    uint32_t split_bound = rand_val_gen->get_rand_value(0U, stmt_count - 1);
    std::shared_ptr<BasicBlock> end_bb = std::make_shared<BasicBlock>(stmt_count - split_bound, depth);
    end_bb->set_successors(successors);
    successors.clear();

    // Secondly, choose separating instruction
    // TODO: not the best way
    GenPolicy p;
    last_inst_id = rand_val_gen->get_rand_id(p.get_else_prob()) ? IF_ELSE : IF;

    for (int i = 0; i < (last_inst_id == IF ? 1 : 2); i++) {
        uint32_t inst_count = rand_val_gen->get_rand_value(p.get_min_scope_stmt_count(), p.get_max_scope_stmt_count());
        successors.emplace_back(std::make_shared<BasicBlock>(inst_count, depth + 1));
        successors.back()->add_successor(end_bb);
        if (rand_val_gen->get_rand_id(p.get_split_block_prob()) && depth <= p.get_max_if_depth()) {
            successors.back()->split_block();
        }
    }
}

std::vector<std::shared_ptr<Stmt>> BasicBlock::fill_with_stmts(std::shared_ptr<Context> ctx) {
    std::shared_ptr<ScopeStmt> tmp_scope = ScopeStmt::generate(ctx, stmt_count);
    if (last_inst_id == MAX_INST_ID)
        return tmp_scope->get_stmts();

    auto inp = ScopeStmt::extract_inp_and_mix_from_ctx(ctx);
    std::shared_ptr<Expr> cond = ArithExpr::generate(ctx, inp);
    bool cond_taken = IfStmt::count_if_taken(cond);
    auto create_if_branch = [&ctx, &cond_taken] (std::shared_ptr<BasicBlock>& successor, bool is_then_br) -> std::shared_ptr<ScopeStmt> {
        std::vector<std::shared_ptr<Stmt>> branch = successor->fill_with_stmts(
                std::make_shared<Context>(*(ctx->get_gen_policy()), ctx, Node::NodeID::SCOPE, is_then_br == cond_taken));
        std::shared_ptr<ScopeStmt> scope = std::make_shared<ScopeStmt>();
        for (auto const &i : branch)
            scope->add_stmt(i);
        return  scope;
    };
    std::shared_ptr<ScopeStmt> then_scope = create_if_branch(successors.front(), true);
    std::shared_ptr<ScopeStmt> else_scope = nullptr;
    if (last_inst_id == IF_ELSE)
        else_scope = create_if_branch(successors.back(), false);
    tmp_scope->add_stmt(std::make_shared<IfStmt>(cond, then_scope, else_scope));
    return tmp_scope->get_stmts();
}
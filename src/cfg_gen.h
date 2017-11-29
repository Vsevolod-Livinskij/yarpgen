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

#pragma once

#include <vector>
#include <memory>
#include "stmt.h"

namespace yarpgen {

class BasicBlock {
    public:
        enum LastInstID { IF, IF_ELSE, MAX_INST_ID };

        BasicBlock (uint32_t _stmt_count, uint32_t _depth) :
                stmt_count(_stmt_count), last_inst_id(MAX_INST_ID), depth(_depth) {}
        std::vector<std::shared_ptr<BasicBlock>>& get_successors() { return successors; }
        void set_successors(std::vector<std::shared_ptr<BasicBlock>> _successors) { successors = _successors; }
        void add_successor (std::shared_ptr<BasicBlock>successor) { successors.push_back(successor); }
        void split_block();
        std::vector<std::shared_ptr<Stmt>> fill_with_stmts(std::shared_ptr<Context> ctx);

    private:
        uint32_t stmt_count;
        std::vector<std::shared_ptr<BasicBlock>> successors;
        LastInstID last_inst_id;
        uint32_t depth;
};

}

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

//////////////////////////////////////////////////////////////////////////////
#include "gen_policy.h"

///////////////////////////////////////////////////////////////////////////////

using namespace rl;

int MAX_ALLOWED_INT_TYPES = 3;

int MAX_ARITH_DEPTH = 5;

int MIN_ARITH_STMT_NUM = 5;
int MAX_ARITH_STMT_NUM = 10;

int MAX_TMP_VAR_NUM = 5;

int MIN_INP_VAR_NUM = 20;
int MAX_INP_VAR_NUM = 60;
int MIN_MIX_VAR_NUM = 20;
int MAX_MIX_VAR_NUM = 60;

int MAX_CSE_NUM = 5;

int MAX_IF_DEPTH = 3;


int MIN_STRUCT_TYPES_NUM = 3;
int MAX_STRUCT_TYPES_NUM = 6;
int MIN_INP_STRUCT_NUM = 3;
int MAX_INP_STRUCT_NUM = 6;
int MIN_MIX_STRUCT_NUM = 3;
int MAX_MIX_STRUCT_NUM = 6;
int MIN_OUT_STRUCT_NUM = 4;
int MAX_OUT_STRUCT_NUM = 8;
int MIN_STRUCT_MEMBERS_NUM = 5;
int MAX_STRUCT_MEMBERS_NUM = 10;
int MAX_STRUCT_DEPTH = 5;
int MIN_BIT_FIELD_SIZE = 8;
int MAX_BIT_FIELD_SIZE = 2;

int MIN_ARR_SIZE = 100;
int MAX_ARR_SIZE = 5000;

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RandValGen> rl::rand_val_gen;
uint64_t RandValGen::struct_type_num = 0;
uint64_t RandValGen::scalar_var_num = 0;
uint64_t RandValGen::struct_var_num = 0;

RandValGen::RandValGen (uint64_t _seed) {
    if (_seed != 0) {
        seed = _seed;
    }
    else {
        std::random_device rd;
        seed = rd ();
    }
    std::cout << "/*SEED " << seed << "*/" << std::endl;
    rand_gen = std::mt19937_64(seed);
}

///////////////////////////////////////////////////////////////////////////////

GenPolicy::GenPolicy () {
    num_of_allowed_int_types = MAX_ALLOWED_INT_TYPES;
    rand_init_allowed_int_types();

    allowed_modifiers.push_back (Type::Mod::NTHG);

    allow_static_var = false;
    allow_static_members = true;

    allow_struct = true;
    min_struct_types_num = MIN_STRUCT_TYPES_NUM;
    max_struct_types_num = MAX_STRUCT_TYPES_NUM;
    min_inp_struct_num = MIN_INP_STRUCT_NUM;
    max_inp_struct_num = MAX_INP_STRUCT_NUM;
    min_mix_struct_num = MIN_MIX_STRUCT_NUM;
    max_mix_struct_num = MAX_MIX_STRUCT_NUM;
    min_out_struct_num = MIN_OUT_STRUCT_NUM;
    max_out_struct_num = MAX_OUT_STRUCT_NUM;
    min_struct_members_num = MIN_STRUCT_MEMBERS_NUM;
    max_struct_members_num = MAX_STRUCT_MEMBERS_NUM;
    allow_mix_mod_in_struct = false;
    allow_mix_static_in_struct = true;
    allow_mix_types_in_struct = true;
    member_use_prob.push_back(Probability<bool>(true, 80));
    member_use_prob.push_back(Probability<bool>(false, 20));
    max_struct_depth = MAX_STRUCT_DEPTH;
    member_class_prob.push_back(Probability<Data::VarClassID>(Data::VarClassID::VAR, 70));
    member_class_prob.push_back(Probability<Data::VarClassID>(Data::VarClassID::STRUCT, 30));
    min_bit_field_size = MIN_BIT_FIELD_SIZE;
    max_bit_field_size = MAX_BIT_FIELD_SIZE;
    bit_field_prob.push_back(Probability<BitFieldID>(UNNAMED, 30));
    bit_field_prob.push_back(Probability<BitFieldID>(NAMED, 60));
    bit_field_prob.push_back(Probability<BitFieldID>(MAX_BIT_FIELD_ID, 10));

    out_data_type_prob.push_back(Probability<OutDataTypeID>(VAR, 70));
    out_data_type_prob.push_back(Probability<OutDataTypeID>(STRUCT, 30));

    max_arith_depth = MAX_ARITH_DEPTH;

    min_arith_stmt_num = MIN_ARITH_STMT_NUM;
    max_arith_stmt_num = MAX_ARITH_STMT_NUM;

    min_inp_var_num = MIN_INP_VAR_NUM;
    max_inp_var_num = MAX_INP_VAR_NUM;
    min_mix_var_num = MIN_MIX_VAR_NUM;
    max_mix_var_num = MAX_MIX_VAR_NUM;

    max_tmp_var_num = MAX_TMP_VAR_NUM;
    used_tmp_var_num = 0;

    max_cse_num = MAX_CSE_NUM;

    for (int i = UnaryExpr::Op::Plus; i < UnaryExpr::Op::MaxOp; ++i) {
        Probability<UnaryExpr::Op> prob ((UnaryExpr::Op) i, 1);
        allowed_unary_op.push_back (prob);
    }

    for (int i = 0; i < BinaryExpr::Op::MaxOp; ++i) {
        Probability<BinaryExpr::Op> prob ((BinaryExpr::Op) i, 1);
        allowed_binary_op.push_back (prob);
    }

    Probability<Node::NodeID> decl_gen (Node::NodeID::DECL, 10);
    stmt_gen_prob.push_back (decl_gen);
    Probability<Node::NodeID> assign_gen (Node::NodeID::EXPR, 10);
    stmt_gen_prob.push_back (assign_gen);
    Probability<Node::NodeID> if_gen (Node::NodeID::IF, 10);
    stmt_gen_prob.push_back (if_gen);

    Probability<ArithLeafID> data_leaf (ArithLeafID::Data, 10);
    arith_leaves.push_back (data_leaf);
    Probability<ArithLeafID> unary_leaf (ArithLeafID::Unary, 20);
    arith_leaves.push_back (unary_leaf);
    Probability<ArithLeafID> binary_leaf (ArithLeafID::Binary, 45);
    arith_leaves.push_back (binary_leaf);
    Probability<ArithLeafID> type_cast_leaf (ArithLeafID::TypeCast, 10);
    arith_leaves.push_back (type_cast_leaf);
    Probability<ArithLeafID> cse_leaf (ArithLeafID::CSE, 5);
    arith_leaves.push_back (cse_leaf);

    Probability<ArithDataID> inp_data (ArithDataID::Inp, 80);
    arith_data_distr.push_back (inp_data);
    Probability<ArithDataID> const_data (ArithDataID::Const, 10);
    arith_data_distr.push_back (const_data);

    Probability<ArithCSEGenID> add_cse (ArithCSEGenID::Add, 20);
    arith_cse_gen.push_back (add_cse);
    Probability<ArithCSEGenID> max_cse_gen (ArithCSEGenID::MAX_CSE_GEN_ID, 80);
    arith_cse_gen.push_back (max_cse_gen);

    Probability<ArithSSP::ConstUse> const_branch (ArithSSP::ConstUse::CONST_BRANCH, 5);
    allowed_arith_ssp_const_use.push_back(const_branch);
    Probability<ArithSSP::ConstUse> half_const (ArithSSP::ConstUse::HALF_CONST, 5);
    allowed_arith_ssp_const_use.push_back(half_const);
    Probability<ArithSSP::ConstUse> no_ssp_const_use (ArithSSP::ConstUse::MAX_CONST_USE, 90);
    allowed_arith_ssp_const_use.push_back(no_ssp_const_use);

    chosen_arith_ssp_const_use = ArithSSP::ConstUse::MAX_CONST_USE;

    Probability<ArithSSP::SimilarOp> additive (ArithSSP::SimilarOp::ADDITIVE, 5);
    allowed_arith_ssp_similar_op.push_back(additive);
    Probability<ArithSSP::SimilarOp> bitwise (ArithSSP::SimilarOp::BITWISE, 5);
    allowed_arith_ssp_similar_op.push_back(bitwise);
    Probability<ArithSSP::SimilarOp> logic (ArithSSP::SimilarOp::LOGIC, 5);
    allowed_arith_ssp_similar_op.push_back(logic);
    Probability<ArithSSP::SimilarOp> mul (ArithSSP::SimilarOp::MUL, 5);
    allowed_arith_ssp_similar_op.push_back(mul);
    Probability<ArithSSP::SimilarOp> bit_sh (ArithSSP::SimilarOp::BIT_SH, 5);
    allowed_arith_ssp_similar_op.push_back(bit_sh);
    Probability<ArithSSP::SimilarOp> add_mul (ArithSSP::SimilarOp::ADD_MUL, 5);
    allowed_arith_ssp_similar_op.push_back(add_mul);
    Probability<ArithSSP::SimilarOp> no_ssp_similar_op (ArithSSP::SimilarOp::MAX_SIMILAR_OP, 70);
    allowed_arith_ssp_similar_op.push_back(no_ssp_similar_op);

    chosen_arith_ssp_similar_op = ArithSSP::SimilarOp::MAX_SIMILAR_OP;

    Probability<bool> else_exist (true, 50);
    else_prob.push_back(else_exist);
    Probability<bool> no_else (false, 50);
    else_prob.push_back(no_else);

    max_if_depth = MAX_IF_DEPTH;

    min_arr_size = MIN_ARR_SIZE;
    max_arr_size = MAX_ARR_SIZE;
    Probability<VecElem::Kind> c_arr (VecElem::Kind::C_ARR, 30);
    arr_kind.push_back(c_arr);
    Probability<VecElem::Kind> std_vec (VecElem::Kind::STD_VEC, 10);
    arr_kind.push_back(std_vec);
    Probability<VecElem::Kind> std_varr (VecElem::Kind::STD_VARR, 10);
    arr_kind.push_back(std_varr);
    Probability<VecElem::Kind> std_arr (VecElem::Kind::STD_ARR, 10);
    arr_kind.push_back(std_arr);
}

void GenPolicy::copy_data (std::shared_ptr<GenPolicy> old) {
    cse = old->get_cse();
}

int GenPolicy::get_access_type_score (VecElem::AccessKind knd_) {
    switch (knd_) {
        case VecElem::AccessKind::REGULAR : return 10;
        case VecElem::AccessKind::AT : return 10;
        case VecElem::AccessKind::PTR : return 10;
        case VecElem::AccessKind::ITERPTR : return 10;
    }
    return 0;
}

GenPolicy GenPolicy::apply_arith_ssp_const_use (ArithSSP::ConstUse pattern_id) {
    chosen_arith_ssp_const_use = pattern_id;
    GenPolicy new_policy = *this;
    if (pattern_id == ArithSSP::ConstUse::CONST_BRANCH) {
        new_policy.arith_data_distr.clear();
        Probability<ArithDataID> const_data (ArithDataID::Const, 100);
        new_policy.arith_data_distr.push_back (const_data);
    }
    else if (pattern_id == ArithSSP::ConstUse::HALF_CONST) {
        new_policy.arith_data_distr.clear();
        Probability<ArithDataID> inp_data (ArithDataID::Inp, 50);
        new_policy.arith_data_distr.push_back (inp_data);
        Probability<ArithDataID> const_data (ArithDataID::Const, 50);
        new_policy.arith_data_distr.push_back (const_data);
    }
    return new_policy;
}

GenPolicy GenPolicy::apply_arith_ssp_similar_op (ArithSSP::SimilarOp pattern_id) {
    chosen_arith_ssp_similar_op = pattern_id;
    GenPolicy new_policy = *this;
    if (pattern_id == ArithSSP::SimilarOp::ADDITIVE || pattern_id == ArithSSP::SimilarOp::ADD_MUL) {
        new_policy.allowed_unary_op.clear();
        // TODO: add default probability to gen_policy;
        Probability<UnaryExpr::Op> plus (UnaryExpr::Op::Plus, 50);
        new_policy.allowed_unary_op.push_back (plus);
        Probability<UnaryExpr::Op> negate (UnaryExpr::Op::Negate, 50);
        new_policy.allowed_unary_op.push_back (negate);

        new_policy.allowed_binary_op.clear();
        // TODO: add default probability to gen_policy;
        Probability<BinaryExpr::Op> add (BinaryExpr::Op::Add, 33);
        new_policy.allowed_binary_op.push_back (add);
        Probability<BinaryExpr::Op> sub (BinaryExpr::Op::Sub, 33);
        new_policy.allowed_binary_op.push_back (sub);

        if (pattern_id == ArithSSP::SimilarOp::ADD_MUL) {
            Probability<BinaryExpr::Op> mul (BinaryExpr::Op::Mul, 33);
            new_policy.allowed_binary_op.push_back (mul);
        }
    }
    else if (pattern_id == ArithSSP::SimilarOp::BITWISE || pattern_id == ArithSSP::SimilarOp::BIT_SH) {
        new_policy.allowed_unary_op.clear();
        Probability<UnaryExpr::Op> bit_not (UnaryExpr::Op::BitNot, 100);
        new_policy.allowed_unary_op.push_back (bit_not);

        new_policy.allowed_binary_op.clear();
        Probability<BinaryExpr::Op> bit_and (BinaryExpr::Op::BitAnd, 20);
        new_policy.allowed_binary_op.push_back (bit_and);
        Probability<BinaryExpr::Op> bit_xor (BinaryExpr::Op::BitXor, 20);
        new_policy.allowed_binary_op.push_back (bit_xor);
        Probability<BinaryExpr::Op> bit_or (BinaryExpr::Op::BitOr, 20);
        new_policy.allowed_binary_op.push_back (bit_or);

        if (pattern_id == ArithSSP::SimilarOp::BIT_SH) {
            Probability<BinaryExpr::Op> shl (BinaryExpr::Op::Shl, 20);
            new_policy.allowed_binary_op.push_back (shl);
            Probability<BinaryExpr::Op> shr (BinaryExpr::Op::Shr, 20);
            new_policy.allowed_binary_op.push_back (shr);
        }
    }
    else if (pattern_id == ArithSSP::SimilarOp::LOGIC) {
        new_policy.allowed_unary_op.clear();
        Probability<UnaryExpr::Op> log_not (UnaryExpr::Op::LogNot, 100);
        new_policy.allowed_unary_op.push_back (log_not);

        new_policy.allowed_binary_op.clear();
        Probability<BinaryExpr::Op> log_and (BinaryExpr::Op::LogAnd, 50);
        new_policy.allowed_binary_op.push_back (log_and);
        Probability<BinaryExpr::Op> log_or (BinaryExpr::Op::LogOr, 50);
        new_policy.allowed_binary_op.push_back (log_or);
    }
    else if (pattern_id == ArithSSP::SimilarOp::MUL) {
        // TODO: what about unary expr?
        new_policy.allowed_binary_op.clear();
        Probability<BinaryExpr::Op> mul (BinaryExpr::Op::Mul, 100);
        new_policy.allowed_binary_op.push_back (mul);
    }
    return new_policy;
}

void GenPolicy::rand_init_allowed_int_types () {
    allowed_int_types.clear ();
    std::vector<IntegerType::IntegerTypeID> tmp_allowed_int_types;
    int gen_types = 0;
    while (gen_types < num_of_allowed_int_types) {
        IntegerType::IntegerTypeID type = (IntegerType::IntegerTypeID) rand_val_gen->get_rand_value<int>(0, IntegerType::IntegerTypeID::MAX_INT_ID - 1);
        if (std::find(tmp_allowed_int_types.begin(), tmp_allowed_int_types.end(), type) == tmp_allowed_int_types.end()) {
            tmp_allowed_int_types.push_back (type);
            gen_types++;
        }
    }
    for (auto i : tmp_allowed_int_types) {
        Probability<IntegerType::IntegerTypeID> prob (i, 1);
        allowed_int_types.push_back (prob);
    }
}

void GenPolicy::set_modifier (bool value, Type::Mod modifier) {
    if (value)
        allowed_modifiers.push_back (modifier);
    else
        allowed_modifiers.erase (std::remove (allowed_modifiers.begin(), allowed_modifiers.end(), modifier), allowed_modifiers.end());
}

bool GenPolicy::get_modifier (Type::Mod modifier) {
    return (std::find(allowed_modifiers.begin(), allowed_modifiers.end(), modifier) != allowed_modifiers.end());
}

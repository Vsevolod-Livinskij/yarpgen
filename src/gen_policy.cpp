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
#include <map>
#include <cassert>

#include "gen_policy.h"

///////////////////////////////////////////////////////////////////////////////

using namespace rl;

int MAX_ALLOWED_INT_TYPES = 3;

int MAX_ARITH_DEPTH = 5;

int MIN_SCOPE_STMT_NUM = 5;
int MAX_SCOPE_STMT_NUM = 10;

int MAX_TOTAL_STMT_NUM = 5000;

int MIN_INP_VAR_NUM = 20;
int MAX_INP_VAR_NUM = 60;
int MIN_MIX_VAR_NUM = 20;
int MAX_MIX_VAR_NUM = 60;

int MAX_CSE_NUM = 5;

int MAX_IF_DEPTH = 3;

uint64_t MAX_TEST_COMPLEXITY = UINT64_MAX;

int MIN_STRUCT_TYPES_NUM = 0;
int MAX_STRUCT_TYPES_NUM = 6;
int MIN_INP_STRUCT_NUM = 0;
int MAX_INP_STRUCT_NUM = 6;
int MIN_MIX_STRUCT_NUM = 0;
int MAX_MIX_STRUCT_NUM = 6;
int MIN_OUT_STRUCT_NUM = 0;
int MAX_OUT_STRUCT_NUM = 8;
int MIN_STRUCT_MEMBERS_NUM = 1;
int MAX_STRUCT_MEMBERS_NUM = 10;
int MAX_STRUCT_DEPTH = 2;
int MIN_BIT_FIELD_SIZE = 8;
int MAX_BIT_FIELD_SIZE = 2; //TODO: unused, because it cause different result for LLVM and GCC. See pr70733

int MIN_ARRAY_SIZE = 10;
int MAX_ARRAY_SIZE = 1000;
int MIN_ARRAY_DEPTH = 1;
int MAX_ARRAY_DEPTH = 4;
///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RandValGen> rl::rand_val_gen;
uint64_t RandValGen::struct_type_num = 0;
uint64_t RandValGen::scalar_var_num = 0;
uint64_t RandValGen::struct_var_num = 0;
uint64_t RandValGen::array_var_num = 0;

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

bool GenPolicy::default_was_loaded = false;
GenPolicy rl::default_gen_policy;

GenPolicy::GenPolicy () {
    if (default_was_loaded)
        *this = default_gen_policy;
}

void GenPolicy::init_from_config () {
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
    rand_val_gen->shuffle_prob(member_use_prob);
    max_struct_depth = MAX_STRUCT_DEPTH;
    member_type_id_prob.push_back(Probability<Type::TypeID>(Type::ATOMIC_TYPE, 10));
    member_type_id_prob.push_back(Probability<Type::TypeID>(Type::STRUCT_TYPE, 80));
    member_type_id_prob.push_back(Probability<Type::TypeID>(Type::ARRAY_TYPE , 80));
    rand_val_gen->shuffle_prob(member_type_id_prob);
    static_member_prob.push_back(Probability<bool>(true, 20));
    static_member_prob.push_back(Probability<bool>(false, 80));
    rand_val_gen->shuffle_prob(static_member_prob);
    array_with_struct_prob.push_back(Probability<bool>(true, 80));
    array_with_struct_prob.push_back(Probability<bool>(false, 20));
    rand_val_gen->shuffle_prob(array_with_struct_prob);
    min_bit_field_size = MIN_BIT_FIELD_SIZE;
    max_bit_field_size = MAX_BIT_FIELD_SIZE;
    bit_field_prob.push_back(Probability<BitFieldID>(UNNAMED, 15));
    bit_field_prob.push_back(Probability<BitFieldID>(NAMED, 20));
    bit_field_prob.push_back(Probability<BitFieldID>(MAX_BIT_FIELD_ID, 65));
    rand_val_gen->shuffle_prob(bit_field_prob);

    min_array_size = MIN_ARRAY_SIZE;
    max_array_size = MAX_ARRAY_SIZE;
    min_array_depth = MIN_ARRAY_DEPTH;
    max_array_depth = MAX_ARRAY_DEPTH;
    array_kind_prob.push_back(Probability<ArrayType::Kind>(ArrayType::C_ARR, 25));
    array_kind_prob.push_back(Probability<ArrayType::Kind>(ArrayType::VAL_ARR, 25));
    array_kind_prob.push_back(Probability<ArrayType::Kind>(ArrayType::STD_VEC, 25));
    array_kind_prob.push_back(Probability<ArrayType::Kind>(ArrayType::STD_ARR, 25));
    rand_val_gen->shuffle_prob(array_kind_prob);
    allow_mix_kind = true;
    array_base_type_prob.push_back(Probability<Type::TypeID>(Type::STRUCT_TYPE, 30));
    array_base_type_prob.push_back(Probability<Type::TypeID>(Type::ATOMIC_TYPE, 0));
    rand_val_gen->shuffle_prob(array_base_type_prob);
    struct_with_array_prob.push_back(Probability<bool>(false, 80));
    struct_with_array_prob.push_back(Probability<bool>(true, 20));
    rand_val_gen->shuffle_prob(struct_with_array_prob);

    out_data_type_prob.push_back(Probability<OutDataTypeID>(VAR, 70));
    out_data_type_prob.push_back(Probability<OutDataTypeID>(STRUCT, 30));
    rand_val_gen->shuffle_prob(out_data_type_prob);

    max_arith_depth = MAX_ARITH_DEPTH;

    min_scope_stmt_num = MIN_SCOPE_STMT_NUM;
    max_scope_stmt_num = MAX_SCOPE_STMT_NUM;

    max_total_stmt_num = MAX_TOTAL_STMT_NUM;

    min_inp_var_num = MIN_INP_VAR_NUM;
    max_inp_var_num = MAX_INP_VAR_NUM;
    min_mix_var_num = MIN_MIX_VAR_NUM;
    max_mix_var_num = MAX_MIX_VAR_NUM;

    max_cse_num = MAX_CSE_NUM;

    for (int i = UnaryExpr::Op::Plus; i < UnaryExpr::Op::MaxOp; ++i) {
        Probability<UnaryExpr::Op> prob ((UnaryExpr::Op) i, 10);
        allowed_unary_op.push_back (prob);
    }
    rand_val_gen->shuffle_prob(allowed_unary_op);

    for (int i = 0; i < BinaryExpr::Op::MaxOp; ++i) {
        Probability<BinaryExpr::Op> prob ((BinaryExpr::Op) i, 10);
        allowed_binary_op.push_back (prob);
    }
    rand_val_gen->shuffle_prob(allowed_binary_op);

    Probability<Node::NodeID> decl_gen (Node::NodeID::DECL, 10);
    stmt_gen_prob.push_back (decl_gen);
    Probability<Node::NodeID> assign_gen (Node::NodeID::EXPR, 10);
    stmt_gen_prob.push_back (assign_gen);
    Probability<Node::NodeID> if_gen (Node::NodeID::IF, 10);
    stmt_gen_prob.push_back (if_gen);
    rand_val_gen->shuffle_prob(stmt_gen_prob);

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
    rand_val_gen->shuffle_prob(arith_leaves);

    Probability<ArithDataID> inp_data (ArithDataID::Inp, 80);
    arith_data_distr.push_back (inp_data);
    Probability<ArithDataID> const_data (ArithDataID::Const, 20);
    arith_data_distr.push_back (const_data);
    rand_val_gen->shuffle_prob(arith_data_distr);

    Probability<ArithCSEGenID> add_cse (ArithCSEGenID::Add, 20);
    arith_cse_gen.push_back (add_cse);
    Probability<ArithCSEGenID> max_cse_gen (ArithCSEGenID::MAX_CSE_GEN_ID, 80);
    arith_cse_gen.push_back (max_cse_gen);
    rand_val_gen->shuffle_prob(arith_cse_gen);

    Probability<ArithSSP::ConstUse> const_branch (ArithSSP::ConstUse::CONST_BRANCH, 5);
    allowed_arith_ssp_const_use.push_back(const_branch);
    Probability<ArithSSP::ConstUse> half_const (ArithSSP::ConstUse::HALF_CONST, 5);
    allowed_arith_ssp_const_use.push_back(half_const);
    Probability<ArithSSP::ConstUse> no_ssp_const_use (ArithSSP::ConstUse::MAX_CONST_USE, 90);
    allowed_arith_ssp_const_use.push_back(no_ssp_const_use);
    rand_val_gen->shuffle_prob(allowed_arith_ssp_const_use);

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
    rand_val_gen->shuffle_prob(allowed_arith_ssp_similar_op);

    chosen_arith_ssp_similar_op = ArithSSP::SimilarOp::MAX_SIMILAR_OP;

    Probability<bool> else_exist (true, 50);
    else_prob.push_back(else_exist);
    Probability<bool> no_else (false, 50);
    else_prob.push_back(no_else);
    rand_val_gen->shuffle_prob(else_prob);

    max_if_depth = MAX_IF_DEPTH;

    max_test_complexity = MAX_TEST_COMPLEXITY;

    default_was_loaded = true;
}

void GenPolicy::copy_data (std::shared_ptr<GenPolicy> old) {
    cse = old->get_cse();
}

template <>
void GenPolicy::prohibit_complex_subtype_id<ArrayType> () {
    prohibit_type_id(array_base_type_prob, Type::STRUCT_TYPE);
}

template <>
void GenPolicy::prohibit_complex_subtype_id<StructType> () {
    prohibit_type_id(member_type_id_prob, Type::ARRAY_TYPE);
}

void GenPolicy::prohibit_type_id(std::vector<Probability<Type::TypeID>> &prob, const Type::TypeID key) {
    std::vector<Probability<Type::TypeID>>::iterator array_iter = std::find_if(prob.begin(), prob.end(),
                                                              [key] (Probability<Type::TypeID> elem)->bool {
                                                                  return (elem.get_id() == key);
                                                              });
    if (array_iter == prob.end())
        return;

    // When we remove one variant from probability vector we don't significantly change distribution of all other variants
    // a = x, b = y, c = z -> P(a) = x / (x + y + z) = 0.2 and P(b) = y / (x + y + z), so P(a) / P(b) = x / y
    // If we remove c and share it's probability among others we'll have
    // P(a) = x / (x + y + z) + z * x / (x + y) = x * (1 / (x + y + z) + z / (x + y))
    // P(B) = y / (x + y + z) + z * y / (x + y) = y * (1 / (x + y + z) + z / (x + y))
    // And P(a) / P(b) = x / y
    prob.erase(array_iter);

    assert (prob.size() > 0 && "Distribution vector can't has zero length");

    bool all_prob_zeroes = std::all_of(prob.begin(), prob.end(),
                                       [] (Probability<Type::TypeID> elem)->bool { return (elem.get_prob() == 0); });
    if (all_prob_zeroes)
        for (auto &i : prob)
            i.increase_prob(1);
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

// Abstract measure of complexity of execution
static const std::map<Node::NodeID, uint64_t> NodeComplexity {
    {Node::NodeID::ASSIGN, 5},
    {Node::NodeID::BINARY, 10},
    {Node::NodeID::CONST, 5},
    {Node::NodeID::TYPE_CAST, 10},
    {Node::NodeID::UNARY, 5},
    {Node::NodeID::VAR_USE, 5},
    {Node::NodeID::MEMBER, 10},
    {Node::NodeID::MAX_EXPR_ID, UINT64_MAX},
    {Node::NodeID::MIN_STMT_ID, UINT64_MAX},
    {Node::NodeID::DECL, 30},
    {Node::NodeID::EXPR, 20},
    {Node::NodeID::SCOPE, 5},
    {Node::NodeID::IF, 50},
    {Node::NodeID::MAX_STMT_ID, UINT64_MAX}
};

uint64_t GenPolicy::test_complexity = 0;

void GenPolicy::add_to_complexity(Node::NodeID node_id) {
    test_complexity += NodeComplexity.at(node_id);
}

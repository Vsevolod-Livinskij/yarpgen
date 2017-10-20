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

#include <cassert>
#include "expr.h"
#include "ir_node.h"
#include "gen_policy.h"
#include "sym_table.h"
#include "type.h"
#include "util.h"
#include "variable.h"

using namespace yarpgen;

uint32_t Expr::total_expr_count = 0;
uint32_t Expr::func_expr_count = 0;

std::shared_ptr<Data> Expr::get_value () {
    switch (value->get_class_id()) {
        case Data::VarClassID::VAR: {
            std::shared_ptr<ScalarVariable> scalar_var = std::make_shared<ScalarVariable>(*(std::static_pointer_cast<ScalarVariable>(value)));
            scalar_var->set_name("");
            return scalar_var;
        }
        case Data::VarClassID::STRUCT: {
            std::shared_ptr<Struct> struct_var = std::make_shared<Struct>(*(std::static_pointer_cast<Struct>(value)));
            struct_var->set_name("");
            return struct_var;
        }
        //TODO: implement for Array
        case Data::VarClassID::ARRAY:
        case Data::VarClassID::MAX_CLASS_ID:
            ERROR("unsupported Data::VarClassID (Expr)");
    }
    ERROR("Expr::get_value() - data corruption");
}

std::shared_ptr<Expr> VarUseExpr::set_value (std::shared_ptr<Expr> _expr) {
    std::shared_ptr<Data> _new_value = _expr->get_value();
    if (_new_value->get_class_id() != value->get_class_id()) {
        ERROR("different Data::VarClassID (VarUseExpr)");
    }
    switch (value->get_class_id()) {
        case Data::VarClassID::VAR:
            //TODO: Add integer type id check. We can't assign different types
            std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(_new_value)->get_cur_value());
            value->get_raw_complexity() = _expr->get_raw_complexity();
            return _expr;
            break;
        case Data::VarClassID::STRUCT:
            //TODO: implement for Struct
            ERROR("struct is unsupported (VarUseExpr)");
            break;
        case Data::VarClassID::ARRAY:
            //TODO: implement for Array
            ERROR("array is unsupported (VarUseExpr)");
            break;
        case Data::VarClassID::MAX_CLASS_ID:
            ERROR("unsupported Data::VarClassID (VarUseExpr)");
    }
    ERROR("Expr::set_value() - data corruption");
}

VarUseExpr::VarUseExpr(std::shared_ptr<Data> _var) : Expr(Node::NodeID::VAR_USE, _var, 1) {
    complexity = value->get_raw_complexity();
}

AssignExpr::AssignExpr (std::shared_ptr<Expr> _to, std::shared_ptr<Expr> _from, bool _taken) :
                        Expr(Node::NodeID::ASSIGN, _to->get_value(), 0),
                        to(_to), from(_from), taken(_taken) {
    if (to->get_id() != Node::NodeID::VAR_USE && to->get_id() != Node::NodeID::MEMBER) {
        ERROR("can assign only to variable (AssignExpr)");
    }
    propagate_type();
    propagate_value();
    to->get_raw_complexity().add_oper_count = from->get_raw_complexity().add_oper_count;
    to->get_raw_complexity().mul_oper_count = from->get_raw_complexity().mul_oper_count;
    complexity = from->get_raw_complexity();
    complexity.full_complexity = to->get_full_complexity() + from->get_full_complexity() + 1;
}

bool AssignExpr::propagate_type () {
    //TODO:StructType check for struct assignment
    if (value->get_class_id() == Data::VarClassID::VAR &&
        from->get_value()->get_class_id() == Data::VarClassID::VAR) {
        from = std::make_shared<TypeCastExpr>(from, value->get_type(), true);
    }
    else {
        ERROR("struct are unsupported (AssignExpr)");
    }
    return true;
}

UB AssignExpr::propagate_value () {
    value = from->get_value();
    if (!taken)
        return NoUB;

    if (to->get_id() == Node::NodeID::VAR_USE) {
        from = std::static_pointer_cast<VarUseExpr>(to)->set_value(from);
    }
    else if (to->get_id() == Node::NodeID::MEMBER) {
        from = std::static_pointer_cast<MemberExpr>(to)->set_value(from);
    }
    else {
        ERROR("can assign only to variable (AssignExpr)");
    }

    return NoUB;
}

void AssignExpr::emit (std::ostream& stream, std::string offset) {
    stream << offset;
    to->emit(stream);
    stream << " = ";
    from->emit(stream);
}

TypeCastExpr::TypeCastExpr (std::shared_ptr<Expr> _expr, std::shared_ptr<Type> _type, bool _is_implicit) :
              Expr(Node::NodeID::TYPE_CAST, nullptr, _expr->get_full_complexity() + 1),
              expr(_expr), to_type(_type), is_implicit(_is_implicit) {
    complexity = _expr->get_raw_complexity();
    propagate_type();
    propagate_value();
}

bool TypeCastExpr::propagate_type () {
    if (!to_type->is_builtin_type())
        //TODO: what about overloaded struct types cast?
        ERROR("can cast only integer or fp types (TypeCastExpr)");
    return true;
}

UB TypeCastExpr::propagate_value () {
    if (expr->get_value()->get_class_id() != Data::VarClassID::VAR)
        //TODO: what about overloaded struct types cast?
        ERROR("can cast only integer or fp types (TypeCastExpr)");

    //TODO: Is it always safe to cast value to ScalarVariable?
    if (to_type->is_int_type())
        value = std::make_shared<ScalarVariable>("", std::static_pointer_cast<IntegerType>(to_type));
    else if (to_type->is_fp_type())
        value = std::make_shared<ScalarVariable>("", std::static_pointer_cast<FPType>(to_type));
    else
        ERROR("can cast only integer or fp types (TypeCastExpr)");

    std::shared_ptr<ScalarVariable> expr_val = std::static_pointer_cast<ScalarVariable>(expr->get_value());
    BuiltinType::ScalarTypedVal new_val (IntegerType::IntegerTypeID::MAX_INT_ID);
    if (to_type->is_int_type())
        new_val = expr_val->get_cur_value().cast_type(to_type->get_int_type_id());
    else if (to_type->is_fp_type())
        new_val = expr_val->get_cur_value().cast_type(to_type->get_fp_type_id());
    else
        ERROR("unsupported type");

    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
    return NoUB;
}

std::shared_ptr<TypeCastExpr> TypeCastExpr::generate (std::shared_ptr<Context> ctx, std::shared_ptr<Expr> from) {
    GenPolicy::add_to_complexity(Node::NodeID::TYPE_CAST);
    std::shared_ptr<Type> to_type;
    if (options->is_int_mode())
        to_type = IntegerType::generate(ctx);
    else if (options->is_fp_mode())
        to_type = FPType::generate(ctx);
    else
        ERROR("Bad mode");
    return std::make_shared<TypeCastExpr> (from, to_type, false);
}

void TypeCastExpr::emit (std::ostream& stream, std::string offset) {
    std::string ret = offset;
    //TODO: add parameter to gen_policy
    if (!is_implicit or is_implicit)
        stream << "(" << value->get_type()->get_simple_name() << ") ";
    stream << "(";
    expr->emit(stream);
    stream << ")";
}

std::vector<BuiltinType::ScalarTypedVal> ConstExpr::arith_const_buffer;
std::vector<BuiltinType::ScalarTypedVal> ConstExpr::bit_log_const_buffer;

//TODO: maybe variadic template function would be better?
template <typename T>
static bool can_reroll (std::vector<Probability<T>>& prob_vec, std::initializer_list<T> bad_variants) {
    uint64_t sum_of_all = 0;
    uint64_t sum_of_bad = 0;
    for (auto &i : prob_vec) {
        sum_of_all += i.get_prob();
        for (auto &j : bad_variants)
            if (i.get_id() == j)
                sum_of_bad += i.get_prob();
    }
    return sum_of_all != sum_of_bad;
}

std::shared_ptr<ConstExpr> ConstExpr::generate (std::shared_ptr<Context> ctx) {
    GenPolicy::add_to_complexity(Node::NodeID::CONST);
    auto p = ctx->get_gen_policy();

    // Arithmetic with floating point doesn't require complicated constant generation
    if (options->is_fp_mode()) {
        FPType::FPTypeID fp_type_id = FPType::generate(ctx)->get_fp_type_id();
        return std::make_shared<ConstExpr>(BuiltinType::ScalarTypedVal::generate(ctx, fp_type_id));
    }

    // Randomly choose if we want to create new constant or somehow reuse the old one
    bool gen_new_const = rand_val_gen->get_rand_id(p->get_new_const_prob());

    // Determine type of context
    auto chosen_arith_ssp_similar_op = p->get_chosen_arith_ssp_similar_op();
    bool bit_log_ctx = chosen_arith_ssp_similar_op == ArithSSP::SimilarOp::BITWISE ||
                       chosen_arith_ssp_similar_op == ArithSSP::SimilarOp::LOGIC   ||
                       chosen_arith_ssp_similar_op == ArithSSP::SimilarOp::BIT_SH  ||
                       chosen_arith_ssp_similar_op == ArithSSP::SimilarOp::MAX_SIMILAR_OP;

    // Determine actual buffer of used constants based on type of context
    std::vector<BuiltinType::ScalarTypedVal>& actual_const_buffer = arith_const_buffer;
    if (bit_log_ctx)
        actual_const_buffer = bit_log_const_buffer;

    // Utility function for various transformation of constants
    auto perform_unary_op = [] (UnaryExpr::Op op, BuiltinType::ScalarTypedVal val) -> BuiltinType::ScalarTypedVal {
        std::shared_ptr<ConstExpr> tmp_const = std::make_shared<ConstExpr>(val);
        UnaryExpr unary_expr = UnaryExpr(op, tmp_const);
        return std::static_pointer_cast<ScalarVariable>(unary_expr.get_value())->get_cur_value();
    };

    // Main logical part
    BuiltinType::ScalarTypedVal new_val (Type::IntegerTypeID::MAX_INT_ID);
    if (gen_new_const || actual_const_buffer.empty()) {
        // Randomly pick type of new constant
        bool gen_new_type = rand_val_gen->get_rand_id(p->get_new_const_type_prob());
        IntegerType::IntegerTypeID int_type_id;
        if (gen_new_type || actual_const_buffer.empty())
            int_type_id = IntegerType::generate(ctx)->get_int_type_id();
        else
            int_type_id = rand_val_gen->get_rand_elem(actual_const_buffer).get_int_type_id();
        std::shared_ptr<IntegerType> tmp_int_type = IntegerType::init(int_type_id);
        new_val = BuiltinType::ScalarTypedVal(int_type_id);

        // Randomly choose what kind of special constant we want
        ConstPattern::SpecialConst spec_const_id = rand_val_gen->get_rand_id(p->get_special_const_prob());
        if (spec_const_id < ConstPattern::SpecialConst::MAX_SPECIAL_CONST) {
            // Magic numbers (0, 1, 2, ...)
            new_val.set_abs_val(spec_const_id);
            bool negative_sign = rand_val_gen->get_rand_value(false, true);
            if (negative_sign && tmp_int_type->get_is_signed())
                new_val = perform_unary_op(UnaryExpr::Negate, new_val);
        }
        else if (spec_const_id == ConstPattern::SpecialConst::MAX_SPECIAL_CONST) {
            bool use_max = rand_val_gen->get_rand_value(false, true);
            new_val = use_max ? tmp_int_type->get_max() : tmp_int_type->get_min();
        }
        else
            ERROR("Bad id for ConstPattern::SpecialConst");
    }
    else {
        BuiltinType::ScalarTypedVal& buf_elem = rand_val_gen->get_rand_elem(actual_const_buffer);
        new_val = buf_elem;

        // Randomly pick a kind of an action on the chosen constant
        UnaryExpr::Op const_transform_id = rand_val_gen->get_rand_id(p->get_const_transform_prob());
        if (!bit_log_ctx && const_transform_id == UnaryExpr::Op::BitNot) {
            bool can_reroll_const_transform = can_reroll(p->get_const_transform_prob(),
                                                         {UnaryExpr::Op::BitNot});
            if (can_reroll_const_transform)
                while (const_transform_id == UnaryExpr::Op::BitNot)
                    const_transform_id = rand_val_gen->get_rand_id(p->get_const_transform_prob());
            else
                const_transform_id = UnaryExpr::Op::Plus;
        }

        if (const_transform_id != UnaryExpr::Op::Plus   &&
            const_transform_id != UnaryExpr::Op::Negate &&
            const_transform_id != UnaryExpr::Op::BitNot)
            ERROR("Bad id for UnaryExpr::Op");

        new_val = perform_unary_op(const_transform_id, new_val);
    }

    return std::make_shared<ConstExpr>(new_val);
}

void ConstExpr::fill_const_buf (std::shared_ptr<Context> ctx) {
    // Arithmetic with floating point doesn't require complicated constant generation,
    // so we don't use these buffers
    if (options->is_fp_mode())
        return;

    // Wipe out old information
    arith_const_buffer.clear();
    bit_log_const_buffer.clear();

    auto p = ctx->get_gen_policy();

    // Fill buffer for constants, used in arithmetic context
    for (uint64_t i = 0; i < p->get_const_buffer_size(); i++) {
        IntegerType::IntegerTypeID int_type_id = IntegerType::generate(ctx)->get_int_type_id();
        arith_const_buffer.push_back(BuiltinType::ScalarTypedVal::generate(ctx, int_type_id));
    }

    // Utility function for EndBits and BitBlock
    auto fill_bits = [](uint32_t start, uint32_t end) -> uint64_t {
        assert(end >= start);
        return (end - start) == 63 ? UINT64_MAX : ((1ULL << (end - start + 1)) - 1ULL) << start;
    };

    // Fill buffer for constants, used in bit-logical context
    for (uint64_t i = 0; i < p->get_const_buffer_size(); i++) {
        // Tmp variables, required for loop iteration
        IntegerType::IntegerTypeID int_type_id = IntegerType::generate(ctx)->get_int_type_id();
        std::shared_ptr<IntegerType> tmp_int_type = IntegerType::init(int_type_id);
        BuiltinType::ScalarTypedVal new_val(int_type_id);

        // Randomly pick kind of new constant
        ConstPattern::NewConstKind new_const_kind = rand_val_gen->get_rand_id(p->get_new_const_kind_prob());
        if (new_const_kind == ConstPattern::NewConstKind::EndBits) {
            bool use_lsb_end = rand_val_gen->get_rand_value(false, true);
            uint32_t fixed_block_point = rand_val_gen->get_rand_value(0U, tmp_int_type->get_bit_size() - 1);
            if (use_lsb_end)
                new_val.set_abs_val(fill_bits(0UL, fixed_block_point));
            else
                new_val.set_abs_val(fill_bits(fixed_block_point, tmp_int_type->get_bit_size() - 1));
        }
        else if (new_const_kind == ConstPattern::NewConstKind::BitBlock) {
            uint32_t block_start = rand_val_gen->get_rand_value(0U, tmp_int_type->get_bit_size() - 1);
            uint32_t block_end = rand_val_gen->get_rand_value(block_start, tmp_int_type->get_bit_size() - 1);
            new_val.set_abs_val(fill_bits(block_start, block_end));
        }
        else if (new_const_kind == ConstPattern::NewConstKind::MAX_NEW_CONST_KIND) {
            // Generate new one
            new_val = BuiltinType::ScalarTypedVal::generate(ctx, int_type_id);
        }
        else
            ERROR("Bad id for ConstPattern::NewConstKind");

        bit_log_const_buffer.push_back(new_val);
    }
}

template <typename T>
std::string ConstExpr::int_to_string(T T_val, T min, std::string suffix) {
    if (!std::static_pointer_cast<ScalarVariable>(value)->get_type()->get_is_signed())
        return std::to_string(T_val) + suffix;
    if (T_val != min)
        return std::to_string(T_val) + suffix;
    return "(" + std::to_string(min + 1) + suffix + " - 1" + suffix + ")";
}

void ConstExpr::emit (std::ostream& stream, std::string offset) {
    std::shared_ptr<ScalarVariable> scalar_val = std::static_pointer_cast<ScalarVariable>(value);
    std::string suffix = std::static_pointer_cast<BuiltinType>(scalar_val->get_type())->get_literal_suffix();
    auto val = scalar_val->get_cur_value().val;

    if (scalar_val->get_type()->is_int_type()) {
        std::shared_ptr<IntegerType> int_type = std::static_pointer_cast<IntegerType>(scalar_val->get_type());
        switch (scalar_val->get_type()->get_int_type_id()) {
            case IntegerType::IntegerTypeID::BOOL:
                stream << (val.bool_val ? "true" : "false");
                break;
            case IntegerType::IntegerTypeID::CHAR:
                stream << int_to_string(val.char_val, int_type->get_min().val.char_val, suffix);
                break;
            case IntegerType::IntegerTypeID::UCHAR:
                stream << int_to_string(val.uchar_val, int_type->get_min().val.uchar_val, suffix);
                break;
            case IntegerType::IntegerTypeID::SHRT:
                stream << int_to_string(val.shrt_val, int_type->get_min().val.shrt_val, suffix);
                break;
            case IntegerType::IntegerTypeID::USHRT:
                stream << int_to_string(val.ushrt_val, int_type->get_min().val.ushrt_val, suffix);
                break;
            case IntegerType::IntegerTypeID::INT:
                stream << int_to_string(val.int_val, int_type->get_min().val.int_val, suffix);
                break;
            case IntegerType::IntegerTypeID::UINT:
                stream << int_to_string(val.uint_val, int_type->get_min().val.uint_val, suffix);
                break;
            case IntegerType::IntegerTypeID::LINT:
                if (options->mode_64bit)
                    stream << int_to_string(val.lint64_val, int_type->get_min().val.lint64_val, suffix);
                else
                    stream << int_to_string(val.lint32_val, int_type->get_min().val.lint32_val, suffix);
                break;
            case IntegerType::IntegerTypeID::ULINT:
                if (options->mode_64bit)
                    stream << int_to_string(val.ulint64_val, int_type->get_min().val.ulint64_val, suffix);
                else
                    stream << int_to_string(val.ulint32_val, int_type->get_min().val.ulint32_val, suffix);
                break;
            case IntegerType::IntegerTypeID::LLINT:
                stream << int_to_string(val.llint_val, int_type->get_min().val.llint_val, suffix);
                break;
            case IntegerType::IntegerTypeID::ULLINT:
                stream << int_to_string(val.ullint_val, int_type->get_min().val.ullint_val, suffix);
                break;
            case IntegerType::IntegerTypeID::MAX_INT_ID:
                ERROR("bad int type id (Constexpr)");
        }
    }
    else if (scalar_val->get_type()->is_fp_type()) {
        switch (scalar_val->get_type()->get_fp_type_id()) {
            case FPType::FPTypeID::FLOAT:
                stream << std::to_string(val.float_val) << suffix;
                break;
            case FPType::FPTypeID::DOUBLE:
                stream << std::to_string(val.double_val) << suffix;
                break;
            case FPType::FPTypeID::LONG_DOUBLE:
                stream << std::to_string(val.long_double_val) << suffix;
                break;
            case FPType::FPTypeID::MAX_FP_ID:
                ERROR("bad fp type id (Constexpr)");
        }
    }
    else
        ERROR("unsupported type (Constexpr)");
}

ConstExpr::ConstExpr(BuiltinType::ScalarTypedVal _val) :
        Expr(Node::NodeID::CONST, nullptr, 1) {
    if (_val.is_int_type())
        value = std::make_shared<ScalarVariable>("", IntegerType::init(_val.get_int_type_id()));
    else if (_val.is_fp_type())
        value = std::make_shared<ScalarVariable>("", FPType::init(_val.get_fp_type_id()));
    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(_val);
}

std::shared_ptr<Expr> ArithExpr::integral_prom (std::shared_ptr<Expr> arg) {
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR)
        ERROR("can perform integral_prom only on ScalarVariable (ArithExpr)");

    if (arg->get_value()->get_type()->is_fp_type())
        return arg;

    if (!arg->get_value()->get_type()->get_is_bit_field()) {
        //[conv.prom]
        if (arg->get_value()->get_type()->get_int_type_id() >= IntegerType::IntegerTypeID::INT) // can't perform integral promotion
            return arg;
        return std::make_shared<TypeCastExpr>(arg, IntegerType::init(Type::IntegerTypeID::INT), true);
    }
    else {
        BuiltinType::ScalarTypedVal val = std::static_pointer_cast<ScalarVariable>(arg->get_value())->get_cur_value();
        if (BitField::can_fit_in_int(val, false))
            return std::make_shared<TypeCastExpr>(arg, IntegerType::init(Type::IntegerTypeID::INT), true);
        if (BitField::can_fit_in_int(val, true))
            return std::make_shared<TypeCastExpr>(arg, IntegerType::init(Type::IntegerTypeID::UINT), true);
        return arg;
    }
}

std::shared_ptr<Expr> ArithExpr::conv_to_bool (std::shared_ptr<Expr> arg) {
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("can perform conv_to_bool only on ScalarVariable (ArithExpr)");
    }

    IntegerType::IntegerTypeID to_type = IntegerType::IntegerTypeID::BOOL;
    if (options->is_c())
        to_type = IntegerType::IntegerTypeID::INT;

    if (arg->get_value()->get_type()->get_int_type_id() == to_type) // can't perform integral promotion
        return arg;
    return std::make_shared<TypeCastExpr>(arg, IntegerType::init(to_type), true);
}

GenPolicy ArithExpr::choose_and_apply_ssp_const_use (GenPolicy old_gen_policy) {
    if (old_gen_policy.get_chosen_arith_ssp_const_use () != ArithSSP::ConstUse::MAX_CONST_USE)
        return old_gen_policy;
    ArithSSP::ConstUse arith_ssp_const_use_id = rand_val_gen->get_rand_id (old_gen_policy.get_allowed_arith_ssp_const_use());
//    std::cerr << "arith_single_pattern_id: " << arith_single_pattern_id << std::endl;
    return old_gen_policy.apply_arith_ssp_const_use (arith_ssp_const_use_id);
}

GenPolicy ArithExpr::choose_and_apply_ssp_similar_op (GenPolicy old_gen_policy) {
    if (old_gen_policy.get_chosen_arith_ssp_similar_op () != ArithSSP::SimilarOp::MAX_SIMILAR_OP)
        return old_gen_policy;
    ArithSSP::SimilarOp arith_ssp_similar_op_id = rand_val_gen->get_rand_id (old_gen_policy.get_allowed_arith_ssp_similar_op());
//    std::cerr << "arith_single_pattern_id: " << arith_single_pattern_id << std::endl;
    return old_gen_policy.apply_arith_ssp_similar_op (arith_ssp_similar_op_id);
}

GenPolicy ArithExpr::choose_and_apply_ssp (GenPolicy gen_policy) {
    GenPolicy new_policy = choose_and_apply_ssp_const_use(gen_policy);
    new_policy = choose_and_apply_ssp_similar_op(new_policy);
    return new_policy;
}

std::shared_ptr<Expr> ArithExpr::generate (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp) {
    ConstExpr::fill_const_buf(ctx);
    return gen_level(ctx, inp, 0);
}

// Top-level recursive function for expression tree generation.
std::shared_ptr<Expr> ArithExpr::gen_level (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp,
                                            uint32_t par_depth) {
    auto p = ctx->get_gen_policy();
    //TODO: it is a stub for testing. Rewrite it later.
    // Pick random pattern for single statement and apply it to gen_policy. Update Context with new gen_policy.
    GenPolicy new_gen_policy;
    if (options->is_int_mode())
        new_gen_policy = choose_and_apply_ssp(*(p));
    else if (options->is_fp_mode())
        //TODO: do we need it for FP mode?
        new_gen_policy = *(p);
    else
        ERROR("bad mode");
    std::shared_ptr<Context> new_ctx = std::make_shared<Context>(*(ctx));
    new_ctx->set_gen_policy(new_gen_policy);

    // Pick random ID of the node being create.
    GenPolicy::ArithLeafID node_type = rand_val_gen->get_rand_id (p->get_arith_leaves());
    std::shared_ptr<Expr> ret = nullptr;

    // If we want to use any Data, we've reached expression tree depth limit or
    // total Arithmetic Expression number, or we want to use CSE but don't have any,
    // we fall into this branch.
    if (node_type == GenPolicy::ArithLeafID::Data || par_depth == p->get_max_arith_depth() ||
        (node_type == GenPolicy::ArithLeafID::CSE && p->get_cse().size() == 0) ||
        Expr::total_expr_count >= p->get_max_total_expr_count() ||
        Expr::func_expr_count  >= p->get_max_func_expr_count()) {
        // Pick random Data ID.
        GenPolicy::ArithDataID data_type = rand_val_gen->get_rand_id (p->get_arith_data_distr());
        // If we want to use Const or don't have any input VarUseExpr / MemberExpr, we fall into this branch.
        if (data_type == GenPolicy::ArithDataID::Const || inp.size() == 0) {
            ret = ConstExpr::generate(new_ctx);
        }
        // Branch for input VarUseExpr / MemberExpr
        else if (data_type == GenPolicy::ArithDataID::Inp) {
            ret = rand_val_gen->get_rand_elem(inp);
            if (ret->get_id() == Node::NodeID::VAR_USE)
                GenPolicy::add_to_complexity(Node::NodeID::VAR_USE);
            else if (ret->get_id() == Node::NodeID::MEMBER)
                GenPolicy::add_to_complexity(Node::NodeID::MEMBER);
            else {
                ERROR("unsupported input data type (ArithExpr)");
            }
        }
        else {
            ERROR("Ops (ArithExpr)");
        }
    }
    // All of the following branches implement direct or indirect recursion, calling ArithExpr::gen_level
    // Unary expr
    else if (node_type == GenPolicy::ArithLeafID::Unary)
        ret = UnaryExpr::generate(new_ctx, inp, par_depth + 1);
    // Binary expr
    else if (node_type == GenPolicy::ArithLeafID::Binary)
        ret = BinaryExpr::generate(new_ctx, inp, par_depth + 1);
    // Conditional (ternary) expr
    else if (node_type == GenPolicy::ArithLeafID::Conditional)
        ret = ConditionalExpr::generate(new_ctx, inp, par_depth + 1);
    // TypeCast expr
    else if (node_type == GenPolicy::ArithLeafID::TypeCast)
        ret = TypeCastExpr::generate(new_ctx, ArithExpr::gen_level(new_ctx, inp, par_depth + 1));
    // Use existing CSE
    else if (node_type == GenPolicy::ArithLeafID::CSE)
        ret = rand_val_gen->get_rand_elem(p->get_cse());
    else
        ERROR("inappropriate node type (ArithExpr)");
//    std::cout << ret->emit() << std::endl;
    if (options->is_fp_mode() &&
       (ret->get_raw_complexity().add_oper_count > p->get_max_arith_expr_add_complexity() ||
        ret->get_raw_complexity().mul_oper_count > p->get_max_arith_expr_mul_complexity())) {
        ret = ConstExpr::generate(new_ctx);
    }

    return ret;
}


std::shared_ptr<UnaryExpr> UnaryExpr::generate (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp, uint32_t par_depth) {
    GenPolicy::add_to_complexity(Node::NodeID::UNARY);
    UnaryExpr::Op op_type = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_allowed_unary_op());
    std::shared_ptr<Expr> rhs = ArithExpr::gen_level (ctx, inp, par_depth);
    return std::make_shared<UnaryExpr>(op_type, rhs);
}

void UnaryExpr::rebuild (UB ub) {
    switch (op) {
        case UnaryExpr::PreInc:
            op = Op::PreDec;
            break;
        case UnaryExpr::PostInc:
            op = Op::PostDec;
            break;
        case UnaryExpr::PreDec:
            op = Op::PreInc;
            break;
        case UnaryExpr::PostDec:
            op = Op::PostInc;
            break;
        case UnaryExpr::Negate:
            op = Op::Plus;
            break;
        case UnaryExpr::Plus:
        case UnaryExpr::LogNot:
        case UnaryExpr::BitNot:
            break;
        case UnaryExpr::MaxOp:
            ERROR("bad op (UnaryExpr)");
            break;
    }
    propagate_type();
    UB ret_ub = propagate_value();
    if (ret_ub) {
        ERROR("illegal strategy (UnaryExpr)");
    }
}

UnaryExpr::UnaryExpr (Op _op, std::shared_ptr<Expr> _arg) :
                       ArithExpr(Node::NodeID::UNARY, _arg->get_value()), op (_op), arg (_arg) {
    //TODO: add UB elimination strategy
    propagate_type();
    UB ret_ub = propagate_value();
    if (ret_ub != NoUB) {
        rebuild(ret_ub);
    }
    complexity = arg->get_raw_complexity();
    complexity.full_complexity = arg->get_full_complexity() + 1;
}

bool UnaryExpr::propagate_type () {
    if (op == MaxOp || arg == nullptr) {
        ERROR("bad args (UnaryExpr");
        return false;
    }

    //TODO: what about overloadedstruct operators?
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("can perform propagate_type only on ScalarVariable (UnaryExpr)");
    }

    switch (op) {
        case PreInc:
        case PreDec:
        case PostInc:
        case PostDec:
            break;
        case Plus:
        case Negate:
        case BitNot:
            arg = integral_prom (arg);
            break;
        case LogNot:
            arg = conv_to_bool (arg);
            break;
        case MaxOp:
            ERROR("bad op (UnaryExpr)");
            break;
    }
    value = arg->get_value();
    return true;
}

UB UnaryExpr::propagate_value () {
    if (op == MaxOp || arg == nullptr) {
        ERROR("bad op (UnaryExpr)");
        return NullPtr;
    }

    //TODO: what about overloadedstruct operators?
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("can perform propagate_value only on ScalarVariable (UnaryExpr)");
    }

    std::shared_ptr<ScalarVariable> scalar_val = std::static_pointer_cast<ScalarVariable>(arg->get_value());

    BuiltinType::ScalarTypedVal new_val (IntegerType::IntegerTypeID::MAX_INT_ID);
    if (scalar_val->get_type()->is_int_type())
        new_val = BuiltinType::ScalarTypedVal(scalar_val->get_type()->get_int_type_id());
    else if (scalar_val->get_type()->is_fp_type())
        new_val = BuiltinType::ScalarTypedVal(scalar_val->get_type()->get_fp_type_id());
    else
        ERROR("unsupported type");

    switch (op) {
        case PreInc:
        case PostInc:
            new_val = scalar_val->get_cur_value()++;
            break;
        case PreDec:
        case PostDec:
            new_val = scalar_val->get_cur_value()--;
            break;
        case Plus:
            new_val = scalar_val->get_cur_value();
            break;
        case Negate:
            new_val = -scalar_val->get_cur_value();
            break;
        case BitNot:
            new_val = ~scalar_val->get_cur_value();
            break;
        case LogNot:
            new_val = !scalar_val->get_cur_value();
            break;
        case MaxOp:
            ERROR("bad op (UnaryExpr)");
            break;
    }

    if (!new_val.has_ub())
        std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
    return new_val.get_ub();
}

// This function rebuilds Unary expression in case of UB.
// The main idea is to replace operator by its complementary operator.
// This trick always works for unary operations.
void UnaryExpr::emit (std::ostream& stream, std::string offset) {
    std::string op_str = offset;
    switch (op) {
        case PreInc:
        case PostInc:
            op_str = "++";
            break;
        case PreDec:
        case PostDec:
            op_str = "--";
            break;
        case Plus:
            op_str = "+";
            break;
        case Negate:
            op_str = "-";
            break;
        case LogNot:
            op_str = "!";
            break;
        case BitNot:
            op_str = "~";
            break;
        case MaxOp:
            ERROR("bad op (UnaryExpr)");
            break;
    }
    std::string ret = "";
    if (op == PostInc || op == PostDec) {
        stream << "(";
        arg->emit(stream);
        stream << ")" + op_str;
    }
    else {
        stream << op_str + "(";
        arg->emit(stream);
        stream << ")";
    }
}

std::shared_ptr<BinaryExpr> BinaryExpr::generate (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp, uint32_t par_depth) {
    GenPolicy::add_to_complexity(Node::NodeID::BINARY);
    BinaryExpr::Op op_type = rand_val_gen->get_rand_id(ctx->get_gen_policy()->get_allowed_binary_op());
    std::shared_ptr<Expr> lhs = ArithExpr::gen_level (ctx, inp, par_depth);
    std::shared_ptr<Expr> rhs = ArithExpr::gen_level (ctx, inp, par_depth);
    std::shared_ptr<BinaryExpr> ret = std::make_shared<BinaryExpr>(op_type, lhs, rhs);
/*
    std::cout << "lhs: " << std::static_pointer_cast<ScalarVariable>(lhs->get_value())->get_cur_value() << std::endl;
    std::cout << "rhs: " << std::static_pointer_cast<ScalarVariable>(rhs->get_value())->get_cur_value() << std::endl;
    std::cout << "ret: " << std::static_pointer_cast<ScalarVariable>(ret->get_value())->get_cur_value() << std::endl;
*/
    return ret;
}

BinaryExpr::BinaryExpr (Op _op, std::shared_ptr<Expr> lhs, std::shared_ptr<Expr> rhs) :
                        ArithExpr(Node::NodeID::BINARY, nullptr), op(_op), arg0(lhs), arg1(rhs) {
    propagate_type();
    UB ret_ub = propagate_value();
    if (ret_ub != NoUB) {
        rebuild(ret_ub);
    }
    //TODO it looks weird, but it is ok for now
    complexity.add_oper_count = arg0->get_raw_complexity().add_oper_count +
                                arg1->get_raw_complexity().add_oper_count;
    complexity.mul_oper_count = arg0->get_raw_complexity().mul_oper_count +
                                arg1->get_raw_complexity().mul_oper_count;
    complexity.full_complexity = arg0->get_full_complexity() + 1 + arg1->get_full_complexity();
    if (op == Add) {
        complexity.add_oper_count = arg0->get_raw_complexity().add_oper_count +
                                    arg1->get_raw_complexity().add_oper_count + 1;
        complexity.mul_oper_count = std::max(arg0->get_raw_complexity().mul_oper_count,
                                             arg1->get_raw_complexity().mul_oper_count);
    }
    else if (op == Mul) {
        complexity.add_oper_count = (arg0->get_raw_complexity().add_oper_count + 1) *
                                    (arg1->get_raw_complexity().add_oper_count + 1) - 1;
        complexity.mul_oper_count = arg0->get_raw_complexity().mul_oper_count +
                                    arg1->get_raw_complexity().mul_oper_count + 1;
    }
}

static uint64_t msb(uint64_t x) {
    uint64_t ret = 0;
    while (x != 0) {
        ret++;
        x = x >> 1;
    }
    return ret;
}

// This function rebuilds Binary expression in case of UB.
// The main idea is to replace operator by its complementary operator.
// This works pretty well in most cases.
// If it doesn't work, we insert child nodes to change operands.
void BinaryExpr::rebuild (UB ub) {
    //TODO: We should implement more rebuild strategies (e.g. regenerate node)
    switch (op) {
        case BinaryExpr::Add:
            op = Sub;
            break;
        case BinaryExpr::Sub:
            op = Add;
            break;
        case BinaryExpr::Mul:
            if (ub == UB::SignOvfMin)
                op = Sub;
            else
                op = Div;
            break;
        case BinaryExpr::Div:
        case BinaryExpr::Mod:
            if (ub == UB::ZeroDiv)
               op = Mul;
            else
               op = Sub;
            break;
        // Shift operators are tricky.
        case BinaryExpr::Shr:
        case BinaryExpr::Shl:
            //TODO: We should rewrite it later. It is awful.
            if ((ub == UB::ShiftRhsNeg) || (ub == UB::ShiftRhsLarge)) {
                std::shared_ptr<Expr> lhs = arg0;
                std::shared_ptr<Expr> rhs = arg1;
                // First of all, we need to find maximum value which can be used as rhs.
                std::shared_ptr<IntegerType> lhs_int_type = std::static_pointer_cast<IntegerType>(lhs->get_value()->get_type());
                uint64_t max_sht_val = lhs_int_type->get_bit_size();
                if ((op == Shl) && (lhs_int_type->get_is_signed()) && (ub == UB::ShiftRhsLarge))
                    max_sht_val -= msb((uint64_t)std::static_pointer_cast<ScalarVariable>(lhs->get_value())->get_cur_value().get_abs_val());
                // Second, we randomly choose value between 0 and maximum rhs value.
                uint64_t const_val = rand_val_gen->get_rand_value(0UL, max_sht_val);
                // Third, we combine chosen value with existing rhs
                uint64_t rhs_abs_val = std::static_pointer_cast<ScalarVariable>(rhs->get_value())->get_cur_value().get_abs_val();
                std::shared_ptr<IntegerType> rhs_int_type = std::static_pointer_cast<IntegerType>(rhs->get_value()->get_type());
                if (ub == UB::ShiftRhsNeg) {
                    const_val += rhs_abs_val;
                    const_val = std::min(const_val, rhs_int_type->get_max().get_abs_val());// TODO: it won't work with INT_MIN
                }
                else {
                    const_val = rhs_abs_val - const_val;
                }

                // And finally we insert new child node with corresponding additive operator
                BuiltinType::ScalarTypedVal const_ins_val (rhs_int_type->get_int_type_id());
                const_ins_val.set_abs_val (const_val);
                std::shared_ptr<ConstExpr> const_ins = std::make_shared<ConstExpr>(const_ins_val);
                if (ub == UB::ShiftRhsNeg)
                    arg1 = std::make_shared<BinaryExpr>(Add, arg1, const_ins);
                else // UB::ShiftRhsLarge
                    arg1 = std::make_shared<BinaryExpr>(Sub, arg1, const_ins);
            }
            // UB::NegShift
            else {
                // It is simple - we always add MAX value to existing lhs
                std::shared_ptr<Expr> lhs = arg0;
                std::shared_ptr<IntegerType> lhs_int_type = std::static_pointer_cast<IntegerType>(lhs->get_value()->get_type());
                uint64_t const_val = lhs_int_type->get_max().get_abs_val();
                BuiltinType::ScalarTypedVal const_ins_val(lhs_int_type->get_int_type_id());
                const_ins_val.set_abs_val (const_val);
                std::shared_ptr<ConstExpr> const_ins = std::make_shared<ConstExpr>(const_ins_val);
                arg0 = std::make_shared<BinaryExpr>(Add, arg0, const_ins);
            }
            break;
        case BinaryExpr::Lt:
        case BinaryExpr::Gt:
        case BinaryExpr::Le:
        case BinaryExpr::Ge:
        case BinaryExpr::Eq:
        case BinaryExpr::Ne:
        case BinaryExpr::BitAnd:
        case BinaryExpr::BitOr:
        case BinaryExpr::BitXor:
        case BinaryExpr::LogAnd:
        case BinaryExpr::LogOr:
            break;
        case BinaryExpr::MaxOp:
        case BinaryExpr::Ter:
            ERROR("invalid Op (ArithExprGen)");
            break;
    }
    propagate_type();
    UB ret_ub = propagate_value();
    if (ret_ub != NoUB) {
        rebuild(ret_ub);
    }
}

void BinaryExpr::perform_arith_conv () {
    if (arg0->get_value()->get_type()->is_fp_type() || arg1->get_value()->get_type()->is_fp_type()) {
        FPType::FPTypeID arg0_fp_type_id;
        FPType::FPTypeID arg1_fp_type_id;
        if (arg0->get_value()->get_type()->is_fp_type() && !arg1->get_value()->get_type()->is_fp_type()) {
            arg0_fp_type_id = arg0->get_value()->get_type()->get_fp_type_id();
            arg1 = std::make_shared<TypeCastExpr>(arg1, FPType::init(arg0_fp_type_id), true);
            return;
        }
        if (!arg0->get_value()->get_type()->is_fp_type() && arg1->get_value()->get_type()->is_fp_type()) {
            arg1_fp_type_id = arg1->get_value()->get_type()->get_fp_type_id();
            arg0 = std::make_shared<TypeCastExpr>(arg0, FPType::init(arg1_fp_type_id), true);
            return;
        }

        if (arg0->get_value()->get_type()->get_fp_type_id() == arg1->get_value()->get_type()->get_fp_type_id())
            return;

        arg0_fp_type_id = arg0->get_value()->get_type()->get_fp_type_id();
        arg1_fp_type_id = arg1->get_value()->get_type()->get_fp_type_id();
        std::shared_ptr<Type> cast_to_type = FPType::init(std::max(arg0_fp_type_id, arg1_fp_type_id));
        if (arg0_fp_type_id > arg1_fp_type_id) {
            arg1 = std::make_shared<TypeCastExpr>(arg1, cast_to_type, true);
            return;
        }
        if (arg0_fp_type_id < arg1_fp_type_id) {
            arg0 = std::make_shared<TypeCastExpr>(arg0, cast_to_type, true);
            return;
        }
        ERROR("unreachable code for fp type");
    }
    // integral promotion should be a part of it, but it was moved to base class
    // 10.5.1
    if (arg0->get_value()->get_type()->get_int_type_id() == arg1->get_value()->get_type()->get_int_type_id())
        return;
    // 10.5.2
    if (arg0->get_value()->get_type()->get_is_signed() == arg1->get_value()->get_type()->get_is_signed()) {
        std::shared_ptr<Type> cast_to_type = IntegerType::init(std::max(arg0->get_value()->get_type()->get_int_type_id(),
                                                                        arg1->get_value()->get_type()->get_int_type_id()));
        if (arg0->get_value()->get_type()->get_int_type_id() <  arg1->get_value()->get_type()->get_int_type_id()) {
            arg0 = std::make_shared<TypeCastExpr>(arg0, cast_to_type, true);
        }
        else {
            arg1 = std::make_shared<TypeCastExpr>(arg1, cast_to_type, true);
        }
        return;
    }
    if ((!arg0->get_value()->get_type()->get_is_signed() && 
         (arg0->get_value()->get_type()->get_int_type_id() >= arg1->get_value()->get_type()->get_int_type_id())) || // 10.5.3
         (arg0->get_value()->get_type()->get_is_signed() && 
          IntegerType::can_repr_value (arg1->get_value()->get_type()->get_int_type_id(), arg0->get_value()->get_type()->get_int_type_id()))) { // 10.5.4
        arg1 = std::make_shared<TypeCastExpr>(arg1, IntegerType::init(arg0->get_value()->get_type()->get_int_type_id()), true);
        return;
    }
    if ((!arg1->get_value()->get_type()->get_is_signed() &&
         (arg1->get_value()->get_type()->get_int_type_id() >= arg0->get_value()->get_type()->get_int_type_id())) || // 10.5.3
         (arg1->get_value()->get_type()->get_is_signed() &&
          IntegerType::can_repr_value (arg0->get_value()->get_type()->get_int_type_id(), arg1->get_value()->get_type()->get_int_type_id()))) { // 10.5.4
        arg0 = std::make_shared<TypeCastExpr>(arg0, IntegerType::init(arg1->get_value()->get_type()->get_int_type_id()), true);
        return;
    }
    // 10.5.5
    if (arg0->get_value()->get_type()->get_is_signed()) {
        std::shared_ptr<Type> cast_to_type = IntegerType::init(IntegerType::get_corr_unsig(arg0->get_value()->get_type()->get_int_type_id()));
        arg0 = std::make_shared<TypeCastExpr>(arg0, cast_to_type, true);
        arg1 = std::make_shared<TypeCastExpr>(arg1, cast_to_type, true);
    }
    if (arg1->get_value()->get_type()->get_is_signed()) {
        std::shared_ptr<Type> cast_to_type = IntegerType::init(IntegerType::get_corr_unsig(arg1->get_value()->get_type()->get_int_type_id()));
        arg0 = std::make_shared<TypeCastExpr>(arg0, cast_to_type, true);
        arg1 = std::make_shared<TypeCastExpr>(arg1, cast_to_type, true);
    }
}

bool BinaryExpr::propagate_type () {
    if (op == MaxOp || arg0 == nullptr || arg1 == nullptr) {
        ERROR("bad args (BinaryExpr)");
    }

    //TODO: what about overloaded struct operators?
    if (arg0->get_value()->get_class_id() != Data::VarClassID::VAR ||
        arg1->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("can perform propagate_type only on ScalarVariable (BinaryExpr)");
    }

    switch (op) {
        case Add:
        case Sub:
        case Mul:
        case Div:
        case Mod:
        case Lt:
        case Gt:
        case Le:
        case Ge:
        case Eq:
        case Ne:
        case BitAnd:
        case BitXor:
        case BitOr:
        case Ter:
            // arithmetic conversions
            arg0 = integral_prom (arg0);
            arg1 = integral_prom (arg1);
            perform_arith_conv();
            break;
        case Shl:
        case Shr:
            arg0 = integral_prom (arg0);
            arg1 = integral_prom (arg1);
            break;
        case LogAnd:
        case LogOr:
            arg0 = conv_to_bool (arg0);
            arg1 = conv_to_bool (arg1);
            break;
        case MaxOp:
            ERROR("bad op (BinaryExpr)");
            break;
    }
    return true;
}

UB BinaryExpr::propagate_value () {
    if (op == MaxOp || arg0 == nullptr || arg1 == nullptr) {
        ERROR("bad args (BinaryExpr)");
    }

    //TODO: what about overloaded struct operators?
    if (arg0->get_value()->get_class_id() != Data::VarClassID::VAR ||
        arg1->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("can perform propagate_value only on ScalarVariable (BinaryExpr)");
    }

    // Value propagation of ternary operator is handled differently
    if (op == BinaryExpr::Ter)
        return UB::NoUB;

    std::shared_ptr<ScalarVariable> scalar_lhs = std::static_pointer_cast<ScalarVariable>(arg0->get_value());
    std::shared_ptr<ScalarVariable> scalar_rhs = std::static_pointer_cast<ScalarVariable>(arg1->get_value());

    BuiltinType::ScalarTypedVal new_val (IntegerType::IntegerTypeID::MAX_INT_ID);
    if (scalar_lhs->get_type()->is_int_type())
        new_val = BuiltinType::ScalarTypedVal(scalar_lhs->get_type()->get_int_type_id());
    else if (scalar_lhs->get_type()->is_fp_type())
        new_val = BuiltinType::ScalarTypedVal(scalar_lhs->get_type()->get_fp_type_id());
    else
        ERROR("unsupported type");


/*
    std::cout << "Before prop:" << std::endl;
    std::cout << arg0->emit() << std::endl;
    std::cout << "lhs: " << std::static_pointer_cast<ScalarVariable>(arg0->get_value())->get_cur_value() << std::endl;
    std::cout << "lhs val id: " << std::static_pointer_cast<ScalarVariable>(arg0->get_value())->get_cur_value().get_int_type_id() << std::endl;
    std::cout << "lhs id: " << arg0->get_value()->get_type()->get_int_type_id() << std::endl;
    std::cout << arg1->emit() << std::endl;
    std::cout << "rhs: " << std::static_pointer_cast<ScalarVariable>(arg1->get_value())->get_cur_value() << std::endl;
    std::cout << "rhs val id: " << std::static_pointer_cast<ScalarVariable>(arg1->get_value())->get_cur_value().get_int_type_id() << std::endl;
    std::cout << "rhs id: " << arg1->get_value()->get_type()->get_int_type_id() << std::endl;
*/

    switch (op) {
        case Add:
            new_val = scalar_lhs->get_cur_value() + scalar_rhs->get_cur_value();
            break;
        case Sub:
            new_val = scalar_lhs->get_cur_value() - scalar_rhs->get_cur_value();
            break;
        case Mul:
            new_val = scalar_lhs->get_cur_value() * scalar_rhs->get_cur_value();
            break;
        case Div:
            new_val = scalar_lhs->get_cur_value() / scalar_rhs->get_cur_value();
            break;
        case Mod:
            new_val = scalar_lhs->get_cur_value() % scalar_rhs->get_cur_value();
            break;
        case Lt:
            new_val = scalar_lhs->get_cur_value() < scalar_rhs->get_cur_value();
            break;
        case Gt:
            new_val = scalar_lhs->get_cur_value() > scalar_rhs->get_cur_value();
            break;
        case Le:
            new_val = scalar_lhs->get_cur_value() <= scalar_rhs->get_cur_value();
            break;
        case Ge:
            new_val = scalar_lhs->get_cur_value() >= scalar_rhs->get_cur_value();
            break;
        case Eq:
            new_val = scalar_lhs->get_cur_value() == scalar_rhs->get_cur_value();
            break;
        case Ne:
            new_val = scalar_lhs->get_cur_value() != scalar_rhs->get_cur_value();
            break;
        case BitAnd:
            new_val = scalar_lhs->get_cur_value() & scalar_rhs->get_cur_value();
            break;
        case BitOr:
            new_val = scalar_lhs->get_cur_value() | scalar_rhs->get_cur_value();
            break;
        case BitXor:
            new_val = scalar_lhs->get_cur_value() ^ scalar_rhs->get_cur_value();
            break;
        case LogAnd:
            new_val = scalar_lhs->get_cur_value() && scalar_rhs->get_cur_value();
            break;
        case LogOr:
            new_val = scalar_lhs->get_cur_value() || scalar_rhs->get_cur_value();
            break;
        case Shl:
            new_val = scalar_lhs->get_cur_value() << scalar_rhs->get_cur_value();
            break;
        case Shr:
            new_val = scalar_lhs->get_cur_value() >> scalar_rhs->get_cur_value();
            break;
        case Ter:
        case MaxOp:
            ERROR("bad op (BinaryExpr)");
            break;
    }

    if (!new_val.has_ub()) {
        if (new_val.is_int_type())
            value = std::make_shared<ScalarVariable>("", IntegerType::init(new_val.get_int_type_id()));
        else if (new_val.is_fp_type())
            value = std::make_shared<ScalarVariable>("", FPType::init(new_val.get_fp_type_id()));
        else
            ERROR("unsupported type");
        std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
    }
    else {
        if (new_val.is_int_type())
            value = std::make_shared<ScalarVariable>("", IntegerType::init(arg0->get_value()->get_type()->get_int_type_id()));
        else if (new_val.is_fp_type())
            value = std::make_shared<ScalarVariable>("", FPType::init(arg0->get_value()->get_type()->get_fp_type_id()));
        else
            ERROR("unsupported type");
    }
/*
    std::cout << "After prop:" << std::endl;
    std::cout << "lhs: " << std::static_pointer_cast<ScalarVariable>(arg0->get_value())->get_cur_value() << std::endl;
    std::cout << "lhs id: " << arg0->get_value()->get_type()->get_int_type_id() << std::endl;
    std::cout << "rhs: " << std::static_pointer_cast<ScalarVariable>(arg1->get_value())->get_cur_value() << std::endl;
    std::cout << "rhs id: " << arg1->get_value()->get_type()->get_int_type_id() << std::endl;
    std::cout << "new_val: " << new_val << std::endl;
    std::cout << "new id: " << new_val.get_int_type_id() << std::endl;
    std::cout << "UB: " << new_val.get_ub() << std::endl;
    std::cout << "ret: " << std::static_pointer_cast<ScalarVariable>(value)->get_cur_value() << std::endl;
    std::cout << "=============" << std::endl;
*/
    return new_val.get_ub();
}

void BinaryExpr::emit (std::ostream& stream, std::string offset) {
    stream << offset << "(";
    arg0->emit(stream);
    stream << ")";
    switch (op) {
        case Add:
            stream << " + ";
            break;
        case Sub:
            stream << " - ";
            break;
        case Mul:
            stream << " * ";
            break;
        case Div:
            stream << " / ";
            break;
        case Mod:
            stream << " % ";
            break;
        case Shl:
            stream << " << ";
            break;
        case Shr:
            stream << " >> ";
            break;
        case Lt:
            stream << " < ";
            break;
        case Gt:
            stream << " > ";
            break;
        case Le:
            stream << " <= ";
            break;
        case Ge:
            stream << " >= ";
            break;
        case Eq:
            stream << " == ";
            break;
        case Ne:
            stream << " != ";
            break;
        case BitAnd:
            stream << " & ";
            break;
        case BitXor:
            stream << " ^ ";
            break;
        case BitOr:
            stream << " | ";
            break;
        case LogAnd:
            stream << " && ";
            break;
        case LogOr:
            stream << " || ";
            break;
        case Ter:
        case MaxOp:
            ERROR("bad op (BinaryExpr)");
            break;
        }
        stream << "(";
        arg1->emit(stream);
        stream << ")";
}

ConditionalExpr::ConditionalExpr (std::shared_ptr<Expr> _cond, std::shared_ptr<Expr> lhs, std::shared_ptr<Expr> rhs) :
                                  BinaryExpr(BinaryExpr::Op::Ter, lhs, rhs), condition(_cond) {
    condition = conv_to_bool(condition);
    propagate_value();
    complexity.full_complexity = condition->get_full_complexity() +
                                 arg0->get_full_complexity() + arg1->get_full_complexity() + 1;
}

UB ConditionalExpr::propagate_value() {
    if (condition == nullptr) {
        ERROR("bad args (ConditionalExpr)");
    }

    //TODO: what about overloaded struct operators?
    if (condition->get_value()->get_class_id() != Data::VarClassID::VAR)
        ERROR("can perform propagate_value only on ScalarVariable (ConditionalExpr)");

    std::shared_ptr<ScalarVariable> scalar_cond = std::static_pointer_cast<ScalarVariable>(condition->get_value());
    // All other check are done in BinaryExpr constructor
    std::shared_ptr<ScalarVariable> scalar_lhs = std::static_pointer_cast<ScalarVariable>(arg0->get_value());
    std::shared_ptr<ScalarVariable> scalar_rhs = std::static_pointer_cast<ScalarVariable>(arg1->get_value());
    BuiltinType::ScalarTypedVal new_val (IntegerType::IntegerTypeID::MAX_INT_ID);
    if (scalar_lhs->get_type()->is_int_type())
        new_val = BuiltinType::ScalarTypedVal(scalar_lhs->get_type()->get_int_type_id());
    else if (scalar_lhs->get_type()->is_fp_type())
        new_val = BuiltinType::ScalarTypedVal(scalar_lhs->get_type()->get_fp_type_id());
    else
        ERROR("unsupported type");

    bool cond_val = options->is_cxx() ? scalar_cond->get_cur_value().val.bool_val :
                                        (bool) scalar_cond->get_cur_value().val.int_val;
    new_val = cond_val ? scalar_lhs->get_cur_value() : scalar_rhs->get_cur_value();

    complexity = cond_val ? arg0->get_raw_complexity() : arg1->get_raw_complexity();

    if (new_val.is_int_type())
        value = std::make_shared<ScalarVariable>("", IntegerType::init(new_val.get_int_type_id()));
    else if (new_val.is_fp_type())
        value = std::make_shared<ScalarVariable>("", FPType::init(new_val.get_fp_type_id()));
    else
        ERROR("unsupported type");

    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);

    return UB::NoUB;
}

void ConditionalExpr::emit (std::ostream& stream, std::string offset) {
    stream << offset << "((";
    condition->emit(stream);
    stream << ") ? (";
    arg0->emit(stream);
    stream << ") : (";
    arg1->emit(stream);
    stream << "))";
}

std::shared_ptr<ConditionalExpr> ConditionalExpr::generate (
        std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp, int par_depth) {
    GenPolicy::add_to_complexity(Node::NodeID::BINARY);
    std::shared_ptr<Expr> cond = ArithExpr::gen_level (ctx, inp, par_depth);
    std::shared_ptr<Expr> lhs = ArithExpr::gen_level (ctx, inp, par_depth);
    std::shared_ptr<Expr> rhs = ArithExpr::gen_level (ctx, inp, par_depth);
    std::shared_ptr<ConditionalExpr> ret = std::make_shared<ConditionalExpr>(cond, lhs, rhs);
    return ret;
}

bool MemberExpr::propagate_type () {
    if (struct_var == nullptr && member_expr == nullptr) {
        ERROR("bad struct_var or member_expr (MemberExpr)");
    }

    if (struct_var != nullptr) {
        if (struct_var->get_member_count() <= identifier) {
            ERROR("bad identifier (MemberExpr)");
        }
        value = struct_var;
    }
    else {
        std::shared_ptr<Data> member_expr_data = member_expr->get_value();
        if (member_expr_data->get_class_id() != Data::VarClassID::STRUCT) {
            ERROR("can take member only from Struct (MemberExpr)");
        }
        std::shared_ptr<Struct> member_expr_struct = std::static_pointer_cast<Struct>(member_expr_data);
        if (member_expr_struct->get_member_count() <= identifier) {
            ERROR("bad identifier (MemberExpr)");
        }
        value = member_expr_struct;
    }
    return true;
}

UB MemberExpr::propagate_value () {
    if (struct_var == nullptr && member_expr == nullptr) {
        ERROR("bad struct_var or member_expr (MemberExpr)");
    }

    if (struct_var != nullptr) {
        if (struct_var->get_member_count() <= identifier) {
            ERROR("bad identifier (MemberExpr)");
        }
        value = struct_var->get_member(identifier);
    }
    else {
        std::shared_ptr<Data> member_expr_data = member_expr->get_value();
        if (member_expr_data->get_class_id() != Data::VarClassID::STRUCT) {
            ERROR("can take member only from Struct (MemberExpr)");
        }
        std::shared_ptr<Struct> member_expr_struct = std::static_pointer_cast<Struct>(member_expr_data);
        if (member_expr_struct->get_member_count() <= identifier) {
            ERROR("bad identifier (MemberExpr)");
        }
        value = member_expr_struct->get_member(identifier);
    }
    return NoUB;
}

std::shared_ptr<Expr> MemberExpr::set_value (std::shared_ptr<Expr> _expr) {
    //TODO: what about struct?
    std::shared_ptr<Data> _new_value = _expr->get_value();
    if (_new_value->get_class_id() != value->get_class_id()) {
        ERROR("different Data::VarClassID (MemberExpr)");
    }
    switch (value->get_class_id()) {
        case Data::VarClassID::VAR:
            if (value->get_type()->get_int_type_id() != _new_value->get_type()->get_int_type_id() ||
                value->get_type()->get_fp_type_id() != _new_value->get_type()->get_fp_type_id()) {
                ERROR("can't assign different types (MemberExpr)");
            }
            if (value->get_type()->get_is_bit_field())
                return check_and_set_bit_field(_expr);
            else {
                std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(_new_value)->get_cur_value());
                value->get_raw_complexity() = _expr->get_raw_complexity();
                return _expr;
            }
            break;
        case Data::VarClassID::STRUCT:
            //TODO: implement for Struct
            ERROR("Struct is unsupported (MemberExpr)");
            break;
        case Data::VarClassID::ARRAY:
            //TODO: implement for Array
            ERROR("Array is unsupported (MemberExpr)");
            break;
        case Data::VarClassID::MAX_CLASS_ID:
            ERROR("unsupported Data::VarClassID (MemberExpr)");
    }
    ERROR("MemberExpr::set_value() - data corruption");
}

static std::shared_ptr<Expr> change_to_value(std::shared_ptr<Context> ctx, std::shared_ptr<Expr> _expr, BuiltinType::ScalarTypedVal to_val) {
    std::shared_ptr<Data> expr_data = _expr->get_value();
    if (expr_data->get_class_id() != Data::VarClassID::VAR) {
        ERROR("only variables are supported");
    }
    BuiltinType::ScalarTypedVal value = std::static_pointer_cast<ScalarVariable>(expr_data)->get_cur_value();
    std::shared_ptr<ConstExpr> const_expr = std::make_shared<ConstExpr>(value);
    std::shared_ptr<Expr> to_zero =  std::make_shared<BinaryExpr>(BinaryExpr::Op::Sub, _expr, const_expr);
    std::shared_ptr<ConstExpr> to_val_const_expr = std::make_shared<ConstExpr>(to_val);
    return std::make_shared<BinaryExpr>(BinaryExpr::Op::Add, to_zero, to_val_const_expr);
}

std::shared_ptr<Expr> MemberExpr::check_and_set_bit_field (std::shared_ptr<Expr> _expr) {
    BuiltinType::ScalarTypedVal new_val = std::static_pointer_cast<ScalarVariable>(_expr->get_value())->get_cur_value();
    std::shared_ptr<BitField> bit_field = std::static_pointer_cast<BitField>(value->get_type());
    BuiltinType::ScalarTypedVal ovf_cmp_val = (bit_field->get_min() > new_val) || (bit_field->get_max() < new_val);
    if (!ovf_cmp_val.val.bool_val) {
        std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
        value->get_raw_complexity() = _expr->get_raw_complexity();
        return _expr;
    }
    //TODO: it is a stub. We need to change it
    GenPolicy gen_policy;
    Context ctx_var (gen_policy, nullptr, Node::NodeID::MAX_STMT_ID, true);
    ctx_var.set_local_sym_table(std::make_shared<SymbolTable>());
    std::shared_ptr<Context> ctx = std::make_shared<Context>(ctx_var);
    BuiltinType::ScalarTypedVal to_value = BuiltinType::ScalarTypedVal::generate(ctx, bit_field->get_min(), bit_field->get_max());
    std::shared_ptr<Expr> ret = change_to_value(ctx, _expr, to_value);

    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(ret->get_value())->get_cur_value());
    value->get_raw_complexity() = ret->get_raw_complexity();
    return ret;
}

void MemberExpr::emit (std::ostream& stream, std::string offset) {
    std::string ret = offset;
    if (struct_var == nullptr && member_expr == nullptr) {
        ERROR("bad struct_var or member_expr (MemberExpr)");
    }

    if (struct_var != nullptr) {
        if (struct_var->get_member_count() <= identifier) {
            ERROR("bad identifier (MemberExpr)");
        }
        stream << struct_var->get_name() + "." + struct_var->get_member(identifier)->get_name();
    }
    else {
        std::shared_ptr<Data> member_expr_data = member_expr->get_value();
        if (member_expr_data->get_class_id() != Data::VarClassID::STRUCT) {
            ERROR("can take member only from Struct (MemberExpr)");
        }
        std::shared_ptr<Struct> member_expr_struct = std::static_pointer_cast<Struct>(member_expr_data);
        if (member_expr_struct->get_member_count() <= identifier) {
            ERROR("bad identifier (MemberExpr)");
        }
        member_expr->emit(stream);
        stream << "." +  member_expr_struct->get_member(identifier)->get_name();
    }
}

MemberExpr::MemberExpr(std::shared_ptr<Struct> _struct, uint64_t _identifier) :
        Expr(Node::NodeID::MEMBER, _struct, 1), member_expr(nullptr), struct_var(_struct), identifier(_identifier) {
    propagate_type();
    propagate_value();
    complexity = struct_var->get_raw_complexity();
}

MemberExpr::MemberExpr(std::shared_ptr<MemberExpr> _member_expr, uint64_t _identifier) :
        Expr(Node::NodeID::MEMBER, _member_expr->get_value(), _member_expr->get_full_complexity() + 1),
        member_expr(_member_expr), struct_var(nullptr), identifier(_identifier) {
    propagate_type();
    propagate_value();
    complexity = member_expr->get_raw_complexity();
}

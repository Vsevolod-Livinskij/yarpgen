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

#include "expr.h"
#include "ir_node.h"
#include "gen_policy.h"
#include "sym_table.h"
#include "type.h"
#include "util.h"
#include "variable.h"

using namespace yarpgen;

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
        case Data::VarClassID::MAX_CLASS_ID:
            ERROR("unsupported Data::VarClassID (Expr)");
    }
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
            return _expr;
            break;
        case Data::VarClassID::STRUCT:
            //TODO: implement for Struct
            ERROR("struct is unsupported (VarUseExpr)");
            break;
        case Data::VarClassID::MAX_CLASS_ID:
            ERROR("unsupported Data::VarClassID (VarUseExpr)");
    }
}

VarUseExpr::VarUseExpr(std::shared_ptr<Data> _var) : Expr(Node::NodeID::VAR_USE, _var) {
}

AssignExpr::AssignExpr (std::shared_ptr<Expr> _to, std::shared_ptr<Expr> _from, bool _taken) :
                        Expr(Node::NodeID::ASSIGN, _to->get_value()), to(_to), from(_from), taken(_taken) {
    if (to->get_id() != Node::NodeID::VAR_USE && to->get_id() != Node::NodeID::MEMBER) {
        ERROR("can assign only to variable (AssignExpr)");
    }
    propagate_type();
    propagate_value();
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

std::string AssignExpr::emit (std::string offset) {
    std::string ret = offset;
    ret += to->emit();
    ret += " = ";
    ret += from->emit();
    return ret;
}

TypeCastExpr::TypeCastExpr (std::shared_ptr<Expr> _expr, std::shared_ptr<Type> _type, bool _is_implicit) :
              Expr(Node::NodeID::TYPE_CAST, nullptr), expr(_expr), to_type(_type), is_implicit(_is_implicit) {
    propagate_type();
    propagate_value();
}

bool TypeCastExpr::propagate_type () {
    if (to_type->get_int_type_id() == Type::IntegerTypeID::MAX_INT_ID ||
        expr->get_value()->get_type()->get_int_type_id() == Type::IntegerTypeID::MAX_INT_ID) {
        //TODO: what about overloaded struct types cast?
        ERROR("can cast only integer types (TypeCastExpr)");
    }
    return true;
}

UB TypeCastExpr::propagate_value () {
    if (expr->get_value()->get_class_id() != Data::VarClassID::VAR) {
        //TODO: what about overloaded struct types cast?
        ERROR("can cast only integer types (TypeCastExpr)");
    }
    //TODO: Is it always safe to cast value to ScalarVariable?
    value = std::make_shared<ScalarVariable>("", std::static_pointer_cast<IntegerType>(to_type));
    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(expr->get_value())->get_cur_value().cast_type(to_type->get_int_type_id()));
    return NoUB;
}

std::shared_ptr<TypeCastExpr> TypeCastExpr::generate (std::shared_ptr<Context> ctx, std::shared_ptr<Expr> from) {
    GenPolicy::add_to_complexity(Node::NodeID::TYPE_CAST);
    std::shared_ptr<IntegerType> to_type = IntegerType::generate(ctx);
    return std::make_shared<TypeCastExpr> (from, to_type, false);
}

std::string TypeCastExpr::emit (std::string offset) {
    std::string ret = offset;
    //TODO: add parameter to gen_policy
    if (!is_implicit)
        ret += "(" + value->get_type()->get_simple_name() + ") ";
    else
        ret += "(" + value->get_type()->get_simple_name() + ") ";
    ret += "(" + expr->emit() + ")";
    return ret;
}

std::shared_ptr<ConstExpr> ConstExpr::generate (std::shared_ptr<Context> ctx) {
    GenPolicy::add_to_complexity(Node::NodeID::CONST);
    std::shared_ptr<IntegerType> int_type = IntegerType::generate (ctx);
    return std::make_shared<ConstExpr>(FundType::ScalarTypedVal::generate(ctx, int_type->get_int_type_id()));
}

std::string ConstExpr::emit (std::string offset) {
    std::string ret = offset;
    std::shared_ptr<ScalarVariable> scalar_val = std::static_pointer_cast<ScalarVariable>(value);
    auto val = scalar_val->get_cur_value().val;
    switch (scalar_val->get_type()->get_int_type_id()) {
        case IntegerType::IntegerTypeID::BOOL:
            ret += std::to_string(val.bool_val);
            break;
        case IntegerType::IntegerTypeID::CHAR:
            ret += std::to_string(val.char_val);
            break;
        case IntegerType::IntegerTypeID::UCHAR:
            ret += std::to_string(val.uchar_val);
            break;
        case IntegerType::IntegerTypeID::SHRT:
            ret += std::to_string(val.shrt_val);
            break;
        case IntegerType::IntegerTypeID::USHRT:
            ret += std::to_string(val.ushrt_val);
            break;
        case IntegerType::IntegerTypeID::INT:
            ret += std::to_string(val.int_val);
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret += std::to_string(val.uint_val);
            break;
        case IntegerType::IntegerTypeID::LINT:
            if (mode_64bit)
                ret += std::to_string(val.lint64_val);
            else
                ret += std::to_string(val.lint32_val);
            break;
        case IntegerType::IntegerTypeID::ULINT:
            if (mode_64bit)
                ret += std::to_string(val.ulint64_val);
            else
                ret += std::to_string(val.ulint32_val);
            break;
        case IntegerType::IntegerTypeID::LLINT:
            ret += std::to_string(val.llint_val);
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            ret += std::to_string(val.ullint_val);
            break;
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            ERROR("bad int type id (Constexpr)");
    }
    ret += std::static_pointer_cast<FundType>(scalar_val->get_type())->get_suffix ();
    return ret;
}

ConstExpr::ConstExpr(FundType::ScalarTypedVal _val) :
        Expr(Node::NodeID::CONST, std::make_shared<ScalarVariable>("", IntegerType::init(_val.get_int_type_id()))) {
    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(_val);
}

std::shared_ptr<Expr> ArithExpr::integral_prom (std::shared_ptr<Expr> arg) {
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        ERROR("can perform integral_prom only on ScalarVariable (ArithExpr)");
    }

    if (!arg->get_value()->get_type()->get_is_bit_field()) {
        //[conv.prom]
        if (arg->get_value()->get_type()->get_int_type_id() >= IntegerType::IntegerTypeID::INT) // can't perform integral promotion
            return arg;
        return std::make_shared<TypeCastExpr>(arg, IntegerType::init(Type::IntegerTypeID::INT), true);
    }
    else {
        FundType::ScalarTypedVal val = std::static_pointer_cast<ScalarVariable>(arg->get_value())->get_cur_value();
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

    if (arg->get_value()->get_type()->get_int_type_id() == IntegerType::IntegerTypeID::BOOL) // can't perform integral promotiom
        return arg;
    return std::make_shared<TypeCastExpr>(arg, IntegerType::init(Type::IntegerTypeID::BOOL), true);
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
//    std::cout << "============" << std::endl;
    return gen_level(ctx, inp, 0);
}

// Top-level recursive function for expression tree generation.
std::shared_ptr<Expr> ArithExpr::gen_level (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp, int par_depth) {
    auto p = ctx->get_gen_policy();
    //TODO: it is a stub for testing. Rewrite it later.
    // Randomly choose pattern for single statement and apply it to gen_policy. Update Context with new gen_policy.
    GenPolicy new_gen_policy = choose_and_apply_ssp(*(p));
    std::shared_ptr<Context> new_ctx = std::make_shared<Context>(*(ctx));
    new_ctx->set_gen_policy(new_gen_policy);

    // Randomly choose ID of creating node.
    GenPolicy::ArithLeafID node_type = rand_val_gen->get_rand_id (p->get_arith_leaves());
    std::shared_ptr<Expr> ret = nullptr;

    // If we want to use any Data, we've reached expression tree depth limit or we want to use CSE but don't have any,
    // we fall into this branch.
    if (node_type == GenPolicy::ArithLeafID::Data || par_depth == p->get_max_arith_depth() ||
       (node_type == GenPolicy::ArithLeafID::CSE && p->get_cse().size() == 0)) {
        // Randomly choose ID of Data
        GenPolicy::ArithDataID data_type = rand_val_gen->get_rand_id (p->get_arith_data_distr());
        // If we want to use Const or don't have any input VarUseExpr / MemberExpr, we fall into this branch.
        if (data_type == GenPolicy::ArithDataID::Const || inp.size() == 0) {
            ret = ConstExpr::generate(new_ctx);
        }
        // Branch for input VarUseExpr / MemberExpr
        else if (data_type == GenPolicy::ArithDataID::Inp) {
            int inp_num = rand_val_gen->get_rand_value<int>(0, inp.size() - 1);
            ret = inp.at(inp_num);
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
    else if (node_type == GenPolicy::ArithLeafID::Unary) {
        ret = UnaryExpr::generate(new_ctx, inp, par_depth + 1);
    }
    // Binary expr
    else if (node_type == GenPolicy::ArithLeafID::Binary) {
        ret = BinaryExpr::generate(new_ctx, inp, par_depth + 1);
    }
    // TypeCast expr
    else if (node_type == GenPolicy::ArithLeafID::TypeCast) {
        ret = TypeCastExpr::generate(new_ctx, ArithExpr::gen_level(new_ctx, inp, par_depth + 1));
    }
    // Use existing CSE
    else if (node_type == GenPolicy::ArithLeafID::CSE) {
        int cse_num = rand_val_gen->get_rand_value<int>(0, p->get_cse().size() - 1);
        ret = p->get_cse().at(cse_num);
    }
    else {
        ERROR("unappropriate node type (ArithExpr)");
    }
//    std::cout << ret->emit() << std::endl;
    return ret;
}


std::shared_ptr<UnaryExpr> UnaryExpr::generate (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp, int par_depth) {
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

    FundType::ScalarTypedVal new_val (scalar_val->get_type()->get_int_type_id());

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
// Main idea is to replace operator to its complementary operator. This always works with unary operations.
std::string UnaryExpr::emit (std::string offset) {
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
    if (op == PostInc || op == PostDec)
        ret = "(" + arg->emit() + ")" + op_str;
    else
        ret = op_str + "(" + arg->emit() + ")";
    return ret;
}

std::shared_ptr<BinaryExpr> BinaryExpr::generate (std::shared_ptr<Context> ctx, std::vector<std::shared_ptr<Expr>> inp, int par_depth) {
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
// Main idea is to replace operator to its complementary operator. This works pretty well in most cases.
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
            //TODO: We should rewrite it later. It is awfull
            if ((ub == UB::ShiftRhsNeg) || (ub == UB::ShiftRhsLarge)) {
                std::shared_ptr<Expr> lhs = arg0;
                std::shared_ptr<Expr> rhs = arg1;
                // First of all, we need to find maximum value which can be used as rhs
                std::shared_ptr<IntegerType> lhs_int_type = std::static_pointer_cast<IntegerType>(lhs->get_value()->get_type());
                uint64_t max_sht_val = lhs_int_type->get_bit_size();
                if ((op == Shl) && (lhs_int_type->get_is_signed()) && (ub == UB::ShiftRhsLarge))
                    max_sht_val -= msb((uint64_t)std::static_pointer_cast<ScalarVariable>(lhs->get_value())->get_cur_value().get_abs_val());
                // Secondly, we randomly choose value between 0 and maximum rhs value
                uint64_t const_val = rand_val_gen->get_rand_value<int>(0, max_sht_val);
                // Thirdly, we combine chosen value with existing rhs
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
                FundType::ScalarTypedVal const_ins_val (rhs_int_type->get_int_type_id());
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
                FundType::ScalarTypedVal const_ins_val(lhs_int_type->get_int_type_id());
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

    std::shared_ptr<ScalarVariable> scalar_lhs = std::static_pointer_cast<ScalarVariable>(arg0->get_value());
    std::shared_ptr<ScalarVariable> scalar_rhs = std::static_pointer_cast<ScalarVariable>(arg1->get_value());
    FundType::ScalarTypedVal new_val (scalar_lhs->get_type()->get_int_type_id());


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
        case MaxOp:
            ERROR("bad op (BinaryExpr)");
            break;
    }

    if (!new_val.has_ub()) {
        value = std::make_shared<ScalarVariable>("", IntegerType::init(new_val.get_int_type_id()));
        std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
    }
    else {
        value = std::make_shared<ScalarVariable>("", IntegerType::init(arg0->get_value()->get_type()->get_int_type_id()));
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

std::string BinaryExpr::emit (std::string offset) {
    std::string ret = offset;
    ret += "(" + arg0->emit() + ")";
    switch (op) {
        case Add:
            ret += " + ";
            break;
        case Sub:
            ret += " - ";
            break;
        case Mul:
            ret += " * ";
            break;
        case Div:
            ret += " / ";
            break;
        case Mod:
            ret += " % ";
            break;
        case Shl:
            ret += " << ";
            break;
        case Shr:
            ret += " >> ";
            break;
        case Lt:
            ret += " < ";
            break;
        case Gt:
            ret += " > ";
            break;
        case Le:
            ret += " <= ";
            break;
        case Ge:
            ret += " >= ";
            break;
        case Eq:
            ret += " == ";
            break;
        case Ne:
            ret += " != ";
            break;
        case BitAnd:
            ret += " & ";
            break;
        case BitXor:
            ret += " ^ ";
            break;
        case BitOr:
            ret += " | ";
            break;
        case LogAnd:
            ret += " && ";
            break;
        case LogOr:
            ret += " || ";
            break;
        case MaxOp:
            ERROR("bad op (BinaryExpr)");
            break;
        }
        ret += "(" + arg1->emit() + ")";
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
            if (value->get_type()->get_int_type_id() != _new_value->get_type()->get_int_type_id()) {
                ERROR("can't assign different types (MemberExpr)");
            }
            if (value->get_type()->get_is_bit_field())
                return check_and_set_bit_field(_expr);
            else {
                std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(_new_value)->get_cur_value());
                return _expr;
            }
            break;
        case Data::VarClassID::STRUCT:
            //TODO: implement for Struct
            ERROR("Struct is unsupported (MemberExpr)");
            break;
        case Data::VarClassID::MAX_CLASS_ID:
            ERROR("unsupported Data::VarClassID (MemberExpr)");
    }
}

static std::shared_ptr<Expr> change_to_value(std::shared_ptr<Context> ctx, std::shared_ptr<Expr> _expr, FundType::ScalarTypedVal to_val) {
    std::shared_ptr<Data> expr_data = _expr->get_value();
    if (expr_data->get_class_id() != Data::VarClassID::VAR) {
        ERROR("only variables are supported");
    }
    FundType::ScalarTypedVal value = std::static_pointer_cast<ScalarVariable>(expr_data)->get_cur_value();
    std::shared_ptr<ConstExpr> const_expr = std::make_shared<ConstExpr>(value);
    std::shared_ptr<Expr> to_zero =  std::make_shared<BinaryExpr>(BinaryExpr::Op::Sub, _expr, const_expr);
    std::shared_ptr<ConstExpr> to_val_const_expr = std::make_shared<ConstExpr>(to_val);
    return std::make_shared<BinaryExpr>(BinaryExpr::Op::Add, to_zero, to_val_const_expr);
}

std::shared_ptr<Expr> MemberExpr::check_and_set_bit_field (std::shared_ptr<Expr> _expr) {
    FundType::ScalarTypedVal new_val = std::static_pointer_cast<ScalarVariable>(_expr->get_value())->get_cur_value();
    std::shared_ptr<BitField> bit_field = std::static_pointer_cast<BitField>(value->get_type());
    FundType::ScalarTypedVal ovf_cmp_val = (bit_field->get_min() > new_val) || (bit_field->get_max() < new_val);
    if (!ovf_cmp_val.val.bool_val) {
        std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
        return _expr;
    }
    //TODO: it is a stub. We need to change it
    GenPolicy gen_policy;
    Context ctx_var (gen_policy, nullptr, Node::NodeID::MAX_STMT_ID, true);
    ctx_var.set_local_sym_table(std::make_shared<SymbolTable>());
    std::shared_ptr<Context> ctx = std::make_shared<Context>(ctx_var);
    FundType::ScalarTypedVal to_value = FundType::ScalarTypedVal::generate(ctx, bit_field->get_min(), bit_field->get_max());
    std::shared_ptr<Expr> ret = change_to_value(ctx, _expr, to_value);

    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(ret->get_value())->get_cur_value());
    return ret;
}

std::string MemberExpr::emit (std::string offset) {
    std::string ret = offset;
    if (struct_var == nullptr && member_expr == nullptr) {
        ERROR("bad struct_var or member_expr (MemberExpr)");
    }

    if (struct_var != nullptr) {
        if (struct_var->get_member_count() <= identifier) {
            ERROR("bad identifier (MemberExpr)");
        }
        ret += struct_var->get_name() + "." + struct_var->get_member(identifier)->get_name();
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
        ret += member_expr->emit() + "." +  member_expr_struct->get_member(identifier)->get_name();
    }
    return ret;
}

MemberExpr::MemberExpr(std::shared_ptr<Struct> _struct, uint64_t _identifier) :
        Expr(Node::NodeID::MEMBER, _struct), member_expr(nullptr), struct_var(_struct), identifier(_identifier) {
    propagate_type();
    propagate_value();
}

MemberExpr::MemberExpr(std::shared_ptr<MemberExpr> _member_expr, uint64_t _identifier) :
        Expr(Node::NodeID::MEMBER, _member_expr->get_value()), member_expr(_member_expr), struct_var(nullptr), identifier(_identifier) {
    propagate_type();
    propagate_value();
}

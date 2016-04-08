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

#include "type.h"
#include "variable.h"
#include "ir_node.h"
#include "expr.h"
#include "sym_table.h"

using namespace rl;

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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported Data::VarClassID in Expr::get_value" << std::endl;
            exit(-1);
    }
}

void VarUseExpr::set_value (std::shared_ptr<Data> _new_value) {
    if (_new_value->get_class_id() != value->get_class_id()) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": different Data::VarClassID in VarUseExpr::set_value" << std::endl;
        exit(-1);
    }
    switch (value->get_class_id()) {
        case Data::VarClassID::VAR:
            //TODO: Add integer type id check. We can't assign different types
            std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(_new_value)->get_cur_value());
            break;
        case Data::VarClassID::STRUCT:
            //TODO: implement for Struct
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": Struct is unsupported in in VarUseExpr::set_value" << std::endl;
            exit(-1);
            break;
        case Data::VarClassID::MAX_CLASS_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported Data::VarClassID in VarUseExpr::set_value" << std::endl;
            exit(-1);
    }
}

AssignExpr::AssignExpr (std::shared_ptr<Expr> _to, std::shared_ptr<Expr> _from, bool _taken) :
                        Expr(Node::NodeID::ASSIGN, _to->get_value()), to(_to), from(_from), taken(_taken) {
    if (to->get_id() != Node::NodeID::VAR_USE && to->get_id() != Node::NodeID::MEMBER) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can assign only to variable in AssignExpr::AssignExpr" << std::endl;
        exit(-1);
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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": struct are unsupported in AssignExpr::propagate_value" << std::endl;
            exit(-1);
        }
    return true;
}

UB AssignExpr::propagate_value () {
    value = from->get_value();
    if (!taken)
        return NoUB;

    if (to->get_id() == Node::NodeID::VAR_USE) {
        std::static_pointer_cast<VarUseExpr>(to)->set_value(from->get_value());
    }
    else if (to->get_id() == Node::NodeID::MEMBER) {
        std::static_pointer_cast<MemberExpr>(to)->set_value(from->get_value());
    }
    else {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can assign only to variable in AssignExpr::propagate_value" << std::endl;
        exit(-1);
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
              Expr(Node::NodeID::TYPE_CAST, NULL), expr(_expr), to_type(_type), is_implicit(_is_implicit) {
    propagate_type();
    propagate_value();
}

bool TypeCastExpr::propagate_type () {
    if (to_type->get_int_type_id() == Type::IntegerTypeID::MAX_INT_ID ||
        expr->get_value()->get_type()->get_int_type_id() == Type::IntegerTypeID::MAX_INT_ID) {
        //TODO: what about overloaded struct types cast?
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can cast only integer types in TypeCastExpr::propagate_type" << std::endl;
        exit(-1);
    }
    return true;
}

UB TypeCastExpr::propagate_value () {
    if (expr->get_value()->get_class_id() != Data::VarClassID::VAR) {
        //TODO: what about overloaded struct types cast?
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can cast only integer types in TypeCastExpr::propagate_value" << std::endl;
        exit(-1);
    }
    //TODO: Is it always safe to cast value to ScalarVariable?
    value = std::make_shared<ScalarVariable>("", std::static_pointer_cast<IntegerType>(to_type));
    std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(expr->get_value())->get_cur_value().cast_type(to_type->get_int_type_id()));
    return NoUB;
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

std::string ConstExpr::emit (std::string offset) {
    std::string ret = offset;
    std::shared_ptr<ScalarVariable> scalar_val = std::static_pointer_cast<ScalarVariable>(value);
    switch (scalar_val->get_type()->get_int_type_id()) {
        case IntegerType::IntegerTypeID::BOOL:
            ret += std::to_string(scalar_val->get_cur_value().val.bool_val);
            break;
        case IntegerType::IntegerTypeID::CHAR:
            ret += std::to_string(scalar_val->get_cur_value().val.char_val);
            break;
        case IntegerType::IntegerTypeID::UCHAR:
            ret += std::to_string(scalar_val->get_cur_value().val.uchar_val);
            break;
        case IntegerType::IntegerTypeID::SHRT:
            ret += std::to_string(scalar_val->get_cur_value().val.shrt_val);
            break;
        case IntegerType::IntegerTypeID::USHRT:
            ret += std::to_string(scalar_val->get_cur_value().val.ushrt_val);
            break;
        case IntegerType::IntegerTypeID::INT:
            ret += std::to_string(scalar_val->get_cur_value().val.int_val);
            break;
        case IntegerType::IntegerTypeID::UINT:
            ret += std::to_string(scalar_val->get_cur_value().val.uint_val);
            break;
        case IntegerType::IntegerTypeID::LINT:
            ret += std::to_string(scalar_val->get_cur_value().val.lint_val);
            break;
        case IntegerType::IntegerTypeID::ULINT:
            ret += std::to_string(scalar_val->get_cur_value().val.ulint_val);
            break;
        case IntegerType::IntegerTypeID::LLINT:
            ret += std::to_string(scalar_val->get_cur_value().val.llint_val);
            break;
        case IntegerType::IntegerTypeID::ULLINT:
            ret += std::to_string(scalar_val->get_cur_value().val.ullint_val);
            break;
        case IntegerType::IntegerTypeID::MAX_INT_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad int type id in Constexpr::emit" << std::endl;
            exit(-1);
    }
    ret += std::static_pointer_cast<AtomicType>(scalar_val->get_type())->get_suffix ();
    return ret;
}

std::shared_ptr<Expr> ArithExpr::integral_prom (std::shared_ptr<Expr> arg) {
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can perform integral_prom only on ScalarVariable in ArithExpr::integral_prom" << std::endl;
        exit(-1);
    }

    //[conv.prom]
    if (arg->get_value()->get_type()->get_int_type_id() >= IntegerType::IntegerTypeID::INT) // can't perform integral promotiom
        return arg;
    return std::make_shared<TypeCastExpr>(arg, IntegerType::init(Type::IntegerTypeID::INT), true);
}

std::shared_ptr<Expr> ArithExpr::conv_to_bool (std::shared_ptr<Expr> arg) {
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can perform conv_to_bool only on ScalarVariable in ArithExpr::conv_to_bool" << std::endl;
        exit(-1);
    }

    if (arg->get_value()->get_type()->get_int_type_id() == IntegerType::IntegerTypeID::BOOL) // can't perform integral promotiom
        return arg;
    return std::make_shared<TypeCastExpr>(arg, IntegerType::init(Type::IntegerTypeID::BOOL), true);
}

std::shared_ptr<Expr> ArithExpr::generate (Context ctx, std::vector<std::shared_ptr<Expr>> inp) {
    return gen_level(ctx, inp, 0);
}

std::shared_ptr<Expr> ArithExpr::gen_level (Context ctx, std::vector<std::shared_ptr<Expr>> inp, int par_depth) {
    //TODO: itsi a stub fortesting. Rewrite it later.
    GenPolicy::ArithLeafID node_type = rand_val_gen->get_rand_id (ctx.get_gen_policy()->get_arith_leaves());
    std::shared_ptr<Expr> ret = NULL;
    if (node_type == GenPolicy::ArithLeafID::Data || par_depth == 1) {
        int var_num = rand_val_gen->get_rand_value<int>(0, inp.size() - 1);
        ret = inp.at(var_num);
    }
/*
    else //if (node_type == GenPolicy::ArithLeafID::Unary) { // Unary expr
    {
        ret = UnaryExpr::generate(ctx, inp, par_depth + 1);
    }

    else if (node_type == GenPolicy::ArithLeafID::Binary) { // Binary expr
*/
    else {
        ret = BinaryExpr::generate(ctx, inp, par_depth + 1);
    }
/*
    else {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unappropriate node type in ArithExpr::gen_level" << std::endl;
        exit (-1);
    }
*/
    return ret;
}


std::shared_ptr<UnaryExpr> UnaryExpr::generate (Context ctx, std::vector<std::shared_ptr<Expr>> inp, int par_depth) {
    UnaryExpr::Op op_type = rand_val_gen->get_rand_id(ctx.get_gen_policy()->get_allowed_unary_op());
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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in UnaryExpr::rebuild" << std::endl;
            exit(-1);
            break;
    }
    propagate_type();
    UB ret_ub = propagate_value();
    if (ret_ub) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": illegal strategy in UnaryExpr::rebuild" << std::endl;
        exit(-1);
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
    if (op == MaxOp || arg == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad args in UnaryExpr::propagate_type" << std::endl;
        exit(-1);
        return false;
    }

    //TODO: what about overloadedstruct operators?
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can perform propagate_type only on ScalarVariable in UnaryExpr::propagate_type" << std::endl;
        exit(-1);
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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in UnaryExpr::propagate_type" << std::endl;
            exit(-1);
            break;
    }
    value = arg->get_value();
    return true;
}

UB UnaryExpr::propagate_value () {
    if (op == MaxOp || arg == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in UnaryExpr::propagate_value" << std::endl;
        exit(-1);
        return NullPtr;
    }

    //TODO: what about overloadedstruct operators?
    if (arg->get_value()->get_class_id() != Data::VarClassID::VAR) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can perform propagate_value only on ScalarVariable in UnaryExpr::propagate_value" << std::endl;
        exit(-1);
    }

    std::shared_ptr<ScalarVariable> scalar_val = std::static_pointer_cast<ScalarVariable>(arg->get_value());

    AtomicType::ScalarTypedVal new_val (scalar_val->get_type()->get_int_type_id());

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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in UnaryExpr::propagate_value" << std::endl;
            exit(-1);
            break;
    }

    if (!new_val.has_ub())
        std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
    return new_val.get_ub();
}

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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in UnaryExpr::emit" << std::endl;
            exit(-1);
            break;
    }
    std::string ret = "";
    if (op == PostInc || op == PostDec)
        ret = "(" + arg->emit() + ")" + op_str;
    else
        ret = op_str + "(" + arg->emit() + ")";
    return ret;
}

std::shared_ptr<BinaryExpr> BinaryExpr::generate (Context ctx, std::vector<std::shared_ptr<Expr>> inp, int par_depth) {
    BinaryExpr::Op op_type = rand_val_gen->get_rand_id(ctx.get_gen_policy()->get_allowed_binary_op());
    std::shared_ptr<Expr> lhs = ArithExpr::gen_level (ctx, inp, par_depth);
    std::shared_ptr<Expr> rhs = ArithExpr::gen_level (ctx, inp, par_depth);
    return std::make_shared<BinaryExpr>(op_type, lhs, rhs);
}

BinaryExpr::BinaryExpr (Op _op, std::shared_ptr<Expr> lhs, std::shared_ptr<Expr> rhs) :
                        ArithExpr(Node::NodeID::BINARY, NULL), op(_op), arg0(lhs), arg1(rhs) {
    //TODO: add UB elimination strategy
    propagate_type();
    UB ret_ub = propagate_value();
    if (ret_ub != NoUB) {
        std::cout << "Bi UB: " << ret_ub << std::endl;
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

void BinaryExpr::rebuild (UB ub) {
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
        case BinaryExpr::Shr:
        case BinaryExpr::Shl:
            //TODO: We should rewrite it later. It is awfull
            if ((ub == UB::ShiftRhsNeg) || (ub == UB::ShiftRhsLarge)) {
                std::shared_ptr<Expr> lhs = arg0;
                std::shared_ptr<Expr> rhs = arg1;
                std::shared_ptr<IntegerType> lhs_int_type = std::static_pointer_cast<IntegerType>(lhs->get_value()->get_type());
                uint64_t max_sht_val = lhs_int_type->get_bit_size();
                if ((op == Shl) && (lhs_int_type->get_is_signed()) && (ub == UB::ShiftRhsLarge))
                    max_sht_val -= msb((uint64_t)std::static_pointer_cast<ScalarVariable>(lhs->get_value())->get_cur_value().get_abs_val());
                uint64_t const_val = rand_val_gen->get_rand_value<int>(0, max_sht_val);
                uint64_t rhs_abs_val = std::static_pointer_cast<ScalarVariable>(rhs->get_value())->get_cur_value().get_abs_val();
                std::shared_ptr<IntegerType> rhs_int_type = std::static_pointer_cast<IntegerType>(rhs->get_value()->get_type());
                if (ub == UB::ShiftRhsNeg) {
                    const_val += rhs_abs_val;
                    const_val = std::min(const_val, rhs_int_type->get_max().get_abs_val());// TODO: it won't work with INT_MIN
                }
                else {
                    const_val = rhs_abs_val - const_val;
                }

                AtomicType::ScalarTypedVal const_ins_val (rhs_int_type->get_int_type_id());
                const_ins_val.set_abs_val (const_val);
                std::shared_ptr<ConstExpr> const_ins = std::make_shared<ConstExpr>(const_ins_val);
                if (ub == UB::ShiftRhsNeg)
                    arg1 = std::make_shared<BinaryExpr>(Add, arg1, const_ins);
                else
                    arg1 = std::make_shared<BinaryExpr>(Sub, arg1, const_ins);
            }
            else {
                std::shared_ptr<Expr> lhs = arg0;
                std::shared_ptr<IntegerType> lhs_int_type = std::static_pointer_cast<IntegerType>(lhs->get_value()->get_type());
                uint64_t const_val = lhs_int_type->get_max().get_abs_val();
                AtomicType::ScalarTypedVal const_ins_val(lhs_int_type->get_int_type_id());
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
            std::cerr << "ArithExprGen::rebuild_binary : invalid Op" << std::endl;
            break;
    }
    propagate_type();
    UB ret_ub = propagate_value();
    if (ret_ub != NoUB) {
        std::cout << "Bi UB in: " << ret_ub << std::endl;
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
    if (op == MaxOp || arg0 == NULL || arg1 == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad args in BinaryExpr::propagate_type" << std::endl;
        exit(-1);
    }

    //TODO: what about overloaded struct operators?
    if (arg0->get_value()->get_class_id() != Data::VarClassID::VAR ||
        arg1->get_value()->get_class_id() != Data::VarClassID::VAR) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can perform propagate_type only on ScalarVariable in BinaryExpr::propagate_type" << std::endl;
        exit(-1);
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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in BinaryExpr::propagate_type" << std::endl;
            exit(-1);
            break;
    }
    return true;
}

UB BinaryExpr::propagate_value () {
    if (op == MaxOp || arg0 == NULL || arg1 == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad args in BinaryExpr::propagate_value" << std::endl;
        exit(-1);
    }

    //TODO: what about overloaded struct operators?
    if (arg0->get_value()->get_class_id() != Data::VarClassID::VAR ||
        arg1->get_value()->get_class_id() != Data::VarClassID::VAR) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can perform propagate_value only on ScalarVariable in BinaryExpr::propagate_value" << std::endl;
        exit(-1);
    }

    std::shared_ptr<ScalarVariable> scalar_lhs = std::static_pointer_cast<ScalarVariable>(arg0->get_value());
    std::shared_ptr<ScalarVariable> scalar_rhs = std::static_pointer_cast<ScalarVariable>(arg1->get_value());
    AtomicType::ScalarTypedVal new_val (scalar_lhs->get_type()->get_int_type_id());

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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in BinaryExpr::propagate_value" << std::endl;
            exit(-1);
            break;
    }

    if (!new_val.has_ub()) {
        value = std::make_shared<ScalarVariable>("", IntegerType::init(new_val.get_int_type_id()));
        std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(new_val);
    }
    else {
        value = std::make_shared<ScalarVariable>("", IntegerType::init(arg0->get_value()->get_type()->get_int_type_id()));
    }
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
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad op in BinaryExpr::emit" << std::endl;
            exit(-1);
            break;
        }
        ret += "(" + arg1->emit() + ")";
        return ret;
}

bool MemberExpr::propagate_type () {
    if (struct_var == NULL && member_expr == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad struct_var or member_expr in MemberExpr::propagate_type" << std::endl;
        exit (-1);
    }

    if (struct_var != NULL) {
        if (struct_var->get_num_of_members() <= identifier) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad identifier in MemberExpr::propagate_type" << std::endl;
            exit (-1);
        }
        value = struct_var;
    }
    else {
        std::shared_ptr<Data> member_expr_data = member_expr->get_value();
        if (member_expr_data->get_class_id() != Data::VarClassID::STRUCT) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can take member only from Struct in MemberExpr::propagate_type" << std::endl;
            exit (-1);
        }
        std::shared_ptr<Struct> member_expr_struct = std::static_pointer_cast<Struct>(member_expr_data);
        if (member_expr_struct->get_num_of_members() <= identifier) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad identifier in MemberExpr::propagate_type" << std::endl;
            exit (-1);
        }
        value = member_expr_struct;
    }
    return true;
}

UB MemberExpr::propagate_value () {
    if (struct_var == NULL && member_expr == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad struct_var or member_expr in MemberExpr::propagate_value" << std::endl;
        exit (-1);
    }

    if (struct_var != NULL) {
        if (struct_var->get_num_of_members() <= identifier) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad identifier in MemberExpr::propagate_value" << std::endl;
            exit (-1);
        }
        value = struct_var->get_member(identifier);
    }
    else {
        std::shared_ptr<Data> member_expr_data = member_expr->get_value();
        if (member_expr_data->get_class_id() != Data::VarClassID::STRUCT) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can take member only from Struct in MemberExpr::propagate_value" << std::endl;
            exit (-1);
        }
        std::shared_ptr<Struct> member_expr_struct = std::static_pointer_cast<Struct>(member_expr_data);
        if (member_expr_struct->get_num_of_members() <= identifier) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad identifier in MemberExpr::propagate_value" << std::endl;
            exit (-1);
        }
        value = member_expr_struct->get_member(identifier);
    }
    return NoUB;
}

void MemberExpr::set_value (std::shared_ptr<Data> _new_value) {
    //TODO: what about struct?
    if (member_expr == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad member_expr in MemberExpr::set_value" << std::endl;
        exit (-1);
    }

    if (_new_value->get_class_id() != value->get_class_id()) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": different Data::VarClassID in MemberExpr::set_value" << std::endl;
        exit(-1);
    }
    switch (value->get_class_id()) {
        case Data::VarClassID::VAR:
            //TODO: Add integer type id check. We can't assign different types
            std::static_pointer_cast<ScalarVariable>(value)->set_cur_value(std::static_pointer_cast<ScalarVariable>(_new_value)->get_cur_value());
            break;
        case Data::VarClassID::STRUCT:
            //TODO: implement for Struct
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": Struct is unsupported in in MemberExpr::set_value" << std::endl;
            exit(-1);
            break;
        case Data::VarClassID::MAX_CLASS_ID:
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": unsupported Data::VarClassID in MemberExpr::set_value" << std::endl;
            exit(-1);
    }
}

std::string MemberExpr::emit (std::string offset) {
    std::string ret = offset;
    if (struct_var == NULL && member_expr == NULL) {
        std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad struct_var or member_expr in MemberExpr::emit" << std::endl;
        exit (-1);
    }

    if (struct_var != NULL) {
        if (struct_var->get_num_of_members() <= identifier) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad identifier in MemberExpr::emit" << std::endl;
            exit (-1);
        }
        ret += struct_var->get_name() + "." + struct_var->get_member(identifier)->get_name();
    }
    else {
        std::shared_ptr<Data> member_expr_data = member_expr->get_value();
        if (member_expr_data->get_class_id() != Data::VarClassID::STRUCT) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": can take member only from Struct in MemberExpr::emit" << std::endl;
            exit (-1);
        }
        std::shared_ptr<Struct> member_expr_struct = std::static_pointer_cast<Struct>(member_expr_data);
        if (member_expr_struct->get_num_of_members() <= identifier) {
            std::cerr << "ERROR at " << __FILE__ << ":" << __LINE__ << ": bad identifier in MemberExpr::emit" << std::endl;
            exit (-1);
        }
        ret += member_expr->emit() + "." +  member_expr_struct->get_member(identifier)->get_name();
    }
    return ret;
}

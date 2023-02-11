#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

using runtime::Closure;
using runtime::Context;
using runtime::ObjectHolder;

namespace {
const string ADD_METHOD = "__add__"s;
const string INIT_METHOD = "__init__"s;
}  // namespace

ObjectHolder Assignment::Execute(Closure& closure, Context& context) {
    closure[_var] = _rv->Execute(closure,context);
    return ObjectHolder::Share(*closure.at(_var).Get());    
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    :_var(std::move(var))
    ,_rv(std::move(rv)){
}

VariableValue::VariableValue(const std::string& var_name)
    :_var_name(var_name){
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
    :_dotted_ids(std::move(dotted_ids)){
}

ObjectHolder VariableValue::Execute(Closure& closure, Context& /*context*/) {
    if(_var_name.size() != 0 && _dotted_ids.size() == 0){
        if(closure.count(_var_name) == 0){
            throw std::runtime_error("Not have variable"s);
        }
        return ObjectHolder::Share(*closure.at(_var_name).Get());
    }else if(_var_name.size() == 0 && _dotted_ids.size() != 0){
        Closure new_closure;
        ObjectHolder obj;
        bool first = true;
        for(const std::string& name: _dotted_ids){
            if(first){
                obj = closure.at(name);
                first = false;
            }else{
                new_closure = obj.TryAs<runtime::ClassInstance>()->Fields();
                obj = new_closure.at(name);
            }
        }
        return obj;
    }

    throw std::runtime_error("Not have value"s);
}

void Print::SetVariable(const std::string& name){
    _name = name;
}

unique_ptr<Print> Print::Variable(const std::string& name) {
    Print pr(nullptr);
    pr.SetVariable(name);
    return make_unique<Print>(std::move(pr));
}

Print::Print(unique_ptr<Statement> argument)
    :_argument(std::move(argument.get())){
}

Print::Print(vector<unique_ptr<Statement>> args)
    :_args(std::move(args)){
}

ObjectHolder Print::Execute(Closure& closure, Context& context) {
    if( _argument == nullptr && _args.size() == 0){
        // В случае, если команде Print передается пустое значение
        if(_name.size() == 0){
            context.GetOutputStream() << '\n';
            return {};
        }

        ObjectHolder obj = closure.at(_name);
        if(obj.Get()){
            obj->Print(context.GetOutputStream(), context);
        }else{
            context.GetOutputStream() << "None";
        }
        context.GetOutputStream() << '\n';
        return obj;
    }else if(_name.size() == 0 && _argument == nullptr && _args.size() != 0){
        size_t count = 0;
        for(std::unique_ptr<Statement>& st: _args){
            ObjectHolder obj = st->Execute(closure, context);
            if(obj.Get()){
                obj->Print(context.GetOutputStream(), context);
            }else{
                context.GetOutputStream() << "None";
            }
            if(count < (_args.size()-1)){
                context.GetOutputStream() << ' ';
            }
            count++;
        }
        context.GetOutputStream() << '\n';
    }
    return {};
}

MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
                       std::vector<std::unique_ptr<Statement>> args)
    :_object(std::move(object))
    ,_method(std::move(method))
    ,_args(std::move(args)){
}

ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
    std::vector<ObjectHolder> args;
    for(std::unique_ptr<Statement>& st: _args){
        args.push_back(st->Execute(closure,context));
    }
    runtime::ClassInstance* cl_inst = _object->Execute(closure,context).TryAs<runtime::ClassInstance>();
    if(cl_inst && cl_inst->HasMethod(_method, _args.size())){
        return cl_inst->Call(_method, args, context);
    }
    return {};
}

ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
    std::ostringstream output;
    ObjectHolder obj = std::move(_argument.get()->Execute(closure, context));
    if(obj){
        obj->Print(output, context);
        return ObjectHolder::Own(runtime::String (output.str()));
    }else{
        return ObjectHolder::Own(runtime::String("None"s));
    }
}

ObjectHolder Add::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = _lhs.get()->Execute(closure, context);
    ObjectHolder rhs = _rhs.get()->Execute(closure, context);

    // В первую очередь проверяется тип левого аргумента
    runtime::Number* n_lhs = lhs.TryAs<runtime::Number>();
    runtime::String* s_lhs = lhs.TryAs<runtime::String>();
    runtime::ClassInstance* class_inst_lhs = lhs.TryAs<runtime::ClassInstance>();

    if(n_lhs && !s_lhs && !class_inst_lhs){
        runtime::Number* n_rhs = rhs.TryAs<runtime::Number>();
        if(n_rhs){
            int int_result_add = n_lhs->GetValue() + n_rhs->GetValue();
            return std::move(ObjectHolder::Own(runtime::Number(int_result_add)));
        }
    }else if(!n_lhs && s_lhs && !class_inst_lhs){
        runtime::String* s_rhs = rhs.TryAs<runtime::String>();
        if(s_rhs){
            std::string str_result_add = s_lhs->GetValue() + s_rhs->GetValue();
            return std::move(ObjectHolder::Own(runtime::String(str_result_add)));
        }
    }else if(!n_lhs && !s_lhs && class_inst_lhs){
        if(class_inst_lhs && class_inst_lhs->HasMethod("__add__"s, 1) ){
            std::vector<runtime::ObjectHolder> actual_args;
            actual_args.push_back(rhs);
            return class_inst_lhs->Call("__add__"s, actual_args, context);
        }
    }

    throw std::runtime_error("Not valid add"s);
}

ObjectHolder Sub::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = _lhs.get()->Execute(closure, context);
    ObjectHolder rhs = _rhs.get()->Execute(closure, context);

    runtime::Number* n_lhs = lhs.TryAs<runtime::Number>();
    runtime::Number* n_rhs = rhs.TryAs<runtime::Number>();

    if(n_lhs && n_rhs){
        int int_result_add = n_lhs->GetValue() - n_rhs->GetValue();
        return std::move(ObjectHolder::Own(runtime::Number(int_result_add)));
    }

    throw std::runtime_error("Not valid sub"s);
}

ObjectHolder Mult::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = _lhs.get()->Execute(closure, context);
    ObjectHolder rhs = _rhs.get()->Execute(closure, context);

    runtime::Number* n_lhs = lhs.TryAs<runtime::Number>();
    runtime::Number* n_rhs = rhs.TryAs<runtime::Number>();

    if(n_lhs && n_rhs){
        int int_result_add = n_lhs->GetValue() * n_rhs->GetValue();
        return std::move(ObjectHolder::Own(runtime::Number(int_result_add)));
    }

    throw std::runtime_error("Not valid mult"s);
}

ObjectHolder Div::Execute(Closure& closure, Context& context) {
    ObjectHolder lhs = _lhs.get()->Execute(closure, context);
    ObjectHolder rhs = _rhs.get()->Execute(closure, context);

    runtime::Number* n_lhs = lhs.TryAs<runtime::Number>();
    runtime::Number* n_rhs = rhs.TryAs<runtime::Number>();

    if(n_lhs && n_rhs){
        if(n_rhs->GetValue() != 0){
            int int_result_add = n_lhs->GetValue() / n_rhs->GetValue();
            return std::move(ObjectHolder::Own(runtime::Number(int_result_add)));
        }
    }

    throw std::runtime_error("Not valid div"s);
}

ObjectHolder Compound::Execute(Closure& closure, Context& context) {
    for(std::unique_ptr<Statement>& st_ptr :_stmts){
        st_ptr.get()->Execute(closure, context);
    }

    return ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure, Context& context) {
    ObjectHolder obj = _statement.get()->Execute(closure, context);
    throw std::move(obj);
}

ClassDefinition::ClassDefinition(ObjectHolder cls)
    :_cls(std::move(cls)){
}

ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {
    closure[_cls.TryAs<runtime::Class>()->GetName()] = std::move(_cls);
    return {};
}

FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
                                 std::unique_ptr<Statement> rv)
    :_object(std::move(object))
    ,_field_name(std::move(field_name))
    ,_rv(rv.get()){
    rv.release();
}

ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {
    ObjectHolder obj = _object.Execute(closure,context);
    obj.TryAs<runtime::ClassInstance>()->Fields()[_field_name] = _rv.get()->Execute(closure,context);
    return  _rv.get()->Execute(closure,context);
}

IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body,
               std::unique_ptr<Statement> else_body)
    :_condition(std::move(condition))
    ,_if_body(std::move(if_body))
    ,_else_body(std::move(else_body)){
}

ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
    ObjectHolder obj_condition = _condition->Execute(closure,context);
    bool obj_condition_bool = obj_condition.TryAs<runtime::Bool>()->GetValue();

    if(obj_condition_bool){
        return _if_body->Execute(closure, context);
    }else{
        if(_else_body.get()){
            return _else_body->Execute(closure, context);
        }
    }

    return {};
}

ObjectHolder Or::Execute(Closure& closure, Context& context) {
    ObjectHolder l_obj = _lhs.get()->Execute(closure,context);

    runtime::Number* n_lhs = l_obj.TryAs<runtime::Number>();
    if(n_lhs ){
        bool bool_result = n_lhs->GetValue();
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::String* s_lhs = l_obj.TryAs<runtime::String>();

    if(s_lhs ){
        bool bool_result = s_lhs->GetValue().size() != 0 ;
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::Bool* b_lhs = l_obj.TryAs<runtime::Bool>();

    if(b_lhs ){
        bool bool_result = b_lhs->GetValue();
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    ObjectHolder r_obj = _rhs.get()->Execute(closure,context);

    runtime::Number* n_rhs = r_obj.TryAs<runtime::Number>();
    if(n_rhs ){
        bool bool_result = n_rhs->GetValue();
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::String* s_rhs = r_obj.TryAs<runtime::String>();

    if(s_rhs ){
        bool bool_result = s_rhs->GetValue().size() != 0 ;
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::Bool* b_rhs = r_obj.TryAs<runtime::Bool>();

    if(b_rhs ){
        bool bool_result = b_rhs->GetValue();
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    return ObjectHolder::Own(runtime::Bool(false));
}

ObjectHolder And::Execute(Closure& closure, Context& context) {
    ObjectHolder l_obj = _lhs.get()->Execute(closure,context);

    bool first_value_true = false;

    runtime::Number* n_lhs = l_obj.TryAs<runtime::Number>();
    if(n_lhs ){
        bool bool_result = n_lhs->GetValue();
        if(bool_result){
            first_value_true = true;
        }
    }

    runtime::String* s_lhs = l_obj.TryAs<runtime::String>();
    if(s_lhs ){
        bool bool_result = s_lhs->GetValue().size() != 0 ;
        if(bool_result){
            first_value_true = true;
        }
    }

    runtime::Bool* b_lhs = l_obj.TryAs<runtime::Bool>();
    if(b_lhs ){
        bool bool_result = b_lhs->GetValue();
        if(bool_result){
            first_value_true = true;
        }
    }

    if(!first_value_true){
        return ObjectHolder::Own(runtime::Bool(false));
    }

    ObjectHolder r_obj = _rhs.get()->Execute(closure,context);

    runtime::Number* n_rhs = r_obj.TryAs<runtime::Number>();
    if(n_rhs ){
        bool bool_result = n_rhs->GetValue();
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::String* s_rhs = r_obj.TryAs<runtime::String>();
    if(s_rhs ){
        bool bool_result = s_rhs->GetValue().size() != 0 ;
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::Bool* b_rhs = r_obj.TryAs<runtime::Bool>();
    if(b_rhs ){
        bool bool_result = b_rhs->GetValue();
        if(bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    return ObjectHolder::Own(runtime::Bool(false));
}

ObjectHolder Not::Execute(Closure& closure, Context& context) {
    ObjectHolder obj = _argument.get()->Execute(closure,context);

    runtime::Number* n_rhs = obj.TryAs<runtime::Number>();
    if(n_rhs ){
        bool bool_result = n_rhs->GetValue();
        if(!bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::String* s_rhs = obj.TryAs<runtime::String>();
    if(s_rhs ){
        bool bool_result = s_rhs->GetValue().size() != 0 ;
        if(!bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    runtime::Bool* b_rhs = obj.TryAs<runtime::Bool>();
    if(b_rhs ){
        bool bool_result = b_rhs->GetValue();
        if(!bool_result){
            return std::move(ObjectHolder::Own(runtime::Bool(true)));
        }
    }

    return ObjectHolder::Own(runtime::Bool(false));
}

Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : BinaryOperation(std::move(lhs), std::move(rhs))
    ,_cmp(std::move(cmp)){
}

ObjectHolder Comparison::Execute(Closure& closure, Context& context) {
    ObjectHolder obj_lhs = _lhs->Execute(closure, context);
    ObjectHolder obj_rhs = _rhs->Execute(closure, context);
    bool result = _cmp(obj_lhs,obj_rhs,context);

    return ObjectHolder::Own(runtime::Bool(result));
}

NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
    :_class_inst(class_)
    ,_args(std::move(args)){
}

NewInstance::NewInstance(const runtime::Class& class_)
    :_class_inst(class_){
}

ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {
    if(_class_inst.HasMethod("__init__"s, _args.size())){
        std::vector<ObjectHolder> args;
        for(std::unique_ptr<Statement>& st: _args){
            args.push_back(st->Execute(closure,context));
        }
        _class_inst.Call("__init__"s, args, context);
    }

    return std::move(ObjectHolder::Share(_class_inst));
}

MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
    :_body(std::move(body)){
}

ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
    try {
        ObjectHolder result = _body.get()->Execute(closure,context);
        return ObjectHolder::None();
    } catch (ObjectHolder& ret_obj) {
        return ret_obj;
    }
}

}  // namespace ast

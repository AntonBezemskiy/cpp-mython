#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
    : data_(std::move(data)) {
}

void ObjectHolder::AssertIsValid() const {
    assert(data_ != nullptr);
}

ObjectHolder ObjectHolder::Share(Object& object) {
    // Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
    return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
}

ObjectHolder ObjectHolder::None() {
    return ObjectHolder();
}

Object& ObjectHolder::operator*() const {
    AssertIsValid();
    return *Get();
}

Object* ObjectHolder::operator->() const {
    AssertIsValid();
    return Get();
}

Object* ObjectHolder::Get() const {
    return data_.get();
}

ObjectHolder::operator bool() const {
    return Get() != nullptr;
}

bool IsTrue(const ObjectHolder& object) {
    // В случае, если оба значения None
    if(!object){
        return false;
    }

    // В случае, если оба значения String
    const String* str = object.TryAs<String>();

    if(str){
        return str->GetValue().size() != 0;
    }

    // В случае, если оба значения Number
    const Number* num = object.TryAs<Number>();

    if(num){
        return num->GetValue();
    }

    // В случае, если оба значения Bool
    const Bool* _bool = object.TryAs<Bool>();

    if(_bool){
        return _bool->GetValue();
    }

    return false;
}

void ClassInstance::Print(std::ostream& os, Context& context) {    
    if(this->HasMethod("__str__"s, 0)){
        const std::vector<ObjectHolder> vect = {};
        this->Call("__str__"s, vect, context)->Print(os, context);
    }else{
        os << this;
    }   
}

bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
    const Method* _method = this->_cls.GetMethod(method);
    if(_method != nullptr && _method->formal_params.size() == argument_count){
        return true;
    }

    return false;
}

Closure& ClassInstance::Fields() {
    return _field;   
}

const Closure& ClassInstance::Fields() const {
    return _field;   
}

ClassInstance::ClassInstance(const Class& cls)
    :_cls(cls){
    _closure["self"s] = ObjectHolder::Share(*this);   
}

ObjectHolder ClassInstance::Call(const std::string& method,
                                 const std::vector<ObjectHolder>& actual_args,
                                 Context& context) {
    if(!HasMethod(method, actual_args.size())){
        throw std::runtime_error("Not have menthod"s);
    }

    const Method* method_ = this->_cls.GetMethod(method);
    if(method_){
        Closure _closure_new = {{"self"s, ObjectHolder::Share(*this)}};
        int counter = 0;
        for(const std::string& name_formal_params: method_->formal_params){
            _closure_new[name_formal_params] = actual_args[counter];
            counter++;
        }
        return method_->body->Execute(_closure_new, context);
    }

    throw std::runtime_error("Not have method"s);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : _parent_class(parent){
    _name_class = std::move(name) ;
    _methods = std::move(methods);

}

const Method* Class::GetMethod(const std::string& name) const {
    for(const Method& method: _methods){
        if(method.name == name){
            return &method;
        }
    }

    //Если есть родительский класс, ищу метод в нем
    if(_parent_class){
        return _parent_class->GetMethod(name);
    }
    // Такого метода нет, возвращаю nullptr
    return nullptr;
}

[[nodiscard]] const std::string& Class::GetName() const {
    assert(this->_name_class.size() != 0);
    return this->_name_class;
}

void Class::Print(ostream& os, Context& /*context*/) {
    assert(_name_class.size() != 0);
    os << "Class "
       << _name_class ;
}

void Bool::Print(std::ostream& os, [[maybe_unused]] Context& context) {
    os << (GetValue() ? "True"sv : "False"sv);
}

bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    // В случае, если оба значения None
    if(!lhs && !rhs){
        return true;
    }

    // В случае, если оба значения String
    const String* lhs_str = lhs.TryAs<String>();
    const String* rhs_str = rhs.TryAs<String>();

    if(lhs_str && rhs_str){
        return lhs_str->GetValue() == rhs_str->GetValue();
    }

    // В случае, если оба значения Number
    const Number* lhs_num = lhs.TryAs<Number>();
    const Number* rhs_num = rhs.TryAs<Number>();

    if(lhs_num && rhs_num){
        return lhs_num->GetValue() == rhs_num->GetValue();
    }

    // В случае, если оба значения Bool
    const Bool* lhs_bool = lhs.TryAs<Bool>();
    const Bool* rhs_bool = rhs.TryAs<Bool>();

    if(lhs_bool && rhs_bool){
        return lhs_bool->GetValue() == rhs_bool->GetValue();
    }

    // В случае, если левое значение Class
    ClassInstance* lhs_class = lhs.TryAs<ClassInstance>();
    if(lhs_class){
        if(lhs_class->HasMethod("__eq__"s, 1)){
            const std::vector<ObjectHolder> vect = {rhs};
            const ObjectHolder result = lhs_class->Call("__eq__"s, vect, context);
            return IsTrue(result);
        }
    }

    throw std::runtime_error("Cannot compare objects for equality"s);
}

bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    // В случае, если оба значения String
    const String* lhs_str = lhs.TryAs<String>();
    const String* rhs_str = rhs.TryAs<String>();

    if(lhs_str && rhs_str){
        return lhs_str->GetValue() < rhs_str->GetValue();
    }

    // В случае, если оба значения Number
    const Number* lhs_num = lhs.TryAs<Number>();
    const Number* rhs_num = rhs.TryAs<Number>();

    if(lhs_num && rhs_num){
        return lhs_num->GetValue() < rhs_num->GetValue();
    }

    // В случае, если оба значения Bool
    const Bool* lhs_bool = lhs.TryAs<Bool>();
    const Bool* rhs_bool = rhs.TryAs<Bool>();

    if(lhs_bool && rhs_bool){
        return lhs_bool->GetValue() < rhs_bool->GetValue();
    }

    // В случае, если левое значение Class
    ClassInstance* lhs_class = lhs.TryAs<ClassInstance>();
    if(lhs_class){
        if(lhs_class->HasMethod("__lt__"s, 1)){
            const std::vector<ObjectHolder> vect = {rhs};
            const ObjectHolder result = lhs_class->Call("__lt__"s, vect, context);
            return IsTrue(result);
        }
    }

    throw std::runtime_error("Cannot compare objects for less"s);
}

bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Equal(lhs, rhs, context);
}

bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context) && !Equal(lhs, rhs, context);
}

bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return Less(lhs, rhs, context) || Equal(lhs, rhs, context);
}

bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
    return !Less(lhs, rhs, context);
}

}  // namespace runtime

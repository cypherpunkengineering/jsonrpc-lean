// This file is derived from xsonrpc Copyright (C) 2015 Erik Johansson <erik@ejohansson.se>
// This file is part of jsonrpc-lean, a c++11 JSON-RPC client/server library.
//
// Modifications and additions Copyright (C) 2015 Adriano Maia <tony@stark.im>
//
// This library is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as published by the
// Free Software Foundation; either version 2.1 of the License, or (at your
// option) any later version.
//
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
// for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this library; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

#ifndef JSONRPC_LEAN_VALUE_H
#define JSONRPC_LEAN_VALUE_H

#include <cstdint>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <ostream>

#include "util.h"
#include "fault.h"
#include "writer.h"

struct tm;

#if 1 // CYPHERPUNK_IMPLEMENTATION

#include <cctype>

namespace detail {
    template<typename T, typename T1, typename ...Tn> struct same_or_convertible { static constexpr const bool value = std::is_same<T, T1>::value || std::is_convertible<T, T1>::value || same_or_convertible<T, Tn...>::value; };
    template<typename T, typename T1> struct same_or_convertible<T, T1> { static constexpr const bool value = std::is_same<T, T1>:: value || std::is_convertible<T, T1>::value; };
}
template<typename T, typename ...Tn> using is_passable = detail::same_or_convertible<T, Tn...>;

template<typename T> static inline auto forward_begin(std::remove_reference_t<T>&  iterable) { return                         adl_begin(iterable) ; }
template<typename T> static inline auto forward_end  (std::remove_reference_t<T>&  iterable) { return                         adl_end  (iterable) ; }
template<typename T> static inline auto forward_begin(std::remove_reference_t<T>&& iterable) { return std::make_move_iterator(adl_begin(iterable)); }
template<typename T> static inline auto forward_end  (std::remove_reference_t<T>&& iterable) { return std::make_move_iterator(adl_end  (iterable)); }

namespace jsonrpc {

    class Value
    {
    public:
        typedef struct UndefinedType {} Undefined;
        typedef std::nullptr_t Null;
        typedef bool Boolean;
        typedef double Double;
        typedef int Int32;
        typedef std::string String;
        typedef std::unordered_map<String, Value> Object;
        typedef std::vector<Value> Array;

        static constexpr Undefined undefined = Undefined();
        static constexpr Double NaN = std::numeric_limits<Double>::quiet_NaN();

        enum Type : unsigned char
        {
            TYPE_UNDEFINED = 0x00,
            TYPE_NULL = 0x01,
            TYPE_BOOLEAN = 0x02,
            TYPE_NUMBER = 0x04,
            TYPE_DOUBLE = TYPE_NUMBER | 0x00,
            TYPE_INT32 = TYPE_NUMBER | 0x01,
            TYPE_STRING = 0x08,
            TYPE_OBJECT = 0x10,
            TYPE_ARRAY = TYPE_OBJECT | 0x01,

            // This type stored in this Value or its internal representation
            // must not change (e.g. this is actually a subclass with type
            // guarantees). Reference returned by AsType<T>() are stable.
            TYPE_FROZEN = 0x80,

            TYPE_UNDEFINED_FROZEN = TYPE_UNDEFINED | TYPE_FROZEN,
            TYPE_NULL_FROZEN = TYPE_NULL | TYPE_FROZEN,
            TYPE_BOOLEAN_FROZEN = TYPE_BOOLEAN | TYPE_FROZEN,
            TYPE_DOUBLE_FROZEN = TYPE_DOUBLE | TYPE_FROZEN,
            TYPE_INT32_FROZEN = TYPE_INT32 | TYPE_FROZEN,
            TYPE_STRING_FROZEN = TYPE_STRING | TYPE_FROZEN,
            TYPE_OBJECT_FROZEN = TYPE_OBJECT | TYPE_FROZEN,
            TYPE_ARRAY_FROZEN = TYPE_ARRAY | TYPE_FROZEN,

            TYPE_FLAGS = (TYPE_FROZEN),
            TYPE_MASK = 0xFF ^ TYPE_FLAGS,
        };

    public:
        constexpr Value() : _type(TYPE_UNDEFINED) {}
        constexpr Value(const Undefined&) : _type(TYPE_UNDEFINED) {}
        constexpr Value(const Null&) : _type(TYPE_NULL) {}
        constexpr Value(bool value) : _type(TYPE_BOOLEAN), _as(value) {}
        constexpr Value(int value) : _type(TYPE_INT32), _as(value) {}
        constexpr Value(double value) : _type(TYPE_DOUBLE), _as(value) {}
        Value(const char* value) : _type(TYPE_STRING) { _as.stringPointer = new String(value); }
        Value(String value) : _type(TYPE_STRING) { _as.stringPointer = new String(std::move(value)); }
        Value(Object value) : _type(TYPE_OBJECT) { _as.objectPointer = new Object(std::move(value)); }
        Value(Array  value) : _type(TYPE_ARRAY) { _as.arrayPointer = new Array(std::move(value)); }
        // Construct with iterable (use ... to lower priority and let String/Object/Array match first)
        template<typename T, typename = std::enable_if_t<!is_passable<T, String, Object, Array>::value>, typename X = decltype(std::declval<T>().begin(), std::declval<T>().end(), true)> Value(T&& iterable, ...) : Value() { Construct(std::forward<T>(iterable)); }
        // Construct with iterator pair
        template<typename T, typename U> Value(T&& first, U&& last) : Value() { Construct(std::forward<T>(first), std::forward<U>(last)); }

        explicit Value(const Value& copy) : Value() { Assign(copy); }
        Value(Value&& move) noexcept : Value() { Assign(std::move(move)); }

		~Value() { Unfreeze(); Reset(); }

    private:
        Type SetType(Type type)
        {
            if (!CanChangeType(type))
                throw std::invalid_argument("Attempted to change type of a typed/frozen Value");
            Type old = (Type)(_type &~ TYPE_FLAGS);
            _type = (Type)((_type & TYPE_FLAGS) | (type &~ TYPE_FLAGS));
            return old;
        }

        void Construct() { SetType(TYPE_UNDEFINED); }
        void Construct(const Undefined&) { SetType(TYPE_UNDEFINED); }
        void Construct(const Null&) { SetType(TYPE_NULL); }
        Boolean& Construct(Boolean value) { SetType(TYPE_BOOLEAN); return _as.booleanValue = value; }
        Int32& Construct(Int32 value) { SetType(TYPE_INT32); return _as.int32Value = value; }
        Double& Construct(Double value) { SetType(TYPE_DOUBLE); return _as.doubleValue = value; }
        Double& Construct(int64_t value) { SetType(TYPE_DOUBLE); return _as.doubleValue = (double)value; }
        String& Construct(const char* value) { SetType(TYPE_STRING); return *(_as.stringPointer = new String(value)); }
        String& Construct(String value) { SetType(TYPE_STRING); return *(_as.stringPointer = new String(std::move(value))); }
        Object& Construct(Object value) { SetType(TYPE_OBJECT); return *(_as.objectPointer = new Object(std::move(value))); }
        Array & Construct(Array  value) { SetType(TYPE_ARRAY ); return *(_as.arrayPointer  = new Array (std::move(value))); }
        // Iterable constructor; matches any type that has member functions begin() and end()
        template<typename T, typename = std::enable_if_t<!is_passable<T, String, Object, Array>::value>, typename X = decltype(std::declval<T>().begin(), std::declval<T>().end(), true)> auto Construct(T&& iterable)
        { return Construct(forward_begin<T>(iterable), forward_end<T>(iterable)); }

        template<typename T, typename E = decltype(*std::declval<T>()), typename X = std::enable_if_t<
             std::is_assignable<char, E>::value
        >> String& Construct(T&& first, T&& last) { SetType(TYPE_STRING); return *(_as.stringPointer = new String(std::forward<T>(first), std::forward<T>(last))); }
        template<typename T, typename E = decltype(*std::declval<T>()), typename X = std::enable_if_t<
            !std::is_assignable<char, E>::value &&
             std::is_assignable<std::pair<const std::string, Value>, E>::value
        >> Object& Construct(T&& first, T&& last) { SetType(TYPE_OBJECT); return *(_as.objectPointer = new Object(std::forward<T>(first), std::forward<T>(last))); }
        template<typename T, typename E = decltype(*std::declval<T>()), typename X = std::enable_if_t<
            !std::is_assignable<char, E>::value &&
            !std::is_assignable<std::pair<const std::string, Value>, E>::value &&
             std::is_assignable<Value, E>::value
        >> Array& Construct(T&& first, T&& last) { SetType(TYPE_ARRAY); return *(_as.arrayPointer = new Array(std::forward<T>(first), std::forward<T>(last))); }

    public:
        Type GetType() const { return (Type)(_type & TYPE_MASK); }

        bool CanChangeType() const { return (_type & (TYPE_FROZEN)) == 0; }
        bool CanChangeType(Type other) const { return CanChangeType() || GetType() == other; }
        void Freeze() { _type = (Type)(_type | TYPE_FROZEN); }
        void Unfreeze() { _type = (Type)(_type & ~TYPE_FROZEN); }

        bool IsUndefined() const { return GetType() == TYPE_UNDEFINED; }
        bool IsNull() const { return GetType() == TYPE_NULL; }
        bool IsBoolean() const { return (GetType() & TYPE_BOOLEAN) != 0; }
        bool IsNumber() const { return (GetType() & TYPE_NUMBER) != 0; }
        bool IsDouble() const { return GetType() == TYPE_DOUBLE; }
        bool IsInt32() const { return GetType() == TYPE_INT32; }
        bool IsString() const { return GetType() == TYPE_STRING; }
        bool IsObject() const { return GetType() == TYPE_OBJECT; }
        bool IsArray() const { return GetType() == TYPE_ARRAY; }

        bool IsTrue() const { return GetType() == TYPE_BOOLEAN && _as.booleanValue; }
        bool IsFalse() const { return GetType() == TYPE_BOOLEAN && !_as.booleanValue; }
        bool IsTruthy() const { return operator bool(); }
        bool IsFalsy() const { return !operator bool(); }

        explicit operator bool() const
        {
            Type type = GetType();
            if (type < TYPE_BOOLEAN) return false;
            if (type & (TYPE_NUMBER | TYPE_STRING))
            {
                return (type & TYPE_STRING) ? !_as.stringPointer->empty() :
                    (type & 1) ? _as.int32Value != 0 : _as.doubleValue != 0;
            }
            return true;
        }

        static void Check(bool condition) { if (!condition) throw std::invalid_argument(""); }

              Boolean& AsBoolean()       { return Check(IsBoolean()),  _as.booleanValue; }
        const Boolean& AsBoolean() const { return Check(IsBoolean()),  _as.booleanValue; }
              Double & AsDouble ()       { return Check(IsDouble ()),  _as.doubleValue ; }
        const Double & AsDouble () const { return Check(IsDouble ()),  _as.doubleValue ; }
              Int32  & AsInt32  ()       { return Check(IsInt32  ()),  _as.int32Value  ; }
        const Int32  & AsInt32  () const { return Check(IsInt32  ()),  _as.int32Value  ; }
              String & AsString ()       { return Check(IsString ()), *_as.stringPointer ; }
        const String & AsString () const { return Check(IsString ()), *_as.stringPointer ; }
              Object & AsObject ()       { return Check(IsObject ()), *_as.objectPointer ; }
        const Object & AsObject () const { return Check(IsObject ()), *_as.objectPointer ; }
              Array  & AsArray  ()       { return Check(IsArray  ()), *_as.arrayPointer  ; }
        const Array  & AsArray  () const { return Check(IsArray  ()), *_as.arrayPointer  ; }
        template<typename T> inline       T& AsType();
        template<typename T> inline const T& AsType() const;

        Boolean ToBoolean() const
        {
            return IsTruthy();
        }
        Double ToDouble() const
        {
            switch (GetType())
            {
            case TYPE_DOUBLE: return _as.doubleValue;
            case TYPE_INT32: return (Double)_as.int32Value;
            case TYPE_BOOLEAN: return _as.booleanValue ? 1.0 : 0.0;
            case TYPE_NULL: return 0.0;
            case TYPE_STRING: return ParseDouble(*_as.stringPointer);
            case TYPE_ARRAY: return _as.arrayPointer->size() == 0 ? 0.0 : _as.arrayPointer->size() == 1 ? (*_as.arrayPointer)[0].ToDouble() : NaN;
            default: return NaN;
            }
        }
        Int32 ToInt32() const
        {
            if (IsInt32()) return _as.int32Value;
            double d = ToDouble();
            return isfinite(d) ? (Int32)std::trunc(d) : 0;
        }
        String ToString() const
        {
            switch (GetType())
            {
            case TYPE_STRING: return *_as.stringPointer;
            case TYPE_UNDEFINED: return "undefined";
            case TYPE_NULL: return "null";
            case TYPE_BOOLEAN: return _as.booleanValue ? "true" : "false";
            case TYPE_DOUBLE:
                if (std::isnan(_as.doubleValue)) return "NaN";
                else if (std::isinf(_as.doubleValue)) return _as.doubleValue < 0 ? "-Infinity" : "Infinity";
                else return std::to_string(_as.doubleValue);
            case TYPE_INT32: return std::to_string(_as.int32Value);
            case TYPE_ARRAY:
            {
                std::string result;
                bool first = true;
                for (const auto& v : *_as.arrayPointer)
                {
                    if (!first) result += ',';
                    result += v.ToString();
                    first = false;
                }
                return result;
            }
            default: throw new std::invalid_argument("Can't cast this Value to string");
            }
        }
        template<typename T> inline T ToType() const;

        void Write(Writer& writer) const
        {
            switch (GetType())
            {
            case TYPE_UNDEFINED:
            case TYPE_NULL: writer.WriteNull(); break;
            case TYPE_BOOLEAN: writer.Write(_as.booleanValue); break;
            case TYPE_DOUBLE: writer.Write(_as.doubleValue); break;
            case TYPE_INT32: writer.Write(_as.int32Value); break;
            case TYPE_STRING: writer.Write(*_as.stringPointer); break;
            case TYPE_OBJECT:
                writer.StartStruct();
                for (const auto& p : *_as.objectPointer)
                {
                    if (p.second.IsUndefined())
                        continue;
                    writer.StartStructElement(p.first);
                    p.second.Write(writer);
                    writer.EndStructElement();
                }
                writer.EndStruct();
                break;
            case TYPE_ARRAY:
                writer.StartArray();
                for (const auto& e : *_as.arrayPointer)
                {
                    if (e.IsUndefined())
                        writer.WriteNull();
                    else
                        e.Write(writer);
                }
                writer.EndArray();
                break;
            default: break;
            }
        }

        friend inline std::ostream& operator<<(std::ostream& os, const Value& value)
        {
            bool first = true;
            switch (value.GetType())
            {
            case TYPE_UNDEFINED: return os << "undefined";
            case TYPE_NULL: return os << "null";
            case TYPE_BOOLEAN: return os << (value._as.booleanValue ? "true" : "false");
            case TYPE_DOUBLE: return os << value._as.doubleValue;
            case TYPE_INT32: return os << value._as.int32Value;
            case TYPE_STRING: return os << '"' << *value._as.stringPointer << '"'; // FIXME: doesn't escape
            case TYPE_OBJECT:
                os << '{';
                for (auto& p : *value._as.objectPointer)
                {
                    if (first) first = false; else os << ", ";
                    os << p.first << ": " << p.second;
                }
                return os << '}';
            case TYPE_ARRAY:
                os << '[';
                for (auto& e : *value._as.arrayPointer)
                {
                    if (first) first = false; else os << ", ";
                    os << e;
                }
                return os << ']';
            default: return os;
            }
        }


        // Copy+move assignment operators
        Value& operator=(const Value &  value) { return Assign(          value ); }
        Value& operator=(      Value && value) { return Assign(std::move(value)); }
        // Special universal assignment operator to let us specialize without becoming ambiguous with constructors
        template<typename T> Value& operator=(T&& value) { return Assign(std::forward<T>(value)); }

    private:
        // Only worth having overloads to match actual instances of String/Object/Array, otherwise will have to create new instances anyway
        template<typename T> Value& Assign(T&& value, std::enable_if_t<std::is_base_of<String, std::decay_t<T>>::value, void*> = 0) { if (IsString()) { *_as.stringPointer = std::forward<T>(value); return *this; } else return Reset(std::forward<T>(value)); }
        template<typename T> Value& Assign(T&& value, std::enable_if_t<std::is_base_of<Object, std::decay_t<T>>::value, void*> = 0) { if (IsObject()) { *_as.objectPointer = std::forward<T>(value); return *this; } else return Reset(std::forward<T>(value)); }
        template<typename T> Value& Assign(T&& value, std::enable_if_t<std::is_base_of<Array , std::decay_t<T>>::value, void*> = 0) { if (IsArray ()) { *_as.arrayPointer  = std::forward<T>(value); return *this; } else return Reset(std::forward<T>(value)); }

        Value& Assign(const Value& copy)
        {
            if (&copy == this)
                return *this;
            Type type = GetType(), other = copy.GetType();
            if (type == other)
            {
                switch (type)
                {
                case TYPE_STRING: *_as.stringPointer = *copy._as.stringPointer; break;
                case TYPE_OBJECT: *_as.objectPointer = *copy._as.objectPointer; break;
                case TYPE_ARRAY: *_as.arrayPointer = *copy._as.arrayPointer; break;
                default: _as = copy._as; break;
                }
            }
            else if (!CanChangeType())
                throw std::invalid_argument("Attempted to change type of a frozen Value");
            else
            {
                Reset();
                switch (other)
                {
                case TYPE_STRING: _as.stringPointer = new String(*copy._as.stringPointer); break;
                case TYPE_OBJECT: _as.objectPointer = new Object(*copy._as.objectPointer); break;
                case TYPE_ARRAY: _as.arrayPointer = new Array(*copy._as.arrayPointer); break;
                default: _as = copy._as; break;
                }
                SetType(other);
            }
            return *this;
        }
        Value& Assign(Value&& move)
        {
            if (&move == this)
                return *this;
            Type type = GetType(), other = move.GetType();
            if (!CanChangeType())
            {
                // Internal representation is not allowed to change
                if (type == other)
                {
                    switch (type)
                    {
                    case TYPE_STRING: *_as.stringPointer = std::move(*move._as.stringPointer); break;
                    case TYPE_OBJECT: *_as.objectPointer = std::move(*move._as.objectPointer); break;
                    case TYPE_ARRAY: *_as.arrayPointer = std::move(*move._as.arrayPointer); break;
                    default: _as = move._as; break;
                    }
                }
                else
                    throw std::invalid_argument("Attempted to change type of a frozen Value");
            }
            else if ((other & (TYPE_STRING | TYPE_OBJECT | TYPE_ARRAY)) == 0)
            {
                // Other state is trivially copyable as-is
                Reset();
                _as = move._as;
                SetType(other);
            }
            else if (move.CanChangeType())
            {
                // Just steal state and leave the other as-is
                Reset();
                _as = move._as;
                SetType(move.SetType(TYPE_UNDEFINED));
            }
            else if (type == other)
            {
                // Can swap underlying objects and leave the other side with empty remains
                switch (type)
                {
                case TYPE_STRING: _as.stringPointer->clear(); break;
                case TYPE_OBJECT: _as.objectPointer->clear(); break;
                case TYPE_ARRAY: _as.arrayPointer->clear(); break;
                default: break;
                }
                std::swap(_as, move._as);
            }
            else
            {
                // No choice but to copy assign
                return Assign(static_cast<const Value&>(move));
            }
            return *this;
        }
        // Fallback assignment operator if nothing else matched
        //template<typename T> std::enable_if_t<!std::is_assignable<String, T>::value && !std::is_assignable<Object, T>::value && !std::is_assignable<Array, T>::value, Value&> operator=(T&& value) { return Reset(std::forward<T>(value)); }

    public:
        friend bool operator==(const Value& a, const Value& b)
        {
            if (&a == &b) return true;
            if (a.GetType() != b.GetType()) return false;
            switch (a.GetType())
            {
            case TYPE_BOOLEAN: return a._as.booleanValue == b._as.booleanValue;
            case TYPE_DOUBLE: return a._as.doubleValue == b._as.doubleValue;
            case TYPE_INT32: return a._as.int32Value == b._as.int32Value;
            case TYPE_STRING: return *a._as.stringPointer == *b._as.stringPointer;
            case TYPE_OBJECT: return *a._as.objectPointer == *b._as.objectPointer;
            case TYPE_ARRAY: return *a._as.arrayPointer == *b._as.arrayPointer;
            default: return true;
            }
        }
        friend bool operator!=(const Value& a, const Value& b) { return !(a == b); }


        static Double ParseDouble(const std::string& str) { return ParseDouble(str.c_str()); }
        static Double ParseDouble(const char* str)
        {
            if (!str) return NaN;
            if (!*str) return 0.0;
            char* end;
            double result = std::strtod(str, &end);
            if (end == str) return NaN;
            while (std::isspace(*end)) ++end;
            if (*end) return NaN;
            return result;
        }
        static Int32 ParseInt32(const std::string& str) { return ParseInt32(str.c_str()); }
        static Int32 ParseInt32(const char* str)
        {
            if (!str || !*str) return 0;
            char* end;
            long int result = std::strtol(str, &end, 0);
            if (end == str) return 0;
            while (std::isspace(*end)) ++end;
            if (*end) return 0;
            return result;
        }

    protected:
        template<typename ...Args>
        Value& Reset(Args&& ...args)
        {
            Reset();
            Construct(std::forward<Args>(args)...);
            return *this;
        }
        Value& Reset()
        {
            Type type = SetType(TYPE_UNDEFINED);
            if (type & (TYPE_STRING | TYPE_OBJECT))
            {
                if (type == TYPE_STRING) delete _as.stringPointer;
                else if (type == TYPE_OBJECT) delete _as.objectPointer;
                else if (type == TYPE_ARRAY) delete _as.arrayPointer;
            }
            return *this;
        }

    protected:
        Type _type;
        union Storage
        {
            struct {} initValue;
            Boolean booleanValue;
            Double doubleValue;
            Int32 int32Value;
            String* stringPointer;
            Object* objectPointer;
            Array* arrayPointer;

            constexpr Storage() : initValue() {}
            constexpr Storage(bool value) : booleanValue(value) {}
            constexpr Storage(int value) : int32Value(value) {}
            constexpr Storage(double value) : doubleValue(value) {}
        } _as;


    public:
        // JSONRPC-lean compatibility:
        typedef Object Struct;
        Struct& AsStruct() { return AsObject(); }
        const Struct& AsStruct() const { return AsObject(); }
        Value(std::string value, bool) : Value(value) {}
        Value(int64_t value) : Value((double)value) {}
        bool IsInteger32() const { return IsInt32(); }
        bool IsInteger64() const { return false; }
        bool IsNil() const { return IsNull(); }
        int32_t AsInteger32() const { return AsInt32(); }
        int64_t AsInteger64() const { return Check(IsInteger64()), _as.int32Value; }
    };

    template<> inline       Value::Boolean& Value::AsType<Value::Boolean>()       { return AsBoolean(); }
    template<> inline const Value::Boolean& Value::AsType<Value::Boolean>() const { return AsBoolean(); }
    template<> inline       Value::Double & Value::AsType<Value::Double >()       { return AsDouble (); }
    template<> inline const Value::Double & Value::AsType<Value::Double >() const { return AsDouble (); }
    template<> inline       Value::Int32  & Value::AsType<Value::Int32  >()       { return AsInt32  (); }
    template<> inline const Value::Int32  & Value::AsType<Value::Int32  >() const { return AsInt32  (); }
    template<> inline       Value::String & Value::AsType<Value::String >()       { return AsString (); }
    template<> inline const Value::String & Value::AsType<Value::String >() const { return AsString (); }
    template<> inline       Value::Object & Value::AsType<Value::Object >()       { return AsObject (); }
    template<> inline const Value::Object & Value::AsType<Value::Object >() const { return AsObject (); }
    template<> inline       Value::Array  & Value::AsType<Value::Array  >()       { return AsArray  (); }
    template<> inline const Value::Array  & Value::AsType<Value::Array  >() const { return AsArray  (); }

    template<> inline Value::Boolean Value::ToType<Value::Boolean>() const { return ToBoolean(); }
    template<> inline Value::Double  Value::ToType<Value::Double >() const { return ToDouble (); }
    template<> inline Value::Int32   Value::ToType<Value::Int32  >() const { return ToInt32  (); }
    template<> inline Value::String  Value::ToType<Value::String >() const { return ToString (); }

}

#else

namespace jsonrpc {

    class Value {
    public:
        typedef std::vector<Value> Array;
        typedef tm DateTime;
        typedef std::string String;
        typedef std::map<std::string, Value> Struct;

        enum class Type {
            ARRAY,
            BINARY,
            BOOLEAN,
            DATE_TIME,
            DOUBLE,
            INTEGER_32,
            INTEGER_64,
            NIL,
            STRING,
            STRUCT
        };

        Value() : myType(Type::NIL) {}

        Value(Array value) : myType(Type::ARRAY) {
            as.myArray = new Array(std::move(value));
        }

        Value(bool value) : myType(Type::BOOLEAN) { as.myBoolean = value; }

        Value(const DateTime& value) : myType(Type::DATE_TIME) {
            as.myDateTime = new DateTime(value);
            as.myDateTime->tm_isdst = -1;
        }

        Value(double value) : myType(Type::DOUBLE) { as.myDouble = value; }

        Value(int32_t value) : myType(Type::INTEGER_32) {
            as.myInteger32 = value;
            as.myInteger64 = value;
            as.myDouble = value;
        }

        Value(int64_t value) : myType(Type::INTEGER_64) {
            as.myInteger32 = static_cast<int32_t>(value);
            as.myInteger64 = value;
            as.myDouble = static_cast<double>(value);
        }

        Value(const char* value) : Value(String(value)) {}

        Value(String value, bool binary = false) : myType(binary ? Type::BINARY : Type::STRING) {
            as.myString = new String(std::move(value));
        }

        Value(Struct value) : myType(Type::STRUCT) {
            as.myStruct = new Struct(std::move(value));
        }

        ~Value() {
            Reset();
        }

        template<typename T>
        Value(std::vector<T> value) : Value(Array{}) {
            as.myArray->reserve(value.size());
            for (auto& v : value) {
                as.myArray->emplace_back(std::move(v));
            }
        }

        template<typename T>
        Value(const std::map<std::string, T>& value) : Value(Struct{}) {
            for (auto& v : value) {
                as.myStruct->emplace(v.first, v.second);
            }
        }

        template<typename T>
        Value(const std::unordered_map<std::string, T>& value) : Value(Struct{}) {
            for (auto& v : value) {
                as.myStruct->emplace(v.first, v.second);
            }
        }

        explicit Value(const Value& other) : myType(other.myType), as(other.as) {
            switch (myType) {
            case Type::BOOLEAN:
            case Type::DOUBLE:
            case Type::INTEGER_32:
            case Type::INTEGER_64:
            case Type::NIL:
                break;

            case Type::ARRAY:
                as.myArray = new Array(other.AsArray());
                break;
            case Type::DATE_TIME:
                as.myDateTime = new DateTime(other.AsDateTime());
                break;
            case Type::BINARY:
            case Type::STRING:
                as.myString = new String(other.AsString());
                break;
            case Type::STRUCT:
                as.myStruct = new Struct(other.AsStruct());
                break;
            }
        }

        Value& operator=(const Value&) = delete;

        Value(Value&& other) noexcept : myType(other.myType), as(other.as) {
            other.myType = Type::NIL;
        }

        Value& operator=(Value&& other) noexcept {
            if (this != &other) {
                Reset();

                myType = other.myType;
                as = other.as;

                other.myType = Type::NIL;
            }
            return *this;
        }

        bool IsArray() const { return myType == Type::ARRAY; }
        bool IsBinary() const { return myType == Type::BINARY; }
        bool IsBoolean() const { return myType == Type::BOOLEAN; }
        bool IsDateTime() const { return myType == Type::DATE_TIME; }
        bool IsDouble() const { return myType == Type::DOUBLE; }
        bool IsInteger32() const { return myType == Type::INTEGER_32; }
        bool IsInteger64() const { return myType == Type::INTEGER_64; }
        bool IsNil() const { return myType == Type::NIL; }
        bool IsString() const { return myType == Type::STRING; }
        bool IsStruct() const { return myType == Type::STRUCT; }

        const Array& AsArray() const {
            if (IsArray()) {
                return *as.myArray;
            }
            throw InvalidParametersFault();
        }

        const String& AsBinary() const { return AsString(); }

        const bool& AsBoolean() const {
            if (IsBoolean()) {
                return as.myBoolean;
            }
            throw InvalidParametersFault();
        }

        const DateTime& AsDateTime() const {
            if (IsDateTime()) {
                return *as.myDateTime;
            }
            throw InvalidParametersFault();
        }

        const double& AsDouble() const {
            if (IsDouble() || IsInteger32() || IsInteger64()) {
                return as.myDouble;
            }
            throw InvalidParametersFault();
        }

        const int32_t& AsInteger32() const {
            if (IsInteger32()) {
                return as.myInteger32;
            } else if (IsInteger64()
                && static_cast<int64_t>(as.myInteger32) == as.myInteger64) {
                return as.myInteger32;
            }
            throw InvalidParametersFault();
        }

        const int64_t& AsInteger64() const {
            if (IsInteger32() || IsInteger64()) {
                return as.myInteger64;
            }
            throw InvalidParametersFault();
        }

        const String& AsString() const {
            if (IsString() || IsBinary()) {
                return *as.myString;
            }
            throw InvalidParametersFault();
        }

        const Struct& AsStruct() const {
            if (IsStruct()) {
                return *as.myStruct;
            }
            throw InvalidParametersFault();
        }

        template<typename T>
        inline const T& AsType() const;

        Type GetType() const { return myType; }

        void Write(Writer& writer) const {
            switch (myType) {
            case Type::ARRAY:
                writer.StartArray();
                for (auto& element : *as.myArray) {
                    element.Write(writer);
                }
                writer.EndArray();
                break;
            case Type::BINARY:
                writer.WriteBinary(as.myString->data(), as.myString->size());
                break;
            case Type::BOOLEAN:
                writer.Write(as.myBoolean);
                break;
            case Type::DATE_TIME:
                writer.Write(*as.myDateTime);
                break;
            case Type::DOUBLE:
                writer.Write(as.myDouble);
                break;
            case Type::INTEGER_32:
                writer.Write(as.myInteger32);
                break;
            case Type::INTEGER_64:
                writer.Write(as.myInteger64);
                break;
            case Type::NIL:
                writer.WriteNull();
                break;
            case Type::STRING:
                writer.Write(*as.myString);
                break;
            case Type::STRUCT:
                writer.StartStruct();
                for (auto& element : *as.myStruct) {
                    writer.StartStructElement(element.first);
                    element.second.Write(writer);
                    writer.EndStructElement();
                }
                writer.EndStruct();
                break;
            }
        }

        inline const Value& operator[](Array::size_type i) const;
        inline const Value& operator[](const Struct::key_type& key) const;

    private:
        void Reset() {
            switch (myType) {
            case Type::ARRAY:
                delete as.myArray;
                break;
            case Type::DATE_TIME:
                delete as.myDateTime;
                break;
            case Type::BINARY:
            case Type::STRING:
                delete as.myString;
                break;
            case Type::STRUCT:
                delete as.myStruct;
                break;

            case Type::BOOLEAN:
            case Type::DOUBLE:
            case Type::INTEGER_32:
            case Type::INTEGER_64:
            case Type::NIL:
                break;
            }

            myType = Type::NIL;
        }

        Type myType;
        union {
            Array* myArray;
            bool myBoolean;
            DateTime* myDateTime;
            String* myString;
            Struct* myStruct;
            struct {
                double myDouble;
                int32_t myInteger32;
                int64_t myInteger64;
            };
        } as;
    };

    template<> inline const Value::Array& Value::AsType<typename Value::Array>() const {
        return AsArray();
    }

    template<> inline const bool& Value::AsType<bool>() const {
        return AsBoolean();
    }

    template<> inline const Value::DateTime& Value::AsType<typename Value::DateTime>() const {
        return AsDateTime();
    }

    template<> inline const double& Value::AsType<double>() const {
        return AsDouble();
    }

    template<> inline const int32_t& Value::AsType<int32_t>() const {
        return AsInteger32();
    }

    template<> inline const int64_t& Value::AsType<int64_t>() const {
        return AsInteger64();
    }

    template<> inline const Value::String& Value::AsType<typename Value::String>() const {
        return AsString();
    }

    template<> inline const Value::Struct& Value::AsType<typename Value::Struct>() const {
        return AsStruct();
    }

    template<> inline const Value& Value::AsType<Value>() const {
        return *this;
    }

    inline const Value& Value::operator[](Array::size_type i) const {
        return AsArray().at(i);
    };

    inline const Value& Value::operator[](const Struct::key_type& key) const {
        return AsStruct().at(key);
    }

    inline std::ostream& operator<<(std::ostream& os, const Value& value) {
        switch (value.GetType()) {
        case Value::Type::ARRAY: {
            os << '[';
            auto& a = value.AsArray();
            for (auto it = a.begin(); it != a.end(); ++it) {
                if (it != a.begin()) {
                    os << ", ";
                }
                os << *it;
            }
            os << ']';
            break;
        }
        case Value::Type::BINARY:
            os << util::Base64Encode(value.AsBinary());
            break;
        case Value::Type::BOOLEAN:
            os << value.AsBoolean();
            break;
        case Value::Type::DATE_TIME:
            os << util::FormatIso8601DateTime(value.AsDateTime());
            break;
        case Value::Type::DOUBLE:
            os << value.AsDouble();
            break;
        case Value::Type::INTEGER_32:
            os << value.AsInteger32();
            break;
        case Value::Type::INTEGER_64:
            os << value.AsInteger64();
            break;
        case Value::Type::NIL:
            os << "<nil>";
            break;
        case Value::Type::STRING:
            os << '"' << value.AsString() << '"';
            break;
        case Value::Type::STRUCT: {
            os << '{';
            auto& s = value.AsStruct();
            for (auto it = s.begin(); it != s.end(); ++it) {
                if (it != s.begin()) {
                    os << ", ";
                }
                os << it->first << ": " << it->second;
            }
            os << '}';
            break;
        }
        }
        return os;
    }

} // namespace jsonrpc

#endif

#endif // JSONRPC_LEAN_VALUE_H

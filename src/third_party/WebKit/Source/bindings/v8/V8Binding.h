/*
* Copyright (C) 2009 Google Inc. All rights reserved.
* Copyright (C) 2012 Ericsson AB. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef V8Binding_h
#define V8Binding_h

#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/V8BindingMacros.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "bindings/v8/V8StringResource.h"
#include "bindings/v8/V8ThrowException.h"
#include "bindings/v8/V8ValueCache.h"
#include "wtf/MathExtras.h"
#include "wtf/text/AtomicString.h"
#include <v8.h>

namespace WebCore {

    class DOMWindow;
    class Document;
    class Frame;
    class NodeFilter;
    class ExecutionContext;
    class ScriptWrappable;
    class XPathNSResolver;

    const int kMaxRecursionDepth = 22;

    // Schedule a DOM exception to be thrown, if the exception code is different
    // from zero.
    v8::Handle<v8::Value> setDOMException(int, v8::Isolate*);
    v8::Handle<v8::Value> setDOMException(int, const String&, v8::Isolate*);

    // Schedule a JavaScript error to be thrown.
    v8::Handle<v8::Value> throwError(V8ErrorType, const String&, v8::Isolate*);

    // Schedule a JavaScript error to be thrown.
    v8::Handle<v8::Value> throwError(v8::Handle<v8::Value>, v8::Isolate*);

    // A helper for throwing JavaScript TypeError.
    v8::Handle<v8::Value> throwTypeError(const String&, v8::Isolate*);

    // FIXME: Remove this once we kill its callers.
    v8::Handle<v8::Value> throwUninformativeAndGenericTypeError(v8::Isolate*);

    v8::ArrayBuffer::Allocator* v8ArrayBufferAllocator();

    v8::Handle<v8::Value> toV8Sequence(v8::Handle<v8::Value>, uint32_t& length, v8::Isolate*);

    inline v8::Handle<v8::Value> argumentOrNull(const v8::FunctionCallbackInfo<v8::Value>& info, int index)
    {
        return index >= info.Length() ? v8::Local<v8::Value>() : info[index];
    }

    template<typename CallbackInfo, typename V>
    inline void v8SetReturnValue(const CallbackInfo& info, V v)
    {
        info.GetReturnValue().Set(v);
    }

    template<typename CallbackInfo>
    inline void v8SetReturnValueBool(const CallbackInfo& info, bool v)
    {
        info.GetReturnValue().Set(v);
    }

    template<typename CallbackInfo>
    inline void v8SetReturnValueInt(const CallbackInfo& info, int v)
    {
        info.GetReturnValue().Set(v);
    }

    template<typename CallbackInfo>
    inline void v8SetReturnValueUnsigned(const CallbackInfo& info, unsigned v)
    {
        info.GetReturnValue().Set(v);
    }

    template<typename CallbackInfo>
    inline void v8SetReturnValueNull(const CallbackInfo& info)
    {
        info.GetReturnValue().SetNull();
    }

    template<typename CallbackInfo>
    inline void v8SetReturnValueUndefined(const CallbackInfo& info)
    {
        info.GetReturnValue().SetUndefined();
    }

    template<typename CallbackInfo>
    inline void v8SetReturnValueEmptyString(const CallbackInfo& info)
    {
        info.GetReturnValue().SetEmptyString();
    }

    template <class CallbackInfo>
    inline void v8SetReturnValueString(const CallbackInfo& info, const String& string, v8::Isolate* isolate)
    {
        if (string.isNull()) {
            v8SetReturnValueEmptyString(info);
            return;
        }
        V8PerIsolateData::from(isolate)->stringCache()->setReturnValueFromString(info.GetReturnValue(), string.impl());
    }

    template <class CallbackInfo>
    inline void v8SetReturnValueStringOrNull(const CallbackInfo& info, const String& string, v8::Isolate* isolate)
    {
        if (string.isNull()) {
            v8SetReturnValueNull(info);
            return;
        }
        V8PerIsolateData::from(isolate)->stringCache()->setReturnValueFromString(info.GetReturnValue(), string.impl());
    }

    template <class CallbackInfo>
    inline void v8SetReturnValueStringOrUndefined(const CallbackInfo& info, const String& string, v8::Isolate* isolate)
    {
        if (string.isNull()) {
            v8SetReturnValueUndefined(info);
            return;
        }
        V8PerIsolateData::from(isolate)->stringCache()->setReturnValueFromString(info.GetReturnValue(), string.impl());
    }

    // Convert v8::String to a WTF::String. If the V8 string is not already
    // an external string then it is transformed into an external string at this
    // point to avoid repeated conversions.
    inline String toCoreString(v8::Handle<v8::String> value)
    {
        return v8StringToWebCoreString<String>(value, Externalize);
    }

    inline String toCoreStringWithNullCheck(v8::Handle<v8::String> value)
    {
        if (value.IsEmpty() || value->IsNull())
            return String();
        return toCoreString(value);
    }

    inline String toCoreStringWithUndefinedOrNullCheck(v8::Handle<v8::String> value)
    {
        if (value.IsEmpty() || value->IsNull() || value->IsUndefined())
            return String();
        return toCoreString(value);
    }

    inline AtomicString toCoreAtomicString(v8::Handle<v8::String> value)
    {
        return v8StringToWebCoreString<AtomicString>(value, Externalize);
    }

    // This method will return a null String if the v8::Value does not contain a v8::String.
    // It will not call ToString() on the v8::Value. If you want ToString() to be called,
    // please use the V8TRYCATCH_FOR_V8STRINGRESOURCE_*() macros instead.
    inline String toCoreStringWithUndefinedOrNullCheck(v8::Handle<v8::Value> value)
    {
        if (value.IsEmpty() || !value->IsString())
            return String();

        return toCoreString(value.As<v8::String>());
    }

    // Convert a string to a V8 string.
    // Return a V8 external string that shares the underlying buffer with the given
    // WebCore string. The reference counting mechanism is used to keep the
    // underlying buffer alive while the string is still live in the V8 engine.
    inline v8::Handle<v8::String> v8String(v8::Isolate* isolate, const String& string)
    {
        if (string.isNull())
            return v8::String::Empty(isolate);
        return V8PerIsolateData::from(isolate)->stringCache()->v8ExternalString(string.impl(), isolate);
    }

    inline v8::Handle<v8::Value> v8Undefined()
    {
        return v8::Handle<v8::Value>();
    }

    template <class T>
    struct V8ValueTraits {
        static inline v8::Handle<v8::Value> arrayV8Value(const T& value, v8::Isolate* isolate)
        {
            return toV8(WTF::getPtr(value), v8::Handle<v8::Object>(), isolate);
        }
    };

    template<>
    struct V8ValueTraits<String> {
        static inline v8::Handle<v8::Value> arrayV8Value(const String& value, v8::Isolate* isolate)
        {
            return v8String(isolate, value);
        }
    };

    template<>
    struct V8ValueTraits<unsigned> {
        static inline v8::Handle<v8::Value> arrayV8Value(const unsigned& value, v8::Isolate* isolate)
        {
            return v8::Integer::NewFromUnsigned(value, isolate);
        }
    };

    template<>
    struct V8ValueTraits<unsigned long> {
        static inline v8::Handle<v8::Value> arrayV8Value(const unsigned long& value, v8::Isolate* isolate)
        {
            return v8::Integer::NewFromUnsigned(value, isolate);
        }
    };

    template<>
    struct V8ValueTraits<float> {
        static inline v8::Handle<v8::Value> arrayV8Value(const float& value, v8::Isolate* isolate)
        {
            return v8::Number::New(isolate, value);
        }
    };

    template<>
    struct V8ValueTraits<double> {
        static inline v8::Handle<v8::Value> arrayV8Value(const double& value, v8::Isolate* isolate)
        {
            return v8::Number::New(isolate, value);
        }
    };

    template<typename T, size_t inlineCapacity>
    v8::Handle<v8::Value> v8Array(const Vector<T, inlineCapacity>& iterator, v8::Isolate* isolate)
    {
        v8::Local<v8::Array> result = v8::Array::New(isolate, iterator.size());
        int index = 0;
        typename Vector<T, inlineCapacity>::const_iterator end = iterator.end();
        typedef V8ValueTraits<T> TraitsType;
        for (typename Vector<T, inlineCapacity>::const_iterator iter = iterator.begin(); iter != end; ++iter)
            result->Set(v8::Integer::New(index++, isolate), TraitsType::arrayV8Value(*iter, isolate));
        return result;
    }

    // Conversion flags, used in toIntXX/toUIntXX.
    enum IntegerConversionConfiguration {
        NormalConversion,
        EnforceRange,
        Clamp
    };

    // Convert a value to a 8-bit signed integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-byte
    int8_t toInt8(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);
    inline int8_t toInt8(v8::Handle<v8::Value> value, bool& ok) { return toInt8(value, NormalConversion, ok); }

    // Convert a value to a 8-bit integer assuming the conversion cannot fail.
    inline int8_t toInt8(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toInt8(value, NormalConversion, ok);
    }

    // Convert a value to a 8-bit unsigned integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-octet
    uint8_t toUInt8(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);
    inline uint8_t toUInt8(v8::Handle<v8::Value> value, bool& ok) { return toUInt8(value, NormalConversion, ok); }

    // Convert a value to a 8-bit unsigned integer assuming the conversion cannot fail.
    inline uint8_t toUInt8(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toUInt8(value, NormalConversion, ok);
    }

    // Convert a value to a 16-bit signed integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-short
    int16_t toInt16(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);
    inline int16_t toInt16(v8::Handle<v8::Value> value, bool& ok) { return toInt16(value, NormalConversion, ok); }

    // Convert a value to a 16-bit integer assuming the conversion cannot fail.
    inline int16_t toInt16(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toInt16(value, NormalConversion, ok);
    }

    // Convert a value to a 16-bit unsigned integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-unsigned-short
    uint16_t toUInt16(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);
    inline uint16_t toUInt16(v8::Handle<v8::Value> value, bool& ok) { return toUInt16(value, NormalConversion, ok); }

    // Convert a value to a 16-bit unsigned integer assuming the conversion cannot fail.
    inline uint16_t toUInt16(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toUInt16(value, NormalConversion, ok);
    }

    // Convert a value to a 32-bit signed integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-long
    int32_t toInt32(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);
    inline int32_t toInt32(v8::Handle<v8::Value> value, bool& ok) { return toInt32(value, NormalConversion, ok); }

    // Convert a value to a 32-bit integer assuming the conversion cannot fail.
    inline int32_t toInt32(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toInt32(value, NormalConversion, ok);
    }

    // Convert a value to a 32-bit unsigned integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-unsigned-long
    uint32_t toUInt32(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);
    inline uint32_t toUInt32(v8::Handle<v8::Value> value, bool& ok) { return toUInt32(value, NormalConversion, ok); }

    // Convert a value to a 32-bit unsigned integer assuming the conversion cannot fail.
    inline uint32_t toUInt32(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toUInt32(value, NormalConversion, ok);
    }

    // Convert a value to a 64-bit signed integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-long-long
    int64_t toInt64(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);

    // Convert a value to a 64-bit integer assuming the conversion cannot fail.
    inline int64_t toInt64(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toInt64(value, NormalConversion, ok);
    }

    // Convert a value to a 64-bit unsigned integer. The conversion fails if the
    // value cannot be converted to a number or the range violated per WebIDL:
    // http://www.w3.org/TR/WebIDL/#es-unsigned-long-long
    uint64_t toUInt64(v8::Handle<v8::Value>, IntegerConversionConfiguration, bool& ok);

    // Convert a value to a 64-bit unsigned integer assuming the conversion cannot fail.
    inline uint64_t toUInt64(v8::Handle<v8::Value> value)
    {
        bool ok;
        return toUInt64(value, NormalConversion, ok);
    }

    inline float toFloat(v8::Local<v8::Value> value)
    {
        return static_cast<float>(value->NumberValue());
    }

    WrapperWorldType worldType(v8::Isolate*);
    WrapperWorldType worldTypeInMainThread(v8::Isolate*);

    DOMWrapperWorld* isolatedWorldForIsolate(v8::Isolate*);

    template<class T> struct NativeValueTraits;

    template<>
    struct NativeValueTraits<String> {
        static inline String nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate)
        {
            V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, stringValue, value, String());
            return stringValue;
        }
    };

    template<>
    struct NativeValueTraits<unsigned> {
        static inline unsigned nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate)
        {
            return toUInt32(value);
        }
    };

    template<>
    struct NativeValueTraits<float> {
        static inline float nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate)
        {
            return static_cast<float>(value->NumberValue());
        }
    };

    template<>
    struct NativeValueTraits<double> {
        static inline double nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate)
        {
            return static_cast<double>(value->NumberValue());
        }
    };

    template<>
    struct NativeValueTraits<v8::Handle<v8::Value> > {
        static inline v8::Handle<v8::Value> nativeValue(const v8::Handle<v8::Value>& value, v8::Isolate* isolate)
        {
            return value;
        }
    };

    // Converts a JavaScript value to an array as per the Web IDL specification:
    // http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-array
    template <class T, class V8T>
    Vector<RefPtr<T> > toRefPtrNativeArrayUnchecked(v8::Local<v8::Value> v8Value, uint32_t length, v8::Isolate* isolate, bool* success = 0)
    {
        Vector<RefPtr<T> > result;
        result.reserveInitialCapacity(length);
        v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
        for (uint32_t i = 0; i < length; ++i) {
            v8::Handle<v8::Value> element = object->Get(i);

            if (V8T::hasInstance(element, isolate, worldType(isolate))) {
                v8::Handle<v8::Object> elementObject = v8::Handle<v8::Object>::Cast(element);
                result.uncheckedAppend(V8T::toNative(elementObject));
            } else {
                if (success)
                    *success = false;
                throwTypeError("Invalid Array element type", isolate);
                return Vector<RefPtr<T> >();
            }
        }
        return result;
    }

    template <class T, class V8T>
    Vector<RefPtr<T> > toRefPtrNativeArray(v8::Handle<v8::Value> value, int argumentIndex, v8::Isolate* isolate, bool* success = 0)
    {
        if (success)
            *success = true;

        v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
        uint32_t length = 0;
        if (value->IsArray()) {
            length = v8::Local<v8::Array>::Cast(v8Value)->Length();
        } else if (toV8Sequence(value, length, isolate).IsEmpty()) {
            throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex), isolate);
            return Vector<RefPtr<T> >();
        }

        return toRefPtrNativeArrayUnchecked<T, V8T>(v8Value, length, isolate, success);
    }

    template <class T, class V8T>
    Vector<RefPtr<T> > toRefPtrNativeArray(v8::Handle<v8::Value> value, const String& propertyName, v8::Isolate* isolate, bool* success = 0)
    {
        if (success)
            *success = true;

        v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
        uint32_t length = 0;
        if (value->IsArray()) {
            length = v8::Local<v8::Array>::Cast(v8Value)->Length();
        } else if (toV8Sequence(value, length, isolate).IsEmpty()) {
            throwTypeError(ExceptionMessages::notASequenceTypeProperty(propertyName), isolate);
            return Vector<RefPtr<T> >();
        }

        return toRefPtrNativeArrayUnchecked<T, V8T>(v8Value, length, isolate, success);
    }

    // Converts a JavaScript value to an array as per the Web IDL specification:
    // http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-array
    template <class T>
    Vector<T> toNativeArray(v8::Handle<v8::Value> value, int argumentIndex, v8::Isolate* isolate)
    {
        v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
        uint32_t length = 0;
        if (value->IsArray()) {
            length = v8::Local<v8::Array>::Cast(v8Value)->Length();
        } else if (toV8Sequence(value, length, isolate).IsEmpty()) {
            throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(argumentIndex), isolate);
            return Vector<T>();
        }

        Vector<T> result;
        result.reserveInitialCapacity(length);
        typedef NativeValueTraits<T> TraitsType;
        v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
        for (uint32_t i = 0; i < length; ++i)
            result.uncheckedAppend(TraitsType::nativeValue(object->Get(i), isolate));
        return result;
    }

    template <class T>
    Vector<T> toNativeArguments(const v8::FunctionCallbackInfo<v8::Value>& info, int startIndex)
    {
        ASSERT(startIndex <= info.Length());
        Vector<T> result;
        typedef NativeValueTraits<T> TraitsType;
        int length = info.Length();
        result.reserveInitialCapacity(length);
        for (int i = startIndex; i < length; ++i)
            result.uncheckedAppend(TraitsType::nativeValue(info[i], info.GetIsolate()));
        return result;
    }

    // Validates that the passed object is a sequence type per WebIDL spec
    // http://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-sequence
    inline v8::Handle<v8::Value> toV8Sequence(v8::Handle<v8::Value> value, uint32_t& length, v8::Isolate* isolate)
    {
        // Attempt converting to a sequence if the value is not already an array but is
        // any kind of object except for a native Date object or a native RegExp object.
        ASSERT(!value->IsArray());
        // FIXME: Do we really need to special case Date and RegExp object?
        // https://www.w3.org/Bugs/Public/show_bug.cgi?id=22806
        if (!value->IsObject() || value->IsDate() || value->IsRegExp()) {
            // The caller is responsible for reporting a TypeError.
            return v8Undefined();
        }

        v8::Local<v8::Value> v8Value(v8::Local<v8::Value>::New(isolate, value));
        v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(v8Value);
        v8::Local<v8::String> lengthSymbol = v8::String::NewFromUtf8(isolate, "length", v8::String::kInternalizedString, 6);

        // FIXME: The specification states that the length property should be used as fallback, if value
        // is not a platform object that supports indexed properties. If it supports indexed properties,
        // length should actually be one greater than value’s maximum indexed property index.
        V8TRYCATCH(v8::Local<v8::Value>, lengthValue, object->Get(lengthSymbol));

        if (lengthValue->IsUndefined() || lengthValue->IsNull()) {
            // The caller is responsible for reporting a TypeError.
            return v8Undefined();
        }

        V8TRYCATCH(uint32_t, sequenceLength, lengthValue->Int32Value());
        length = sequenceLength;

        return v8Value;
    }

    PassRefPtr<NodeFilter> toNodeFilter(v8::Handle<v8::Value>, v8::Isolate*);

    inline bool isUndefinedOrNull(v8::Handle<v8::Value> value)
    {
        return value->IsNull() || value->IsUndefined();
    }

    // Returns true if the provided object is to be considered a 'host object', as used in the
    // HTML5 structured clone algorithm.
    inline bool isHostObject(v8::Handle<v8::Object> object)
    {
        // If the object has any internal fields, then we won't be able to serialize or deserialize
        // them; conveniently, this is also a quick way to detect DOM wrapper objects, because
        // the mechanism for these relies on data stored in these fields. We should
        // catch external array data as a special case.
        return object->InternalFieldCount() || object->HasIndexedPropertiesInExternalArrayData();
    }

    inline v8::Handle<v8::Boolean> v8Boolean(bool value, v8::Isolate* isolate)
    {
        return value ? v8::True(isolate) : v8::False(isolate);
    }

    // Since v8Boolean(value, isolate) crashes if we pass a null isolate,
    // we need to use v8BooleanWithCheck(value, isolate) if an isolate can be null.
    //
    // FIXME: Remove all null isolates from V8 bindings, and remove v8BooleanWithCheck(value, isolate).
    inline v8::Handle<v8::Boolean> v8BooleanWithCheck(bool value, v8::Isolate* isolate)
    {
        return isolate ? v8Boolean(value, isolate) : v8Boolean(value, v8::Isolate::GetCurrent());
    }

    inline double toWebCoreDate(v8::Handle<v8::Value> object)
    {
        if (object->IsDate())
            return v8::Handle<v8::Date>::Cast(object)->ValueOf();
        if (object->IsNumber())
            return object->NumberValue();
        return std::numeric_limits<double>::quiet_NaN();
    }

    inline v8::Handle<v8::Value> v8DateOrNull(double value, v8::Isolate* isolate)
    {
        ASSERT(isolate);
        return std::isfinite(value) ? v8::Date::New(isolate, value) : v8::Handle<v8::Value>::Cast(v8::Null(isolate));
    }

    inline v8::Handle<v8::String> v8AtomicString(v8::Isolate* isolate, const char* str)
    {
        ASSERT(isolate);
        return v8::String::NewFromUtf8(isolate, str, v8::String::kInternalizedString, strlen(str));
    }

    inline v8::Handle<v8::String> v8AtomicString(v8::Isolate* isolate, const char* str, size_t length)
    {
        ASSERT(isolate);
        return v8::String::NewFromUtf8(isolate, str, v8::String::kInternalizedString, length);
    }

    bool isNonWindowContextsAllowed();
    void setNonWindowContextsAllowed(bool allowed);

    v8::Handle<v8::FunctionTemplate> createRawTemplate(v8::Isolate*);

    PassRefPtr<XPathNSResolver> toXPathNSResolver(v8::Handle<v8::Value>, v8::Isolate*);

    v8::Handle<v8::Object> toInnerGlobalObject(v8::Handle<v8::Context>);
    DOMWindow* toDOMWindow(v8::Handle<v8::Context>);
    ExecutionContext* toExecutionContext(v8::Handle<v8::Context>);

    DOMWindow* activeDOMWindow();
    ExecutionContext* activeExecutionContext();
    DOMWindow* firstDOMWindow();
    Document* currentDocument();

    // Returns the context associated with a ExecutionContext.
    v8::Local<v8::Context> toV8Context(ExecutionContext*, DOMWrapperWorld*);

    // Returns the frame object of the window object associated with
    // a context, if the window is currently being displayed in the Frame.
    Frame* toFrameIfNotDetached(v8::Handle<v8::Context>);

    inline DOMWrapperWorld* isolatedWorldForEnteredContext(v8::Isolate* isolate)
    {
        v8::Handle<v8::Context> context = isolate->GetEnteredContext();
        if (context.IsEmpty())
            return 0;
        return DOMWrapperWorld::isolatedWorld(context);
    }

    // FIXME: This will be soon embedded in the generated code.
    template<class Collection> static void indexedPropertyEnumerator(const v8::PropertyCallbackInfo<v8::Array>& info)
    {
        Collection* collection = reinterpret_cast<Collection*>(info.Holder()->GetAlignedPointerFromInternalField(v8DOMWrapperObjectIndex));
        int length = collection->length();
        v8::Handle<v8::Array> properties = v8::Array::New(info.GetIsolate(), length);
        for (int i = 0; i < length; ++i) {
            // FIXME: Do we need to check that the item function returns a non-null value for this index?
            v8::Handle<v8::Integer> integer = v8::Integer::New(i, info.GetIsolate());
            properties->Set(integer, integer);
        }
        v8SetReturnValue(info, properties);
    }

    // If the current context causes out of memory, JavaScript setting
    // is disabled and it returns true.
    bool handleOutOfMemory();
    v8::Local<v8::Value> handleMaxRecursionDepthExceeded(v8::Isolate*);

    void crashIfV8IsDead();

    template <class T>
    v8::Handle<T> unsafeHandleFromRawValue(const T* value)
    {
        const v8::Handle<T>* handle = reinterpret_cast<const v8::Handle<T>*>(&value);
        return *handle;
    }

    // Attaches |environment| to |function| and returns it.
    inline v8::Local<v8::Function> createClosure(v8::FunctionCallback function, v8::Handle<v8::Value> environment, v8::Isolate* isolate)
    {
        return v8::Function::New(isolate, function, environment);
    }

    v8::Local<v8::Value> getHiddenValueFromMainWorldWrapper(v8::Isolate*, ScriptWrappable*, v8::Handle<v8::String> key);

    v8::Isolate* mainThreadIsolate();
    v8::Isolate* toIsolate(ExecutionContext*);
    v8::Isolate* toIsolate(Frame*);

    // Can only be called by blink::initialize
    void setMainThreadIsolate(v8::Isolate*);

    // Converts a DOM object to a v8 value.
    // This is a no-inline version of toV8(). If you want to call toV8()
    // without creating #include cycles, you can use this function instead.
    // Each specialized implementation will be generated.
    template<typename T>
    v8::Handle<v8::Value> toV8NoInline(T* impl, v8::Handle<v8::Object> creationContext, v8::Isolate*);
} // namespace WebCore

#endif // V8Binding_h

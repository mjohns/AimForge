#pragma once

#include <functional>

namespace aim {

template <typename T>
struct Field {
  Field(std::function<T()> get,
        std::function<void(T)> set,
        std::function<void()> clear,
        std::function<bool()> has)
      : get(std::move(get)), set(std::move(set)), clear(std::move(clear)), has(std::move(has)) {}

  std::function<T()> get;
  std::function<void(T)> set;
  std::function<void()> clear;
  std::function<bool()> has;
};

static Field<float> MultiplyField(Field<float> unscaled, float multiplier) {
  std::function<float()> get = [=]() { return unscaled.get() * multiplier; };
  std::function<void(float)> set = [=](float value) { unscaled.set(value / multiplier); };
  return Field<float>(get, set, unscaled.clear, unscaled.has);
}

static Field<float> CreateOptionalFloatField(std::optional<float>* optional_value) {
  std::function<float()> get = [=]() { return *(*optional_value); };
  std::function<void(float)> set = [=](float value) { *optional_value = value; };
  std::function<void()> clear = [=]() { *optional_value = {}; };
  std::function<bool()> has = [=]() { return optional_value->has_value(); };
  return Field<float>(get, set, clear, has);
}

static Field<float> CreateFloatField(float* value) {
  std::function<float()> get = [=]() { return *value; };
  std::function<void(float)> set = [=](float new_value) { *value = new_value; };
  std::function<void()> clear = [=]() { *value = 0; };
  std::function<bool()> has = [=]() { return true; };
  return Field<float>(get, set, clear, has);
}

template <typename T>
struct JitteredField {
  JitteredField(Field<T> value, Field<T> jitter) : value(value), jitter(jitter) {}

  Field<T> value;
  Field<T> jitter;
};

#define PROTO_FIELD(T, ProtoClass, instance, field_name)                    \
  aim::Field<T>(std::bind_front(&ProtoClass::field_name, instance),         \
                std::bind_front(&ProtoClass::set_##field_name, instance),   \
                std::bind_front(&ProtoClass::clear_##field_name, instance), \
                std::bind_front(&ProtoClass::has_##field_name, instance))

#define PROTO_FLOAT_FIELD(ProtoClass, instance, field_name) \
  PROTO_FIELD(float, ProtoClass, instance, field_name)
#define PROTO_BOOL_FIELD(ProtoClass, instance, field_name) \
  PROTO_FIELD(bool, ProtoClass, instance, field_name)
#define PROTO_INT_FIELD(ProtoClass, instance, field_name) \
  PROTO_FIELD(int, ProtoClass, instance, field_name)
#define PROTO_STRING_FIELD(ProtoClass, instance, field_name) \
  PROTO_FIELD(std::string, ProtoClass, instance, field_name)
// A float field that is displayed multiplied by 100. i.e. real value is 0.5 but shows 50%
#define PROTO_PERCENT_FIELD(ProtoClass, instance, field_name) \
  MultiplyField(PROTO_FIELD(float, ProtoClass, instance, field_name), 100)

#define PROTO_JITTERED_FIELD(ProtoClass, instance, field_name)              \
  JitteredField<float>(PROTO_FLOAT_FIELD(ProtoClass, instance, field_name), \
                       PROTO_FLOAT_FIELD(ProtoClass, instance, field_name##_jitter))

template <typename T>
struct PtrField {
  PtrField(std::function<T()> get,
           std::function<T*()> get_mutable,
           std::function<void()> clear,
           std::function<bool()> has)
      : get(std::move(get)),
        get_mutable(std::move(get_mutable)),
        clear(std::move(clear)),
        has(std::move(has)) {}

  std::function<T()> get;
  std::function<T*()> get_mutable;
  std::function<void()> clear;
  std::function<bool()> has;
};

#define PROTO_PTR_FIELD(T, ProtoClass, instance, field_name)                           \
  aim::PtrField<##T>(std::bind_front(&##ProtoClass::##field_name, ##instance),         \
                     std::bind_front(&##ProtoClass::mutable_##field_name, ##instance), \
                     std::bind_front(&##ProtoClass::clear_##field_name, ##instance),   \
                     std::bind_front(&##ProtoClass::has_##field_name, ##instance))

}  // namespace aim

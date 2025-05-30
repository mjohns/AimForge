#pragma once

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

template <typename T>
struct JitteredField {
  JitteredField(Field<T> value, Field<T> jitter) : value(value), jitter(jitter) {}

  Field<T> value;
  Field<T> jitter;
};

#define PROTO_FIELD(T, ProtoClass, instance, field_name)                     \
  Field<##T>(std::bind_front(&##ProtoClass::##field_name, ##instance),       \
             std::bind_front(&##ProtoClass::set_##field_name, ##instance),   \
             std::bind_front(&##ProtoClass::clear_##field_name, ##instance), \
             std::bind_front(&##ProtoClass::has_##field_name, ##instance))

#define PROTO_FLOAT_FIELD(ProtoClass, instance, field_name) \
  PROTO_FIELD(float, ProtoClass, instance, field_name)
#define PROTO_BOOL_FIELD(ProtoClass, instance, field_name) \
  PROTO_FIELD(bool, ProtoClass, instance, field_name)
#define PROTO_STRING_FIELD(ProtoClass, instance, field_name) \
  PROTO_FIELD(std::string, ProtoClass, instance, field_name)

#define PROTO_JITTERED_FIELD(ProtoClass, instance, field_name)              \
  JitteredField<float>(PROTO_FLOAT_FIELD(ProtoClass, instance, field_name), \
                       PROTO_FLOAT_FIELD(ProtoClass, instance, field_name##_jitter))

}  // namespace aim

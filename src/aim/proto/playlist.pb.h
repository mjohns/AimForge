// Generated by the protocol buffer compiler.  DO NOT EDIT!
// NO CHECKED-IN PROTOBUF GENCODE
// source: playlist.proto
// Protobuf C++ Version: 5.29.3

#ifndef playlist_2eproto_2epb_2eh
#define playlist_2eproto_2epb_2eh

#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "google/protobuf/runtime_version.h"
#if PROTOBUF_VERSION != 5029003
#error "Protobuf C++ gencode is built with an incompatible version of"
#error "Protobuf C++ headers/runtime. See"
#error "https://protobuf.dev/support/cross-version-runtime-guarantee/#cpp"
#endif
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/arenastring.h"
#include "google/protobuf/generated_message_tctable_decl.h"
#include "google/protobuf/generated_message_util.h"
#include "google/protobuf/metadata_lite.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/repeated_field.h"  // IWYU pragma: export
#include "google/protobuf/extension_set.h"  // IWYU pragma: export
#include "google/protobuf/unknown_field_set.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"

#define PROTOBUF_INTERNAL_EXPORT_playlist_2eproto

namespace google {
namespace protobuf {
namespace internal {
template <typename T>
::absl::string_view GetAnyMessageName();
}  // namespace internal
}  // namespace protobuf
}  // namespace google

// Internal implementation detail -- do not use these members.
struct TableStruct_playlist_2eproto {
  static const ::uint32_t offsets[];
};
extern const ::google::protobuf::internal::DescriptorTable
    descriptor_table_playlist_2eproto;
namespace aim {
class PlaylistDef;
struct PlaylistDefDefaultTypeInternal;
extern PlaylistDefDefaultTypeInternal _PlaylistDef_default_instance_;
class PlaylistItem;
struct PlaylistItemDefaultTypeInternal;
extern PlaylistItemDefaultTypeInternal _PlaylistItem_default_instance_;
}  // namespace aim
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google

namespace aim {

// ===================================================================


// -------------------------------------------------------------------

class PlaylistItem final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:aim.PlaylistItem) */ {
 public:
  inline PlaylistItem() : PlaylistItem(nullptr) {}
  ~PlaylistItem() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(PlaylistItem* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(PlaylistItem));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlaylistItem(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlaylistItem(const PlaylistItem& from) : PlaylistItem(nullptr, from) {}
  inline PlaylistItem(PlaylistItem&& from) noexcept
      : PlaylistItem(nullptr, std::move(from)) {}
  inline PlaylistItem& operator=(const PlaylistItem& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlaylistItem& operator=(PlaylistItem&& from) noexcept {
    if (this == &from) return *this;
    if (::google::protobuf::internal::CanMoveWithInternalSwap(GetArena(), from.GetArena())) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const PlaylistItem& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlaylistItem* internal_default_instance() {
    return reinterpret_cast<const PlaylistItem*>(
        &_PlaylistItem_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 1;
  friend void swap(PlaylistItem& a, PlaylistItem& b) { a.Swap(&b); }
  inline void Swap(PlaylistItem* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(PlaylistItem* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlaylistItem* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<PlaylistItem>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlaylistItem& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlaylistItem& from) { PlaylistItem::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* _InternalSerialize(
      const MessageLite& msg, ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(PlaylistItem* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "aim.PlaylistItem"; }

 protected:
  explicit PlaylistItem(::google::protobuf::Arena* arena);
  PlaylistItem(::google::protobuf::Arena* arena, const PlaylistItem& from);
  PlaylistItem(::google::protobuf::Arena* arena, PlaylistItem&& from) noexcept
      : PlaylistItem(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* GetClassData() const PROTOBUF_FINAL;
  static void* PlacementNew_(const void*, void* mem,
                             ::google::protobuf::Arena* arena);
  static constexpr auto InternalNewImpl_();
  static const ::google::protobuf::internal::ClassDataFull _class_data_;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kScenarioFieldNumber = 1,
    kNumPlaysFieldNumber = 2,
    kAutoNextFieldNumber = 3,
  };
  // string scenario = 1;
  bool has_scenario() const;
  void clear_scenario() ;
  const std::string& scenario() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_scenario(Arg_&& arg, Args_... args);
  std::string* mutable_scenario();
  PROTOBUF_NODISCARD std::string* release_scenario();
  void set_allocated_scenario(std::string* value);

  private:
  const std::string& _internal_scenario() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_scenario(
      const std::string& value);
  std::string* _internal_mutable_scenario();

  public:
  // int32 num_plays = 2;
  bool has_num_plays() const;
  void clear_num_plays() ;
  ::int32_t num_plays() const;
  void set_num_plays(::int32_t value);

  private:
  ::int32_t _internal_num_plays() const;
  void _internal_set_num_plays(::int32_t value);

  public:
  // bool auto_next = 3;
  bool has_auto_next() const;
  void clear_auto_next() ;
  bool auto_next() const;
  void set_auto_next(bool value);

  private:
  bool _internal_auto_next() const;
  void _internal_set_auto_next(bool value);

  public:
  // @@protoc_insertion_point(class_scope:aim.PlaylistItem)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      2, 3, 0,
      33, 2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const PlaylistItem& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::internal::ArenaStringPtr scenario_;
    ::int32_t num_plays_;
    bool auto_next_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_playlist_2eproto;
};
// -------------------------------------------------------------------

class PlaylistDef final : public ::google::protobuf::Message
/* @@protoc_insertion_point(class_definition:aim.PlaylistDef) */ {
 public:
  inline PlaylistDef() : PlaylistDef(nullptr) {}
  ~PlaylistDef() PROTOBUF_FINAL;

#if defined(PROTOBUF_CUSTOM_VTABLE)
  void operator delete(PlaylistDef* msg, std::destroying_delete_t) {
    SharedDtor(*msg);
    ::google::protobuf::internal::SizedDelete(msg, sizeof(PlaylistDef));
  }
#endif

  template <typename = void>
  explicit PROTOBUF_CONSTEXPR PlaylistDef(
      ::google::protobuf::internal::ConstantInitialized);

  inline PlaylistDef(const PlaylistDef& from) : PlaylistDef(nullptr, from) {}
  inline PlaylistDef(PlaylistDef&& from) noexcept
      : PlaylistDef(nullptr, std::move(from)) {}
  inline PlaylistDef& operator=(const PlaylistDef& from) {
    CopyFrom(from);
    return *this;
  }
  inline PlaylistDef& operator=(PlaylistDef&& from) noexcept {
    if (this == &from) return *this;
    if (::google::protobuf::internal::CanMoveWithInternalSwap(GetArena(), from.GetArena())) {
      InternalSwap(&from);
    } else {
      CopyFrom(from);
    }
    return *this;
  }

  inline const ::google::protobuf::UnknownFieldSet& unknown_fields() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance);
  }
  inline ::google::protobuf::UnknownFieldSet* mutable_unknown_fields()
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return _internal_metadata_.mutable_unknown_fields<::google::protobuf::UnknownFieldSet>();
  }

  static const ::google::protobuf::Descriptor* descriptor() {
    return GetDescriptor();
  }
  static const ::google::protobuf::Descriptor* GetDescriptor() {
    return default_instance().GetMetadata().descriptor;
  }
  static const ::google::protobuf::Reflection* GetReflection() {
    return default_instance().GetMetadata().reflection;
  }
  static const PlaylistDef& default_instance() {
    return *internal_default_instance();
  }
  static inline const PlaylistDef* internal_default_instance() {
    return reinterpret_cast<const PlaylistDef*>(
        &_PlaylistDef_default_instance_);
  }
  static constexpr int kIndexInFileMessages = 0;
  friend void swap(PlaylistDef& a, PlaylistDef& b) { a.Swap(&b); }
  inline void Swap(PlaylistDef* other) {
    if (other == this) return;
    if (::google::protobuf::internal::CanUseInternalSwap(GetArena(), other->GetArena())) {
      InternalSwap(other);
    } else {
      ::google::protobuf::internal::GenericSwap(this, other);
    }
  }
  void UnsafeArenaSwap(PlaylistDef* other) {
    if (other == this) return;
    ABSL_DCHECK(GetArena() == other->GetArena());
    InternalSwap(other);
  }

  // implements Message ----------------------------------------------

  PlaylistDef* New(::google::protobuf::Arena* arena = nullptr) const {
    return ::google::protobuf::Message::DefaultConstruct<PlaylistDef>(arena);
  }
  using ::google::protobuf::Message::CopyFrom;
  void CopyFrom(const PlaylistDef& from);
  using ::google::protobuf::Message::MergeFrom;
  void MergeFrom(const PlaylistDef& from) { PlaylistDef::MergeImpl(*this, from); }

  private:
  static void MergeImpl(
      ::google::protobuf::MessageLite& to_msg,
      const ::google::protobuf::MessageLite& from_msg);

  public:
  bool IsInitialized() const {
    return true;
  }
  ABSL_ATTRIBUTE_REINITIALIZES void Clear() PROTOBUF_FINAL;
  #if defined(PROTOBUF_CUSTOM_VTABLE)
  private:
  static ::size_t ByteSizeLong(const ::google::protobuf::MessageLite& msg);
  static ::uint8_t* _InternalSerialize(
      const MessageLite& msg, ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream);

  public:
  ::size_t ByteSizeLong() const { return ByteSizeLong(*this); }
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const {
    return _InternalSerialize(*this, target, stream);
  }
  #else   // PROTOBUF_CUSTOM_VTABLE
  ::size_t ByteSizeLong() const final;
  ::uint8_t* _InternalSerialize(
      ::uint8_t* target,
      ::google::protobuf::io::EpsCopyOutputStream* stream) const final;
  #endif  // PROTOBUF_CUSTOM_VTABLE
  int GetCachedSize() const { return _impl_._cached_size_.Get(); }

  private:
  void SharedCtor(::google::protobuf::Arena* arena);
  static void SharedDtor(MessageLite& self);
  void InternalSwap(PlaylistDef* other);
 private:
  template <typename T>
  friend ::absl::string_view(
      ::google::protobuf::internal::GetAnyMessageName)();
  static ::absl::string_view FullMessageName() { return "aim.PlaylistDef"; }

 protected:
  explicit PlaylistDef(::google::protobuf::Arena* arena);
  PlaylistDef(::google::protobuf::Arena* arena, const PlaylistDef& from);
  PlaylistDef(::google::protobuf::Arena* arena, PlaylistDef&& from) noexcept
      : PlaylistDef(arena) {
    *this = ::std::move(from);
  }
  const ::google::protobuf::internal::ClassData* GetClassData() const PROTOBUF_FINAL;
  static void* PlacementNew_(const void*, void* mem,
                             ::google::protobuf::Arena* arena);
  static constexpr auto InternalNewImpl_();
  static const ::google::protobuf::internal::ClassDataFull _class_data_;

 public:
  ::google::protobuf::Metadata GetMetadata() const;
  // nested types ----------------------------------------------------

  // accessors -------------------------------------------------------
  enum : int {
    kItemsFieldNumber = 1,
    kDescriptionFieldNumber = 2,
  };
  // repeated .aim.PlaylistItem items = 1;
  int items_size() const;
  private:
  int _internal_items_size() const;

  public:
  void clear_items() ;
  ::aim::PlaylistItem* mutable_items(int index);
  ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>* mutable_items();

  private:
  const ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>& _internal_items() const;
  ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>* _internal_mutable_items();
  public:
  const ::aim::PlaylistItem& items(int index) const;
  ::aim::PlaylistItem* add_items();
  const ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>& items() const;
  // string description = 2;
  bool has_description() const;
  void clear_description() ;
  const std::string& description() const;
  template <typename Arg_ = const std::string&, typename... Args_>
  void set_description(Arg_&& arg, Args_... args);
  std::string* mutable_description();
  PROTOBUF_NODISCARD std::string* release_description();
  void set_allocated_description(std::string* value);

  private:
  const std::string& _internal_description() const;
  inline PROTOBUF_ALWAYS_INLINE void _internal_set_description(
      const std::string& value);
  std::string* _internal_mutable_description();

  public:
  // @@protoc_insertion_point(class_scope:aim.PlaylistDef)
 private:
  class _Internal;
  friend class ::google::protobuf::internal::TcParser;
  static const ::google::protobuf::internal::TcParseTable<
      1, 2, 1,
      35, 2>
      _table_;

  friend class ::google::protobuf::MessageLite;
  friend class ::google::protobuf::Arena;
  template <typename T>
  friend class ::google::protobuf::Arena::InternalHelper;
  using InternalArenaConstructable_ = void;
  using DestructorSkippable_ = void;
  struct Impl_ {
    inline explicit constexpr Impl_(
        ::google::protobuf::internal::ConstantInitialized) noexcept;
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena);
    inline explicit Impl_(::google::protobuf::internal::InternalVisibility visibility,
                          ::google::protobuf::Arena* arena, const Impl_& from,
                          const PlaylistDef& from_msg);
    ::google::protobuf::internal::HasBits<1> _has_bits_;
    ::google::protobuf::internal::CachedSize _cached_size_;
    ::google::protobuf::RepeatedPtrField< ::aim::PlaylistItem > items_;
    ::google::protobuf::internal::ArenaStringPtr description_;
    PROTOBUF_TSAN_DECLARE_MEMBER
  };
  union { Impl_ _impl_; };
  friend struct ::TableStruct_playlist_2eproto;
};

// ===================================================================




// ===================================================================


#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif  // __GNUC__
// -------------------------------------------------------------------

// PlaylistDef

// repeated .aim.PlaylistItem items = 1;
inline int PlaylistDef::_internal_items_size() const {
  return _internal_items().size();
}
inline int PlaylistDef::items_size() const {
  return _internal_items_size();
}
inline void PlaylistDef::clear_items() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.items_.Clear();
}
inline ::aim::PlaylistItem* PlaylistDef::mutable_items(int index)
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable:aim.PlaylistDef.items)
  return _internal_mutable_items()->Mutable(index);
}
inline ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>* PlaylistDef::mutable_items()
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_mutable_list:aim.PlaylistDef.items)
  ::google::protobuf::internal::TSanWrite(&_impl_);
  return _internal_mutable_items();
}
inline const ::aim::PlaylistItem& PlaylistDef::items(int index) const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:aim.PlaylistDef.items)
  return _internal_items().Get(index);
}
inline ::aim::PlaylistItem* PlaylistDef::add_items() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  ::aim::PlaylistItem* _add = _internal_mutable_items()->Add();
  // @@protoc_insertion_point(field_add:aim.PlaylistDef.items)
  return _add;
}
inline const ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>& PlaylistDef::items() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_list:aim.PlaylistDef.items)
  return _internal_items();
}
inline const ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>&
PlaylistDef::_internal_items() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.items_;
}
inline ::google::protobuf::RepeatedPtrField<::aim::PlaylistItem>*
PlaylistDef::_internal_mutable_items() {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return &_impl_.items_;
}

// string description = 2;
inline bool PlaylistDef::has_description() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  return value;
}
inline void PlaylistDef::clear_description() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.description_.ClearToEmpty();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const std::string& PlaylistDef::description() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:aim.PlaylistDef.description)
  return _internal_description();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void PlaylistDef::set_description(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  _impl_.description_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:aim.PlaylistDef.description)
}
inline std::string* PlaylistDef::mutable_description() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_description();
  // @@protoc_insertion_point(field_mutable:aim.PlaylistDef.description)
  return _s;
}
inline const std::string& PlaylistDef::_internal_description() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.description_.Get();
}
inline void PlaylistDef::_internal_set_description(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  _impl_.description_.Set(value, GetArena());
}
inline std::string* PlaylistDef::_internal_mutable_description() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  return _impl_.description_.Mutable( GetArena());
}
inline std::string* PlaylistDef::release_description() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:aim.PlaylistDef.description)
  if ((_impl_._has_bits_[0] & 0x00000001u) == 0) {
    return nullptr;
  }
  _impl_._has_bits_[0] &= ~0x00000001u;
  auto* released = _impl_.description_.Release();
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString()) {
    _impl_.description_.Set("", GetArena());
  }
  return released;
}
inline void PlaylistDef::set_allocated_description(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  _impl_.description_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.description_.IsDefault()) {
    _impl_.description_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:aim.PlaylistDef.description)
}

// -------------------------------------------------------------------

// PlaylistItem

// string scenario = 1;
inline bool PlaylistItem::has_scenario() const {
  bool value = (_impl_._has_bits_[0] & 0x00000001u) != 0;
  return value;
}
inline void PlaylistItem::clear_scenario() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.scenario_.ClearToEmpty();
  _impl_._has_bits_[0] &= ~0x00000001u;
}
inline const std::string& PlaylistItem::scenario() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  // @@protoc_insertion_point(field_get:aim.PlaylistItem.scenario)
  return _internal_scenario();
}
template <typename Arg_, typename... Args_>
inline PROTOBUF_ALWAYS_INLINE void PlaylistItem::set_scenario(Arg_&& arg,
                                                     Args_... args) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  _impl_.scenario_.Set(static_cast<Arg_&&>(arg), args..., GetArena());
  // @@protoc_insertion_point(field_set:aim.PlaylistItem.scenario)
}
inline std::string* PlaylistItem::mutable_scenario() ABSL_ATTRIBUTE_LIFETIME_BOUND {
  std::string* _s = _internal_mutable_scenario();
  // @@protoc_insertion_point(field_mutable:aim.PlaylistItem.scenario)
  return _s;
}
inline const std::string& PlaylistItem::_internal_scenario() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.scenario_.Get();
}
inline void PlaylistItem::_internal_set_scenario(const std::string& value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  _impl_.scenario_.Set(value, GetArena());
}
inline std::string* PlaylistItem::_internal_mutable_scenario() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_._has_bits_[0] |= 0x00000001u;
  return _impl_.scenario_.Mutable( GetArena());
}
inline std::string* PlaylistItem::release_scenario() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  // @@protoc_insertion_point(field_release:aim.PlaylistItem.scenario)
  if ((_impl_._has_bits_[0] & 0x00000001u) == 0) {
    return nullptr;
  }
  _impl_._has_bits_[0] &= ~0x00000001u;
  auto* released = _impl_.scenario_.Release();
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString()) {
    _impl_.scenario_.Set("", GetArena());
  }
  return released;
}
inline void PlaylistItem::set_allocated_scenario(std::string* value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  if (value != nullptr) {
    _impl_._has_bits_[0] |= 0x00000001u;
  } else {
    _impl_._has_bits_[0] &= ~0x00000001u;
  }
  _impl_.scenario_.SetAllocated(value, GetArena());
  if (::google::protobuf::internal::DebugHardenForceCopyDefaultString() && _impl_.scenario_.IsDefault()) {
    _impl_.scenario_.Set("", GetArena());
  }
  // @@protoc_insertion_point(field_set_allocated:aim.PlaylistItem.scenario)
}

// int32 num_plays = 2;
inline bool PlaylistItem::has_num_plays() const {
  bool value = (_impl_._has_bits_[0] & 0x00000002u) != 0;
  return value;
}
inline void PlaylistItem::clear_num_plays() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.num_plays_ = 0;
  _impl_._has_bits_[0] &= ~0x00000002u;
}
inline ::int32_t PlaylistItem::num_plays() const {
  // @@protoc_insertion_point(field_get:aim.PlaylistItem.num_plays)
  return _internal_num_plays();
}
inline void PlaylistItem::set_num_plays(::int32_t value) {
  _internal_set_num_plays(value);
  _impl_._has_bits_[0] |= 0x00000002u;
  // @@protoc_insertion_point(field_set:aim.PlaylistItem.num_plays)
}
inline ::int32_t PlaylistItem::_internal_num_plays() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.num_plays_;
}
inline void PlaylistItem::_internal_set_num_plays(::int32_t value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.num_plays_ = value;
}

// bool auto_next = 3;
inline bool PlaylistItem::has_auto_next() const {
  bool value = (_impl_._has_bits_[0] & 0x00000004u) != 0;
  return value;
}
inline void PlaylistItem::clear_auto_next() {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.auto_next_ = false;
  _impl_._has_bits_[0] &= ~0x00000004u;
}
inline bool PlaylistItem::auto_next() const {
  // @@protoc_insertion_point(field_get:aim.PlaylistItem.auto_next)
  return _internal_auto_next();
}
inline void PlaylistItem::set_auto_next(bool value) {
  _internal_set_auto_next(value);
  _impl_._has_bits_[0] |= 0x00000004u;
  // @@protoc_insertion_point(field_set:aim.PlaylistItem.auto_next)
}
inline bool PlaylistItem::_internal_auto_next() const {
  ::google::protobuf::internal::TSanRead(&_impl_);
  return _impl_.auto_next_;
}
inline void PlaylistItem::_internal_set_auto_next(bool value) {
  ::google::protobuf::internal::TSanWrite(&_impl_);
  _impl_.auto_next_ = value;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif  // __GNUC__

// @@protoc_insertion_point(namespace_scope)
}  // namespace aim


// @@protoc_insertion_point(global_scope)

#include "google/protobuf/port_undef.inc"

#endif  // playlist_2eproto_2epb_2eh

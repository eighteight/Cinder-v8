// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_ELEMENTS_H_
#define V8_ELEMENTS_H_

#include "elements-kind.h"
#include "objects.h"
#include "heap.h"
#include "isolate.h"

namespace v8 {
namespace internal {

// Abstract base class for handles that can operate on objects with differing
// ElementsKinds.
class ElementsAccessor {
 public:
  explicit ElementsAccessor(const char* name) : name_(name) { }
  virtual ~ElementsAccessor() { }

  virtual ElementsKind kind() const = 0;
  const char* name() const { return name_; }

  // Checks the elements of an object for consistency, asserting when a problem
  // is found.
  virtual void Validate(Handle<JSObject> obj) = 0;

  // Returns true if a holder contains an element with the specified key
  // without iterating up the prototype chain.  The caller can optionally pass
  // in the backing store to use for the check, which must be compatible with
  // the ElementsKind of the ElementsAccessor. If backing_store is NULL, the
  // holder->elements() is used as the backing store.
  virtual bool HasElement(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  inline bool HasElement(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key) {
    return HasElement(receiver, holder, key, handle(holder->elements()));
  }

  // Returns the element with the specified key or undefined if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual Handle<Object> Get(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  MUST_USE_RESULT inline Handle<Object> Get(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key) {
    return Get(receiver, holder, key, handle(holder->elements()));
  }

  // Returns an element's attributes, or ABSENT if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual PropertyAttributes GetAttributes(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  MUST_USE_RESULT inline PropertyAttributes GetAttributes(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key) {
    return GetAttributes(receiver, holder, key, handle(holder->elements()));
  }

  // Returns an element's type, or NONEXISTENT if there is no such
  // element. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual PropertyType GetType(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  MUST_USE_RESULT inline PropertyType GetType(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key) {
    return GetType(receiver, holder, key, handle(holder->elements()));
  }

  // Returns an element's accessors, or NULL if the element does not exist or
  // is plain. This method doesn't iterate up the prototype chain.  The caller
  // can optionally pass in the backing store to use for the check, which must
  // be compatible with the ElementsKind of the ElementsAccessor. If
  // backing_store is NULL, the holder->elements() is used as the backing store.
  MUST_USE_RESULT virtual MaybeHandle<AccessorPair> GetAccessorPair(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key,
      Handle<FixedArrayBase> backing_store) = 0;

  MUST_USE_RESULT inline MaybeHandle<AccessorPair> GetAccessorPair(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      uint32_t key) {
    return GetAccessorPair(receiver, holder, key, handle(holder->elements()));
  }

  // Modifies the length data property as specified for JSArrays and resizes the
  // underlying backing store accordingly. The method honors the semantics of
  // changing array sizes as defined in EcmaScript 5.1 15.4.5.2, i.e. array that
  // have non-deletable elements can only be shrunk to the size of highest
  // element that is non-deletable.
  MUST_USE_RESULT virtual Handle<Object> SetLength(
      Handle<JSArray> holder,
      Handle<Object> new_length) = 0;

  // Modifies both the length and capacity of a JSArray, resizing the underlying
  // backing store as necessary. This method does NOT honor the semantics of
  // EcmaScript 5.1 15.4.5.2, arrays can be shrunk beyond non-deletable
  // elements. This method should only be called for array expansion OR by
  // runtime JavaScript code that use InternalArrays and don't care about
  // EcmaScript 5.1 semantics.
  virtual void SetCapacityAndLength(
      Handle<JSArray> array,
      int capacity,
      int length) = 0;

  // Deletes an element in an object, returning a new elements backing store.
  MUST_USE_RESULT virtual Handle<Object> Delete(
      Handle<JSObject> holder,
      uint32_t key,
      JSReceiver::DeleteMode mode) = 0;

  // If kCopyToEnd is specified as the copy_size to CopyElements, it copies all
  // of elements from source after source_start to the destination array.
  static const int kCopyToEnd = -1;
  // If kCopyToEndAndInitializeToHole is specified as the copy_size to
  // CopyElements, it copies all of elements from source after source_start to
  // destination array, padding any remaining uninitialized elements in the
  // destination array with the hole.
  static const int kCopyToEndAndInitializeToHole = -2;

  // Copy elements from one backing store to another. Typically, callers specify
  // the source JSObject or JSArray in source_holder. If the holder's backing
  // store is available, it can be passed in source and source_holder is
  // ignored.
  virtual void CopyElements(
      Handle<FixedArrayBase> source,
      uint32_t source_start,
      ElementsKind source_kind,
      Handle<FixedArrayBase> destination,
      uint32_t destination_start,
      int copy_size) = 0;

  // TODO(ishell): Keeping |source_holder| parameter in a non-handlified form
  // helps avoiding ArrayConcat() builtin performance degradation.
  // Revisit this later.
  virtual void CopyElements(
      JSObject* source_holder,
      uint32_t source_start,
      ElementsKind source_kind,
      Handle<FixedArrayBase> destination,
      uint32_t destination_start,
      int copy_size) = 0;

  inline void CopyElements(
      Handle<JSObject> from_holder,
      Handle<FixedArrayBase> to,
      ElementsKind from_kind) {
    CopyElements(
      *from_holder, 0, from_kind, to, 0, kCopyToEndAndInitializeToHole);
  }

  virtual Handle<FixedArray> AddElementsToFixedArray(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      Handle<FixedArray> to,
      Handle<FixedArrayBase> from) = 0;

  inline Handle<FixedArray> AddElementsToFixedArray(
      Handle<Object> receiver,
      Handle<JSObject> holder,
      Handle<FixedArray> to) {
    return AddElementsToFixedArray(
        receiver, holder, to, handle(holder->elements()));
  }

  // Returns a shared ElementsAccessor for the specified ElementsKind.
  static ElementsAccessor* ForKind(ElementsKind elements_kind) {
    ASSERT(elements_kind < kElementsKindCount);
    return elements_accessors_[elements_kind];
  }

  // TODO(ishell): Temporary wrapper until handlified.
  inline static ElementsAccessor* ForArray(Handle<FixedArrayBase> array) {
    return ForArray(*array);
  }
  static ElementsAccessor* ForArray(FixedArrayBase* array);

  static void InitializeOncePerProcess();
  static void TearDown();

 protected:
  friend class SloppyArgumentsElementsAccessor;

  virtual uint32_t GetCapacity(FixedArrayBase* backing_store) = 0;

  // Element handlers distinguish between indexes and keys when they manipulate
  // elements.  Indexes refer to elements in terms of their location in the
  // underlying storage's backing store representation, and are between 0 and
  // GetCapacity.  Keys refer to elements in terms of the value that would be
  // specified in JavaScript to access the element. In most implementations,
  // keys are equivalent to indexes, and GetKeyForIndex returns the same value
  // it is passed. In the NumberDictionary ElementsAccessor, GetKeyForIndex maps
  // the index to a key using the KeyAt method on the NumberDictionary.
  virtual uint32_t GetKeyForIndex(Handle<FixedArrayBase> backing_store,
                                  uint32_t index) = 0;

  // TODO(ishell): Non-handlified versions, used only by accessors'
  // implementations. To be removed once elements.cc is handlified.
  MUST_USE_RESULT virtual MaybeObject* Get(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) = 0;

  MUST_USE_RESULT virtual PropertyAttributes GetAttributes(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) = 0;

  MUST_USE_RESULT virtual PropertyType GetType(
      Object* receiver,
      JSObject* holder,
      uint32_t key,
      FixedArrayBase* backing_store) = 0;

 private:
  static ElementsAccessor** elements_accessors_;
  const char* name_;

  DISALLOW_COPY_AND_ASSIGN(ElementsAccessor);
};

void CheckArrayAbuse(JSObject* obj, const char* op, uint32_t key,
                     bool allow_appending = false);

Handle<Object> ArrayConstructInitializeElements(Handle<JSArray> array,
                                                Arguments* args);

} }  // namespace v8::internal

#endif  // V8_ELEMENTS_H_

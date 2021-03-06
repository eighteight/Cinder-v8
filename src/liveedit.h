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

#ifndef V8_LIVEEDIT_H_
#define V8_LIVEEDIT_H_



// Live Edit feature implementation.
// User should be able to change script on already running VM. This feature
// matches hot swap features in other frameworks.
//
// The basic use-case is when user spots some mistake in function body
// from debugger and wishes to change the algorithm without restart.
//
// A single change always has a form of a simple replacement (in pseudo-code):
//   script.source[positions, positions+length] = new_string;
// Implementation first determines, which function's body includes this
// change area. Then both old and new versions of script are fully compiled
// in order to analyze, whether the function changed its outer scope
// expectations (or number of parameters). If it didn't, function's code is
// patched with a newly compiled code. If it did change, enclosing function
// gets patched. All inner functions are left untouched, whatever happened
// to them in a new script version. However, new version of code will
// instantiate newly compiled functions.


#include "allocation.h"
#include "compiler.h"

namespace v8 {
namespace internal {

// This class collects some specific information on structure of functions
// in a particular script. It gets called from compiler all the time, but
// actually records any data only when liveedit operation is in process;
// in any other time this class is very cheap.
//
// The primary interest of the Tracker is to record function scope structures
// in order to analyze whether function code maybe safely patched (with new
// code successfully reading existing data from function scopes). The Tracker
// also collects compiled function codes.
class LiveEditFunctionTracker {
 public:
  explicit LiveEditFunctionTracker(Isolate* isolate, FunctionLiteral* fun);
  ~LiveEditFunctionTracker();
  void RecordFunctionInfo(Handle<SharedFunctionInfo> info,
                          FunctionLiteral* lit, Zone* zone);
  void RecordRootFunctionInfo(Handle<Code> code);

  static bool IsActive(Isolate* isolate);

 private:
#ifdef ENABLE_DEBUGGER_SUPPORT
  Isolate* isolate_;
#endif
};

#ifdef ENABLE_DEBUGGER_SUPPORT

class LiveEdit : AllStatic {
 public:
  MUST_USE_RESULT static MaybeHandle<JSArray> GatherCompileInfo(
      Handle<Script> script,
      Handle<String> source);

  static void WrapSharedFunctionInfos(Handle<JSArray> array);

  static void ReplaceFunctionCode(Handle<JSArray> new_compile_info_array,
                                  Handle<JSArray> shared_info_array);

  static void FunctionSourceUpdated(Handle<JSArray> shared_info_array);

  // Updates script field in FunctionSharedInfo.
  static void SetFunctionScript(Handle<JSValue> function_wrapper,
                                Handle<Object> script_handle);

  static void PatchFunctionPositions(Handle<JSArray> shared_info_array,
                                     Handle<JSArray> position_change_array);

  // For a script updates its source field. If old_script_name is provided
  // (i.e. is a String), also creates a copy of the script with its original
  // source and sends notification to debugger.
  static Handle<Object> ChangeScriptSource(Handle<Script> original_script,
                                           Handle<String> new_source,
                                           Handle<Object> old_script_name);

  // In a code of a parent function replaces original function as embedded
  // object with a substitution one.
  static void ReplaceRefToNestedFunction(Handle<JSValue> parent_function_shared,
                                         Handle<JSValue> orig_function_shared,
                                         Handle<JSValue> subst_function_shared);

  // Checks listed functions on stack and return array with corresponding
  // FunctionPatchabilityStatus statuses; extra array element may
  // contain general error message. Modifies the current stack and
  // has restart the lowest found frames and drops all other frames above
  // if possible and if do_drop is true.
  static Handle<JSArray> CheckAndDropActivations(
      Handle<JSArray> shared_info_array, bool do_drop);

  // Restarts the call frame and completely drops all frames above it.
  // Return error message or NULL.
  static const char* RestartFrame(JavaScriptFrame* frame);

  // A copy of this is in liveedit-debugger.js.
  enum FunctionPatchabilityStatus {
    FUNCTION_AVAILABLE_FOR_PATCH = 1,
    FUNCTION_BLOCKED_ON_ACTIVE_STACK = 2,
    FUNCTION_BLOCKED_ON_OTHER_STACK = 3,
    FUNCTION_BLOCKED_UNDER_NATIVE_CODE = 4,
    FUNCTION_REPLACED_ON_ACTIVE_STACK = 5
  };

  // Compares 2 strings line-by-line, then token-wise and returns diff in form
  // of array of triplets (pos1, pos1_end, pos2_end) describing list
  // of diff chunks.
  static Handle<JSArray> CompareStrings(Handle<String> s1,
                                        Handle<String> s2);
};


// A general-purpose comparator between 2 arrays.
class Comparator {
 public:
  // Holds 2 arrays of some elements allowing to compare any pair of
  // element from the first array and element from the second array.
  class Input {
   public:
    virtual int GetLength1() = 0;
    virtual int GetLength2() = 0;
    virtual bool Equals(int index1, int index2) = 0;

   protected:
    virtual ~Input() {}
  };

  // Receives compare result as a series of chunks.
  class Output {
   public:
    // Puts another chunk in result list. Note that technically speaking
    // only 3 arguments actually needed with 4th being derivable.
    virtual void AddChunk(int pos1, int pos2, int len1, int len2) = 0;

   protected:
    virtual ~Output() {}
  };

  // Finds the difference between 2 arrays of elements.
  static void CalculateDifference(Input* input,
                                  Output* result_writer);
};



// Simple helper class that creates more or less typed structures over
// JSArray object. This is an adhoc method of passing structures from C++
// to JavaScript.
template<typename S>
class JSArrayBasedStruct {
 public:
  static S Create(Isolate* isolate) {
    Factory* factory = isolate->factory();
    Handle<JSArray> array = factory->NewJSArray(S::kSize_);
    return S(array);
  }

  static S cast(Object* object) {
    JSArray* array = JSArray::cast(object);
    Handle<JSArray> array_handle(array);
    return S(array_handle);
  }

  explicit JSArrayBasedStruct(Handle<JSArray> array) : array_(array) {
  }

  Handle<JSArray> GetJSArray() {
    return array_;
  }

  Isolate* isolate() const {
    return array_->GetIsolate();
  }

 protected:
  void SetField(int field_position, Handle<Object> value) {
    JSObject::SetElement(array_, field_position, value, NONE, SLOPPY).Assert();
  }

  void SetSmiValueField(int field_position, int value) {
    SetField(field_position, Handle<Smi>(Smi::FromInt(value), isolate()));
  }

  Handle<Object> GetField(int field_position) {
    return Object::GetElementNoExceptionThrown(
        isolate(), array_, field_position);
  }

  int GetSmiValueField(int field_position) {
    Handle<Object> res = GetField(field_position);
    return Handle<Smi>::cast(res)->value();
  }

 private:
  Handle<JSArray> array_;
};


// Represents some function compilation details. This structure will be used
// from JavaScript. It contains Code object, which is kept wrapped
// into a BlindReference for sanitizing reasons.
class FunctionInfoWrapper : public JSArrayBasedStruct<FunctionInfoWrapper> {
 public:
  explicit FunctionInfoWrapper(Handle<JSArray> array)
      : JSArrayBasedStruct<FunctionInfoWrapper>(array) {
  }

  void SetInitialProperties(Handle<String> name,
                            int start_position,
                            int end_position,
                            int param_num,
                            int literal_count,
                            int parent_index);

  void SetFunctionCode(Handle<Code> function_code,
                       Handle<HeapObject> code_scope_info);

  void SetFunctionScopeInfo(Handle<Object> scope_info_array) {
    this->SetField(kFunctionScopeInfoOffset_, scope_info_array);
  }

  void SetSharedFunctionInfo(Handle<SharedFunctionInfo> info);

  int GetLiteralCount() {
    return this->GetSmiValueField(kLiteralNumOffset_);
  }

  int GetParentIndex() {
    return this->GetSmiValueField(kParentIndexOffset_);
  }

  Handle<Code> GetFunctionCode();

  Handle<Object> GetCodeScopeInfo();

  int GetStartPosition() {
    return this->GetSmiValueField(kStartPositionOffset_);
  }

  int GetEndPosition() { return this->GetSmiValueField(kEndPositionOffset_); }

 private:
  static const int kFunctionNameOffset_ = 0;
  static const int kStartPositionOffset_ = 1;
  static const int kEndPositionOffset_ = 2;
  static const int kParamNumOffset_ = 3;
  static const int kCodeOffset_ = 4;
  static const int kCodeScopeInfoOffset_ = 5;
  static const int kFunctionScopeInfoOffset_ = 6;
  static const int kParentIndexOffset_ = 7;
  static const int kSharedFunctionInfoOffset_ = 8;
  static const int kLiteralNumOffset_ = 9;
  static const int kSize_ = 10;

  friend class JSArrayBasedStruct<FunctionInfoWrapper>;
};


// Wraps SharedFunctionInfo along with some of its fields for passing it
// back to JavaScript. SharedFunctionInfo object itself is additionally
// wrapped into BlindReference for sanitizing reasons.
class SharedInfoWrapper : public JSArrayBasedStruct<SharedInfoWrapper> {
 public:
  static bool IsInstance(Handle<JSArray> array) {
    return array->length() == Smi::FromInt(kSize_) &&
        Object::GetElementNoExceptionThrown(
            array->GetIsolate(), array, kSharedInfoOffset_)->IsJSValue();
  }

  explicit SharedInfoWrapper(Handle<JSArray> array)
      : JSArrayBasedStruct<SharedInfoWrapper>(array) {
  }

  void SetProperties(Handle<String> name,
                     int start_position,
                     int end_position,
                     Handle<SharedFunctionInfo> info);

  Handle<SharedFunctionInfo> GetInfo();

 private:
  static const int kFunctionNameOffset_ = 0;
  static const int kStartPositionOffset_ = 1;
  static const int kEndPositionOffset_ = 2;
  static const int kSharedInfoOffset_ = 3;
  static const int kSize_ = 4;

  friend class JSArrayBasedStruct<SharedInfoWrapper>;
};

#endif  // ENABLE_DEBUGGER_SUPPORT


} }  // namespace v8::internal

#endif /* V*_LIVEEDIT_H_ */

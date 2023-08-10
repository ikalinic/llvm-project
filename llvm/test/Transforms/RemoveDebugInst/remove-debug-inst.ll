; RUN: opt -S -passes=remove-debug-inst %S/Inputs/foobar.ll | FileCheck %s --dump-input=always

; CHECK-NOT:{{^}}call void @llvm.dbg.declare{{$}}
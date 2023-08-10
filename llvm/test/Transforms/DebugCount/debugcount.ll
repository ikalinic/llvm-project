; RUN: opt -disable-output -passes=debugcount %S/Inputs/foobar.ll 2>&1 | FileCheck %s

; CHECK:{{^}}Function: foo{{$}}
; CHECK:{{^}} llvm.dbg.declare: 1{{$}}
; CHECK:{{^}}Function: bar{{$}}
; CHECK:{{^}}Function: foobar{{$}}
; CHECK:{{^}} llvm.dbg.declare: 2{{$}}

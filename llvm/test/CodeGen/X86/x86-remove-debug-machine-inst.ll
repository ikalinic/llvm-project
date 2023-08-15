; RUN: llc < %s -O=2 -x86-remove-debug-inst | FileCheck %s

; CHECK-NOT: .byte	28                              # DW_AT_const_value
; CHECK-NOT: .byte  13                              # DW_FORM_sdata
; CHECK-NOT: .byte  5                               # DW_AT_const_value
; CHECK-NOT: .byte  2                               # DW_AT_const_value
; CHECK-NOT: .byte  3                               # DW_AT_const_value

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local i32 @foo() local_unnamed_addr #0 !dbg !10 {
  call void @llvm.dbg.value(metadata i32 5, metadata !16, metadata !DIExpression()), !dbg !17
  ret i32 5, !dbg !18
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local void @bar() local_unnamed_addr #0 !dbg !19 {
  ret void, !dbg !22
}

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable
define dso_local i64 @foobar() local_unnamed_addr #0 !dbg !23 {
  call void @llvm.dbg.value(metadata i64 2, metadata !28, metadata !DIExpression()), !dbg !30
  call void @llvm.dbg.value(metadata i64 3, metadata !29, metadata !DIExpression()), !dbg !30
  ret i64 9, !dbg !31
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 18.0.0 (git@github.com:ikalinic/llvm-project.git 553ff8da690082ea335c0a11c7d3d454b6576b49)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/ikalinic@syrmia.com/Projects/temp/test_2.c", directory: "/home/ikalinic@syrmia.com/Projects/github/llvm-project/build", checksumkind: CSK_MD5, checksum: "3bb211544eb821d965ec30bff57cf088")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"PIE Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{i32 7, !"debug-info-assignment-tracking", i1 true}
!9 = !{!"clang version 18.0.0 (git@github.com:ikalinic/llvm-project.git 553ff8da690082ea335c0a11c7d3d454b6576b49)"}
!10 = distinct !DISubprogram(name: "foo", scope: !11, file: !11, line: 1, type: !12, scopeLine: 1, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !15)
!11 = !DIFile(filename: "temp/test_2.c", directory: "/home/ikalinic@syrmia.com/Projects", checksumkind: CSK_MD5, checksum: "3bb211544eb821d965ec30bff57cf088")
!12 = !DISubroutineType(types: !13)
!13 = !{!14}
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !{!16}
!16 = !DILocalVariable(name: "a", scope: !10, file: !11, line: 2, type: !14)
!17 = !DILocation(line: 0, scope: !10)
!18 = !DILocation(line: 3, column: 3, scope: !10)
!19 = distinct !DISubprogram(name: "bar", scope: !11, file: !11, line: 6, type: !20, scopeLine: 6, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!20 = !DISubroutineType(types: !21)
!21 = !{null}
!22 = !DILocation(line: 7, column: 1, scope: !19)
!23 = distinct !DISubprogram(name: "foobar", scope: !11, file: !11, line: 9, type: !24, scopeLine: 9, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !27)
!24 = !DISubroutineType(types: !25)
!25 = !{!26}
!26 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!27 = !{!28, !29}
!28 = !DILocalVariable(name: "a", scope: !23, file: !11, line: 10, type: !26)
!29 = !DILocalVariable(name: "b", scope: !23, file: !11, line: 11, type: !26)
!30 = !DILocation(line: 0, scope: !23)
!31 = !DILocation(line: 18, column: 1, scope: !23)

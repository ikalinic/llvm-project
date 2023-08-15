; RUN: llc < %s -x86-remove-debug-inst < %s | FileCheck %s
; REQUIRED: tagret=x86_64{{.*}}

; CHECK-NOT: {{^}}#DEBUG_VALUE:{{.*&}}

define dso_local i32 @foo() #0 !dbg !10 {
  %1 = alloca i32, align 4
  call void @llvm.dbg.declare(metadata ptr %1, metadata !16, metadata !DIExpression()), !dbg !17
  store i32 5, ptr %1, align 4, !dbg !17
  %2 = load i32, ptr %1, align 4, !dbg !18
  ret i32 %2, !dbg !19
}

declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

define dso_local void @bar() #0 !dbg !20 {
  ret void, !dbg !23
}

define dso_local i64 @foobar() #0 !dbg !24 {
  %1 = alloca i64, align 8
  %2 = alloca i64, align 8
  %3 = alloca i64, align 8
  call void @llvm.dbg.declare(metadata ptr %2, metadata !28, metadata !DIExpression()), !dbg !29
  store i64 2, ptr %2, align 8, !dbg !29
  call void @llvm.dbg.declare(metadata ptr %3, metadata !30, metadata !DIExpression()), !dbg !31
  store i64 3, ptr %3, align 8, !dbg !31
  %4 = load i64, ptr %3, align 8, !dbg !32
  %5 = load i64, ptr %2, align 8, !dbg !34
  %6 = icmp sgt i64 %4, %5, !dbg !35
  br i1 %6, label %7, label %8, !dbg !36

7:                                                ; preds = %0
  store i64 9, ptr %1, align 8, !dbg !37
  br label %9, !dbg !37

8:                                                ; preds = %0
  store i64 10, ptr %1, align 8, !dbg !39
  br label %9, !dbg !39

9:                                                ; preds = %8, %7
  %10 = load i64, ptr %1, align 8, !dbg !41
  ret i64 %10, !dbg !41
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C11, file: !1, producer: "clang version 18.0.0 (git@github.com:ikalinic/llvm-project.git c4769ef59c07bcb8c1ab0d9c0fe6e9ef319ffa6a)", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "/home/ikalinic@syrmia.com/Projects/temp/test_2.c", directory: "/home/ikalinic@syrmia.com/Projects/github/llvm-project/build", checksumkind: CSK_MD5, checksum: "3bb211544eb821d965ec30bff57cf088")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!4 = !{i32 1, !"wchar_size", i32 4}
!5 = !{i32 8, !"PIC Level", i32 2}
!6 = !{i32 7, !"PIE Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 2}
!8 = !{i32 7, !"frame-pointer", i32 2}
!9 = !{!"clang version 18.0.0 (git@github.com:ikalinic/llvm-project.git c4769ef59c07bcb8c1ab0d9c0fe6e9ef319ffa6a)"}
!10 = distinct !DISubprogram(name: "foo", scope: !11, file: !11, line: 1, type: !12, scopeLine: 1, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !15)
!11 = !DIFile(filename: "temp/test_2.c", directory: "/home/ikalinic@syrmia.com/Projects", checksumkind: CSK_MD5, checksum: "3bb211544eb821d965ec30bff57cf088")
!12 = !DISubroutineType(types: !13)
!13 = !{!14}
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !{}
!16 = !DILocalVariable(name: "a", scope: !10, file: !11, line: 2, type: !14)
!17 = !DILocation(line: 2, column: 7, scope: !10)
!18 = !DILocation(line: 3, column: 10, scope: !10)
!19 = !DILocation(line: 3, column: 3, scope: !10)
!20 = distinct !DISubprogram(name: "bar", scope: !11, file: !11, line: 6, type: !21, scopeLine: 6, spFlags: DISPFlagDefinition, unit: !0)
!21 = !DISubroutineType(types: !22)
!22 = !{null}
!23 = !DILocation(line: 7, column: 1, scope: !20)
!24 = distinct !DISubprogram(name: "foobar", scope: !11, file: !11, line: 9, type: !25, scopeLine: 9, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !15)
!25 = !DISubroutineType(types: !26)
!26 = !{!27}
!27 = !DIBasicType(name: "long", size: 64, encoding: DW_ATE_signed)
!28 = !DILocalVariable(name: "a", scope: !24, file: !11, line: 10, type: !27)
!29 = !DILocation(line: 10, column: 10, scope: !24)
!30 = !DILocalVariable(name: "b", scope: !24, file: !11, line: 11, type: !27)
!31 = !DILocation(line: 11, column: 10, scope: !24)
!32 = !DILocation(line: 13, column: 9, scope: !33)
!33 = distinct !DILexicalBlock(scope: !24, file: !11, line: 13, column: 9)
!34 = !DILocation(line: 13, column: 13, scope: !33)
!35 = !DILocation(line: 13, column: 11, scope: !33)
!36 = !DILocation(line: 13, column: 9, scope: !24)
!37 = !DILocation(line: 14, column: 9, scope: !38)
!38 = distinct !DILexicalBlock(scope: !33, file: !11, line: 13, column: 16)
!39 = !DILocation(line: 16, column: 9, scope: !40)
!40 = distinct !DILexicalBlock(scope: !33, file: !11, line: 15, column: 12)
!41 = !DILocation(line: 18, column: 1, scope: !24)


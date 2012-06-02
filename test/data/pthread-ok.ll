; ModuleID = 'examples/llvm/pthread.c'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.__Tid = type { i8*, i32 }

@PTHREAD_MUTEX_NORMAL = constant i32 0, align 4
@PTHREAD_MUTEX_RECURSIVE = constant i32 33554432, align 4
@PTHREAD_MUTEX_RECURSIVE_NP = constant i32 33554432, align 4
@.str = private unnamed_addr constant [23 x i8] c"thread started with %d\00", align 1
@.str1 = private unnamed_addr constant [19 x i8] c"starting thread...\00", align 1
@.str2 = private unnamed_addr constant [27 x i8] c"thread finished with %d...\00", align 1
@__tid = internal global [8 x %struct.__Tid] zeroinitializer, align 16
@.str3 = private unnamed_addr constant [33 x i8] c"FATAL: Thread capacity exceeded.\00", align 1

define i8* @start(i8* nocapture %x) nounwind uwtable {
  tail call void @llvm.dbg.value(metadata !{i8* %x}, i64 0, metadata !12), !dbg !80
  %1 = bitcast i8* %x to i32*, !dbg !81
  tail call void @llvm.dbg.value(metadata !{i32* %1}, i64 0, metadata !13), !dbg !81
  %2 = load i32* %1, align 4, !dbg !82, !tbaa !83
  tail call void (i8*, ...)* @__divine_builtin_trace(i8* getelementptr inbounds ([23 x i8]* @.str, i64 0, i64 0), i32 %2) nounwind, !dbg !82
  %3 = load i32* %1, align 4, !dbg !86, !tbaa !83
  %4 = sext i32 %3 to i64, !dbg !86
  %5 = inttoptr i64 %4 to i8*, !dbg !86
  ret i8* %5, !dbg !86
}

declare void @llvm.dbg.declare(metadata, metadata) nounwind readnone

declare void @__divine_builtin_trace(i8*, ...)

define i32 @main() nounwind uwtable {
  %id = alloca i32, align 4
  %x = alloca i32, align 4
  call void @llvm.dbg.declare(metadata !{i32* %id}, metadata !22), !dbg !87
  call void @llvm.dbg.declare(metadata !{i32* %x}, metadata !25), !dbg !88
  call void @llvm.dbg.value(metadata !89, i64 0, metadata !25), !dbg !90
  call void @llvm.dbg.value(metadata !89, i64 0, metadata !25), !dbg !90
  call void @llvm.dbg.value(metadata !89, i64 0, metadata !25), !dbg !90
  call void @llvm.dbg.value(metadata !89, i64 0, metadata !25), !dbg !90
  call void @llvm.dbg.value(metadata !89, i64 0, metadata !25), !dbg !90
  store i32 5, i32* %x, align 4, !dbg !90, !tbaa !83
  call void (i8*, ...)* @__divine_builtin_trace(i8* getelementptr inbounds ([19 x i8]* @.str1, i64 0, i64 0)) nounwind, !dbg !91
  %1 = bitcast i32* %x to i8*, !dbg !92
  call fastcc void @pthread_create(i32* %id, i8* %1), !dbg !92
  call void @llvm.dbg.value(metadata !{i32* %id}, i64 0, metadata !22), !dbg !93
  call void @llvm.dbg.value(metadata !{i32* %id}, i64 0, metadata !22), !dbg !93
  call void @llvm.dbg.value(metadata !{i32* %id}, i64 0, metadata !22), !dbg !93
  call void @llvm.dbg.value(metadata !{i32* %id}, i64 0, metadata !22), !dbg !93
  call void @llvm.dbg.value(metadata !{i32* %id}, i64 0, metadata !22), !dbg !93
  %2 = load i32* %id, align 4, !dbg !93, !tbaa !83
  %3 = sext i32 %2 to i64, !dbg !94
  %4 = getelementptr inbounds [8 x %struct.__Tid]* @__tid, i64 0, i64 %3, i32 1, !dbg !94
  %5 = call i32 @__divine_builtin_mutex_lock(i32* %4, i32 1) nounwind, !dbg !94
  %6 = getelementptr inbounds [8 x %struct.__Tid]* @__tid, i64 0, i64 %3, i32 0, !dbg !96
  %7 = load i8** %6, align 16, !dbg !96, !tbaa !97
  call void @llvm.dbg.value(metadata !{i8* %7}, i64 0, metadata !26), !dbg !96
  call void @llvm.dbg.value(metadata !{i8* %7}, i64 0, metadata !26), !dbg !98
  call void @llvm.dbg.value(metadata !{i8* %7}, i64 0, metadata !26), !dbg !99
  call void @__divine_builtin_mutex_unlock(i32* %4) nounwind, !dbg !100
  call void (i8*, ...)* @__divine_builtin_trace(i8* getelementptr inbounds ([27 x i8]* @.str2, i64 0, i64 0), i8* %7) nounwind, !dbg !98
  %8 = ptrtoint i8* %7 to i64, !dbg !99
  %9 = trunc i64 %8 to i32, !dbg !99
  call void @llvm.dbg.value(metadata !{i32* %x}, i64 0, metadata !25), !dbg !99
  call void @llvm.dbg.value(metadata !{i32* %x}, i64 0, metadata !25), !dbg !99
  call void @llvm.dbg.value(metadata !{i32* %x}, i64 0, metadata !25), !dbg !99
  call void @llvm.dbg.value(metadata !{i32* %x}, i64 0, metadata !25), !dbg !99
  call void @llvm.dbg.value(metadata !{i32* %x}, i64 0, metadata !25), !dbg !99
  %10 = load i32* %x, align 4, !dbg !99, !tbaa !83
  %11 = icmp eq i32 %9, %10, !dbg !99
  %12 = zext i1 %11 to i32, !dbg !99
  call void @__divine_builtin_assert(i32 %12) nounwind, !dbg !99
  ret i32 0, !dbg !101
}

define internal fastcc void @pthread_create(i32* %ptid, i8* %arg) nounwind uwtable noinline {
  %init = alloca i32, align 4
  call void @llvm.dbg.value(metadata !102, i64 0, metadata !39), !dbg !103
  call void @llvm.dbg.value(metadata !104, i64 0, metadata !43), !dbg !105
  call void @llvm.dbg.declare(metadata !{i32* %init}, metadata !48), !dbg !106
  call void @llvm.dbg.value(metadata !2, i64 0, metadata !48), !dbg !107
  call void @llvm.dbg.value(metadata !2, i64 0, metadata !48), !dbg !107
  call void @llvm.dbg.value(metadata !2, i64 0, metadata !48), !dbg !107
  call void @llvm.dbg.value(metadata !2, i64 0, metadata !48), !dbg !107
  call void @llvm.dbg.value(metadata !2, i64 0, metadata !48), !dbg !107
  store volatile i32 0, i32* %init, align 4, !dbg !107, !tbaa !83
  call void @__divine_builtin_thread_create(i32* %ptid, void (i8* (i8*)*, i32*, i8*, i32*)* @__pthread_entry, i8* (i8*)* @start, i32* %ptid, i8* %arg, i32* %init) nounwind, !dbg !108
  br label %1, !dbg !109

; <label>:1                                       ; preds = %1, %0
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !109
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !109
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !109
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !109
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !109
  %2 = load volatile i32* %init, align 4, !dbg !109, !tbaa !83
  %3 = icmp eq i32 %2, 0, !dbg !109
  br i1 %3, label %1, label %4, !dbg !109

; <label>:4                                       ; preds = %1
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !110
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !110
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !110
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !110
  call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !48), !dbg !110
  %5 = load volatile i32* %init, align 4, !dbg !110, !tbaa !83
  ret void
}

declare void @__divine_builtin_assert(i32)

declare i32 @__divine_builtin_mutex_lock(i32*, i32)

declare void @__divine_builtin_mutex_unlock(i32*)

declare void @__divine_builtin_thread_create(i32*, void (i8* (i8*)*, i32*, i8*, i32*)*, i8* (i8*)*, i32*, i8*, i32*)

define internal void @__pthread_entry(i8* (i8*)* nocapture %entry, i32* nocapture %tid, i8* %arg, i32* nocapture %init) noreturn nounwind uwtable {
  tail call void @llvm.dbg.value(metadata !{i8* (i8*)* %entry}, i64 0, metadata !56), !dbg !111
  tail call void @llvm.dbg.value(metadata !{i32* %tid}, i64 0, metadata !57), !dbg !112
  tail call void @llvm.dbg.value(metadata !{i8* %arg}, i64 0, metadata !58), !dbg !113
  tail call void @llvm.dbg.value(metadata !{i32* %init}, i64 0, metadata !59), !dbg !114
  %1 = load i32* %tid, align 4, !dbg !115, !tbaa !83
  tail call void @llvm.dbg.value(metadata !{i32 %1}, i64 0, metadata !61), !dbg !115
  %2 = icmp eq i32 %1, 8, !dbg !116
  br i1 %2, label %3, label %4, !dbg !116

; <label>:3                                       ; preds = %0
  tail call void (i8*, ...)* @__divine_builtin_trace(i8* getelementptr inbounds ([33 x i8]* @.str3, i64 0, i64 0)) nounwind, !dbg !117
  store volatile i32 2, i32* %init, align 4, !dbg !119, !tbaa !83
  tail call void (...)* @__divine_builtin_thread_stop() noreturn nounwind, !dbg !120
  unreachable, !dbg !120

; <label>:4                                       ; preds = %0
  %5 = sext i32 %1 to i64, !dbg !121
  %6 = getelementptr inbounds [8 x %struct.__Tid]* @__tid, i64 0, i64 %5, i32 1, !dbg !121
  %7 = tail call i32 @__divine_builtin_mutex_lock(i32* %6, i32 1) nounwind, !dbg !121
  store volatile i32 1, i32* %init, align 4, !dbg !122, !tbaa !83
  %8 = tail call i8* %entry(i8* %arg) nounwind, !dbg !123
  %9 = getelementptr inbounds [8 x %struct.__Tid]* @__tid, i64 0, i64 %5, i32 0, !dbg !123
  store i8* %8, i8** %9, align 16, !dbg !123, !tbaa !97
  tail call void @__divine_builtin_mutex_unlock(i32* %6) nounwind, !dbg !124
  tail call void (...)* @__divine_builtin_thread_stop() noreturn nounwind, !dbg !125
  unreachable, !dbg !125
}

declare void @__divine_builtin_thread_stop(...) noreturn

declare void @llvm.dbg.value(metadata, i64, metadata) nounwind readnone

!llvm.dbg.cu = !{!0}

!0 = metadata !{i32 720913, i32 0, i32 12, metadata !"examples/llvm/pthread.c", metadata !"/home/mornfall/dev/divine/mainline", metadata !"clang version 3.0 (tags/RELEASE_30/final)", i1 true, i1 true, metadata !"", i32 0, metadata !1, metadata !1, metadata !3, metadata !63} ; [ DW_TAG_compile_unit ]
!1 = metadata !{metadata !2}
!2 = metadata !{i32 0}
!3 = metadata !{metadata !4}
!4 = metadata !{metadata !5, metadata !17, metadata !27, metadata !34, metadata !51}
!5 = metadata !{i32 720942, i32 0, metadata !6, metadata !"start", metadata !"start", metadata !"", metadata !6, i32 3, metadata !7, i1 false, i1 true, i32 0, i32 0, i32 0, i32 256, i1 true, i8* (i8*)* @start, null, null, metadata !10} ; [ DW_TAG_subprogram ]
!6 = metadata !{i32 720937, metadata !"examples/llvm/pthread.c", metadata !"/home/mornfall/dev/divine/mainline", null} ; [ DW_TAG_file_type ]
!7 = metadata !{i32 720917, i32 0, metadata !"", i32 0, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !8, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!8 = metadata !{metadata !9}
!9 = metadata !{i32 720911, null, metadata !"", null, i32 0, i64 64, i64 64, i64 0, i32 0, null} ; [ DW_TAG_pointer_type ]
!10 = metadata !{metadata !11}
!11 = metadata !{metadata !12, metadata !13}
!12 = metadata !{i32 721153, metadata !5, metadata !"x", metadata !6, i32 16777219, metadata !9, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!13 = metadata !{i32 721152, metadata !14, metadata !"xx", metadata !6, i32 4, metadata !15, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!14 = metadata !{i32 720907, metadata !5, i32 3, i32 22, metadata !6, i32 0} ; [ DW_TAG_lexical_block ]
!15 = metadata !{i32 720911, null, metadata !"", null, i32 0, i64 64, i64 64, i64 0, i32 0, metadata !16} ; [ DW_TAG_pointer_type ]
!16 = metadata !{i32 720932, null, metadata !"int", null, i32 0, i64 32, i64 32, i64 0, i32 0, i32 5} ; [ DW_TAG_base_type ]
!17 = metadata !{i32 720942, i32 0, metadata !6, metadata !"main", metadata !"main", metadata !"", metadata !6, i32 13, metadata !18, i1 false, i1 true, i32 0, i32 0, i32 0, i32 0, i1 true, i32 ()* @main, null, null, metadata !20} ; [ DW_TAG_subprogram ]
!18 = metadata !{i32 720917, i32 0, metadata !"", i32 0, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !19, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!19 = metadata !{metadata !16}
!20 = metadata !{metadata !21}
!21 = metadata !{metadata !22, metadata !25, metadata !26}
!22 = metadata !{i32 721152, metadata !23, metadata !"id", metadata !6, i32 14, metadata !24, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!23 = metadata !{i32 720907, metadata !17, i32 13, i32 12, metadata !6, i32 1} ; [ DW_TAG_lexical_block ]
!24 = metadata !{i32 720918, null, metadata !"pthread_t", metadata !6, i32 47, i64 0, i64 0, i64 0, i32 0, metadata !16} ; [ DW_TAG_typedef ]
!25 = metadata !{i32 721152, metadata !23, metadata !"x", metadata !6, i32 15, metadata !16, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!26 = metadata !{i32 721152, metadata !23, metadata !"result", metadata !6, i32 16, metadata !9, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!27 = metadata !{i32 720942, i32 0, metadata !28, metadata !"pthread_join", metadata !"pthread_join", metadata !"", metadata !28, i32 105, metadata !18, i1 true, i1 true, i32 0, i32 0, i32 0, i32 256, i1 true, null, null, null, metadata !29} ; [ DW_TAG_subprogram ]
!28 = metadata !{i32 720937, metadata !"examples/llvm/divine-llvm.h", metadata !"/home/mornfall/dev/divine/mainline", null} ; [ DW_TAG_file_type ]
!29 = metadata !{metadata !30}
!30 = metadata !{metadata !31, metadata !32}
!31 = metadata !{i32 721153, metadata !27, metadata !"ptid", metadata !28, i32 16777321, metadata !24, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!32 = metadata !{i32 721153, metadata !27, metadata !"x", metadata !28, i32 33554537, metadata !33, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!33 = metadata !{i32 720911, null, metadata !"", null, i32 0, i64 64, i64 64, i64 0, i32 0, metadata !9} ; [ DW_TAG_pointer_type ]
!34 = metadata !{i32 720942, i32 0, metadata !28, metadata !"pthread_create", metadata !"pthread_create", metadata !"", metadata !28, i32 85, metadata !18, i1 true, i1 true, i32 0, i32 0, i32 0, i32 256, i1 true, null, null, null, metadata !35} ; [ DW_TAG_subprogram ]
!35 = metadata !{metadata !36}
!36 = metadata !{metadata !37, metadata !39, metadata !43, metadata !47, metadata !48}
!37 = metadata !{i32 721153, metadata !34, metadata !"ptid", metadata !28, i32 16777298, metadata !38, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!38 = metadata !{i32 720911, null, metadata !"", null, i32 0, i64 64, i64 64, i64 0, i32 0, metadata !24} ; [ DW_TAG_pointer_type ]
!39 = metadata !{i32 721153, metadata !34, metadata !"attr", metadata !28, i32 33554515, metadata !40, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!40 = metadata !{i32 720911, null, metadata !"", null, i32 0, i64 64, i64 64, i64 0, i32 0, metadata !41} ; [ DW_TAG_pointer_type ]
!41 = metadata !{i32 720934, null, metadata !"", null, i32 0, i64 0, i64 0, i64 0, i32 0, metadata !42} ; [ DW_TAG_const_type ]
!42 = metadata !{i32 720918, null, metadata !"pthread_attr_t", metadata !28, i32 48, i64 0, i64 0, i64 0, i32 0, metadata !16} ; [ DW_TAG_typedef ]
!43 = metadata !{i32 721153, metadata !34, metadata !"entry", metadata !28, i32 50331732, metadata !44, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!44 = metadata !{i32 720911, null, metadata !"", null, i32 0, i64 64, i64 64, i64 0, i32 0, metadata !45} ; [ DW_TAG_pointer_type ]
!45 = metadata !{i32 720917, i32 0, metadata !"", i32 0, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !46, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!46 = metadata !{metadata !9, metadata !9}
!47 = metadata !{i32 721153, metadata !34, metadata !"arg", metadata !28, i32 67108948, metadata !9, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!48 = metadata !{i32 721152, metadata !49, metadata !"init", metadata !28, i32 86, metadata !50, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!49 = metadata !{i32 720907, metadata !34, i32 85, i32 1, metadata !28, i32 3} ; [ DW_TAG_lexical_block ]
!50 = metadata !{i32 720949, null, metadata !"", null, i32 0, i64 0, i64 0, i64 0, i32 0, metadata !16} ; [ DW_TAG_volatile_type ]
!51 = metadata !{i32 720942, i32 0, metadata !28, metadata !"__pthread_entry", metadata !"__pthread_entry", metadata !"", metadata !28, i32 65, metadata !52, i1 true, i1 true, i32 0, i32 0, i32 0, i32 256, i1 true, void (i8* (i8*)*, i32*, i8*, i32*)* @__pthread_entry, null, null, metadata !54} ; [ DW_TAG_subprogram ]
!52 = metadata !{i32 720917, i32 0, metadata !"", i32 0, i32 0, i64 0, i64 0, i32 0, i32 0, i32 0, metadata !53, i32 0, i32 0} ; [ DW_TAG_subroutine_type ]
!53 = metadata !{null}
!54 = metadata !{metadata !55}
!55 = metadata !{metadata !56, metadata !57, metadata !58, metadata !59, metadata !61}
!56 = metadata !{i32 721153, metadata !51, metadata !"entry", metadata !28, i32 16777280, metadata !44, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!57 = metadata !{i32 721153, metadata !51, metadata !"tid", metadata !28, i32 33554496, metadata !38, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!58 = metadata !{i32 721153, metadata !51, metadata !"arg", metadata !28, i32 50331712, metadata !9, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!59 = metadata !{i32 721153, metadata !51, metadata !"init", metadata !28, i32 67108928, metadata !60, i32 0, i32 0} ; [ DW_TAG_arg_variable ]
!60 = metadata !{i32 720911, null, metadata !"", null, i32 0, i64 64, i64 64, i64 0, i32 0, metadata !50} ; [ DW_TAG_pointer_type ]
!61 = metadata !{i32 721152, metadata !62, metadata !"id", metadata !28, i32 66, metadata !16, i32 0, i32 0} ; [ DW_TAG_auto_variable ]
!62 = metadata !{i32 720907, metadata !51, i32 65, i32 1, metadata !28, i32 4} ; [ DW_TAG_lexical_block ]
!63 = metadata !{metadata !64}
!64 = metadata !{metadata !65, metadata !67, metadata !68, metadata !69, metadata !79}
!65 = metadata !{i32 720948, i32 0, null, metadata !"PTHREAD_MUTEX_NORMAL", metadata !"PTHREAD_MUTEX_NORMAL", metadata !"", metadata !28, i32 101, metadata !66, i32 0, i32 1, i32* @PTHREAD_MUTEX_NORMAL} ; [ DW_TAG_variable ]
!66 = metadata !{i32 720934, null, metadata !"", null, i32 0, i64 0, i64 0, i64 0, i32 0, metadata !16} ; [ DW_TAG_const_type ]
!67 = metadata !{i32 720948, i32 0, null, metadata !"PTHREAD_MUTEX_RECURSIVE", metadata !"PTHREAD_MUTEX_RECURSIVE", metadata !"", metadata !28, i32 102, metadata !66, i32 0, i32 1, i32* @PTHREAD_MUTEX_RECURSIVE} ; [ DW_TAG_variable ]
!68 = metadata !{i32 720948, i32 0, null, metadata !"PTHREAD_MUTEX_RECURSIVE_NP", metadata !"PTHREAD_MUTEX_RECURSIVE_NP", metadata !"", metadata !28, i32 103, metadata !66, i32 0, i32 1, i32* @PTHREAD_MUTEX_RECURSIVE_NP} ; [ DW_TAG_variable ]
!69 = metadata !{i32 720948, i32 0, null, metadata !"__tid", metadata !"__tid", metadata !"", metadata !28, i32 61, metadata !70, i32 1, i32 1, [8 x %struct.__Tid]* @__tid} ; [ DW_TAG_variable ]
!70 = metadata !{i32 720897, null, metadata !"", null, i32 0, i64 1024, i64 64, i32 0, i32 0, metadata !71, metadata !77, i32 0, i32 0} ; [ DW_TAG_array_type ]
!71 = metadata !{i32 720918, null, metadata !"__Tid", metadata !28, i32 58, i64 0, i64 0, i64 0, i32 0, metadata !72} ; [ DW_TAG_typedef ]
!72 = metadata !{i32 720915, null, metadata !"", metadata !28, i32 55, i64 128, i64 64, i32 0, i32 0, i32 0, metadata !73, i32 0, i32 0} ; [ DW_TAG_structure_type ]
!73 = metadata !{metadata !74, metadata !75}
!74 = metadata !{i32 720909, metadata !72, metadata !"result", metadata !28, i32 56, i64 64, i64 64, i64 0, i32 0, metadata !9} ; [ DW_TAG_member ]
!75 = metadata !{i32 720909, metadata !72, metadata !"mutex", metadata !28, i32 57, i64 32, i64 32, i64 64, i32 0, metadata !76} ; [ DW_TAG_member ]
!76 = metadata !{i32 720918, null, metadata !"pthread_mutex_t", metadata !28, i32 50, i64 0, i64 0, i64 0, i32 0, metadata !16} ; [ DW_TAG_typedef ]
!77 = metadata !{metadata !78}
!78 = metadata !{i32 720929, i64 0, i64 7}        ; [ DW_TAG_subrange_type ]
!79 = metadata !{i32 720948, i32 0, metadata !28, metadata !"MAXTHREAD", metadata !"MAXTHREAD", metadata !"MAXTHREAD", metadata !28, i32 60, metadata !66, i32 1, i32 1, i32 8} ; [ DW_TAG_variable ]
!80 = metadata !{i32 3, i32 19, metadata !5, null}
!81 = metadata !{i32 4, i32 16, metadata !14, null}
!82 = metadata !{i32 5, i32 5, metadata !14, null}
!83 = metadata !{metadata !"int", metadata !84}
!84 = metadata !{metadata !"omnipotent char", metadata !85}
!85 = metadata !{metadata !"Simple C/C++ TBAA", null}
!86 = metadata !{i32 9, i32 5, metadata !14, null}
!87 = metadata !{i32 14, i32 15, metadata !23, null}
!88 = metadata !{i32 15, i32 9, metadata !23, null}
!89 = metadata !{i32 5}
!90 = metadata !{i32 15, i32 14, metadata !23, null}
!91 = metadata !{i32 17, i32 5, metadata !23, null}
!92 = metadata !{i32 18, i32 5, metadata !23, null}
!93 = metadata !{i32 19, i32 5, metadata !23, null}
!94 = metadata !{i32 106, i32 5, metadata !95, metadata !93}
!95 = metadata !{i32 720907, metadata !27, i32 105, i32 51, metadata !28, i32 2} ; [ DW_TAG_lexical_block ]
!96 = metadata !{i32 107, i32 5, metadata !95, metadata !93}
!97 = metadata !{metadata !"any pointer", metadata !84}
!98 = metadata !{i32 20, i32 5, metadata !23, null}
!99 = metadata !{i32 21, i32 5, metadata !23, null}
!100 = metadata !{i32 108, i32 5, metadata !95, metadata !93}
!101 = metadata !{i32 22, i32 5, metadata !23, null}
!102 = metadata !{i32* null}
!103 = metadata !{i32 83, i32 49, metadata !34, null}
!104 = metadata !{i8* (i8*)* @start}
!105 = metadata !{i32 84, i32 35, metadata !34, null}
!106 = metadata !{i32 86, i32 18, metadata !49, null}
!107 = metadata !{i32 86, i32 26, metadata !49, null}
!108 = metadata !{i32 87, i32 5, metadata !49, null}
!109 = metadata !{i32 89, i32 5, metadata !49, null}
!110 = metadata !{i32 90, i32 5, metadata !49, null}
!111 = metadata !{i32 64, i32 37, metadata !51, null}
!112 = metadata !{i32 64, i32 64, metadata !51, null}
!113 = metadata !{i32 64, i32 75, metadata !51, null}
!114 = metadata !{i32 64, i32 94, metadata !51, null}
!115 = metadata !{i32 66, i32 18, metadata !62, null}
!116 = metadata !{i32 67, i32 5, metadata !62, null}
!117 = metadata !{i32 68, i32 9, metadata !118, null}
!118 = metadata !{i32 720907, metadata !62, i32 67, i32 28, metadata !28, i32 5} ; [ DW_TAG_lexical_block ]
!119 = metadata !{i32 69, i32 9, metadata !118, null}
!120 = metadata !{i32 70, i32 9, metadata !118, null}
!121 = metadata !{i32 74, i32 5, metadata !62, null}
!122 = metadata !{i32 75, i32 5, metadata !62, null}
!123 = metadata !{i32 76, i32 24, metadata !62, null}
!124 = metadata !{i32 78, i32 5, metadata !62, null}
!125 = metadata !{i32 79, i32 5, metadata !62, null}

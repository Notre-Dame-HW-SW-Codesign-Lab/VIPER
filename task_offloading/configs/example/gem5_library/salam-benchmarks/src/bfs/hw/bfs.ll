; ModuleID = 'bfs.c'
source_filename = "bfs.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

%struct.node_t_struct = type { i32, i32 }
%struct.edge_t_struct = type { i32 }

; Function Attrs: nofree norecurse nounwind
define dso_local void @bfs(i32 noundef %0) local_unnamed_addr #0 {
  %2 = getelementptr inbounds i8, ptr inttoptr (i32 788547904 to ptr), i32 %0
  store volatile i8 0, ptr %2, align 1, !tbaa !4
  store volatile i32 1, ptr inttoptr (i32 788548224 to ptr), align 128, !tbaa !7
  br label %3

3:                                                ; preds = %38, %1
  %4 = phi i32 [ 0, %1 ], [ %5, %38 ]
  %5 = add nuw nsw i32 %4, 1
  %6 = trunc i32 %5 to i8
  br label %7

7:                                                ; preds = %3, %34
  %8 = phi i32 [ 0, %3 ], [ %36, %34 ]
  %9 = phi i32 [ 0, %3 ], [ %35, %34 ]
  %10 = getelementptr inbounds i8, ptr inttoptr (i32 788547904 to ptr), i32 %8
  %11 = load volatile i8, ptr %10, align 1, !tbaa !4
  %12 = zext i8 %11 to i32
  %13 = icmp eq i32 %4, %12
  br i1 %13, label %14, label %34

14:                                               ; preds = %7
  %15 = getelementptr inbounds %struct.node_t_struct, ptr inttoptr (i32 788529344 to ptr), i32 %8
  %16 = load volatile i32, ptr %15, align 8, !tbaa !9
  %17 = getelementptr inbounds %struct.node_t_struct, ptr inttoptr (i32 788529344 to ptr), i32 %8, i32 1
  %18 = load volatile i32, ptr %17, align 4, !tbaa !11
  %19 = icmp ult i32 %16, %18
  br i1 %19, label %20, label %34

20:                                               ; preds = %14, %30
  %21 = phi i32 [ %32, %30 ], [ %16, %14 ]
  %22 = phi i32 [ %31, %30 ], [ %9, %14 ]
  %23 = getelementptr inbounds %struct.edge_t_struct, ptr inttoptr (i32 788531456 to ptr), i32 %21
  %24 = load volatile i32, ptr %23, align 4, !tbaa !12
  %25 = getelementptr inbounds i8, ptr inttoptr (i32 788547904 to ptr), i32 %24
  %26 = load volatile i8, ptr %25, align 1, !tbaa !4
  %27 = icmp eq i8 %26, 127
  br i1 %27, label %28, label %30

28:                                               ; preds = %20
  store volatile i8 %6, ptr %25, align 1, !tbaa !4
  %29 = add i32 %22, 1
  br label %30

30:                                               ; preds = %28, %20
  %31 = phi i32 [ %29, %28 ], [ %22, %20 ]
  %32 = add nuw i32 %21, 1
  %33 = icmp eq i32 %32, %18
  br i1 %33, label %34, label %20, !llvm.loop !14

34:                                               ; preds = %30, %14, %7
  %35 = phi i32 [ %9, %7 ], [ %9, %14 ], [ %31, %30 ]
  %36 = add nuw nsw i32 %8, 1
  %37 = icmp eq i32 %36, 256
  br i1 %37, label %38, label %7, !llvm.loop !17

38:                                               ; preds = %34
  %39 = getelementptr i32, ptr inttoptr (i32 788548228 to ptr), i32 %4
  store volatile i32 %35, ptr %39, align 4, !tbaa !7
  %40 = icmp eq i32 %35, 0
  %41 = icmp eq i32 %5, 10
  %42 = select i1 %40, i1 true, i1 %41
  br i1 %42, label %43, label %3, !llvm.loop !18

43:                                               ; preds = %38
  ret void
}

attributes #0 = { nofree norecurse nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+armv7-a,+dsp,+soft-float,+strict-align,-aes,-bf16,-d32,-dotprod,-fp-armv8,-fp-armv8d16,-fp-armv8d16sp,-fp-armv8sp,-fp16,-fp16fml,-fp64,-fpregs,-fullfp16,-mve,-mve.fp,-neon,-sha2,-thumb-mode,-vfp2,-vfp2sp,-vfp3,-vfp3d16,-vfp3d16sp,-vfp3sp,-vfp4,-vfp4d16,-vfp4d16sp,-vfp4sp" "use-soft-float"="true" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"min_enum_size", i32 4}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"Ubuntu clang version 18.1.3 (1ubuntu1)"}
!4 = !{!5, !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
!7 = !{!8, !8, i64 0}
!8 = !{!"int", !5, i64 0}
!9 = !{!10, !8, i64 0}
!10 = !{!"node_t_struct", !8, i64 0, !8, i64 4}
!11 = !{!10, !8, i64 4}
!12 = !{!13, !8, i64 0}
!13 = !{!"edge_t_struct", !8, i64 0}
!14 = distinct !{!14, !15, !16}
!15 = !{!"llvm.loop.mustprogress"}
!16 = !{!"llvm.loop.unroll.disable"}
!17 = distinct !{!17, !15, !16}
!18 = distinct !{!18, !15, !16}

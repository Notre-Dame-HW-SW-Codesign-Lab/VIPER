; ModuleID = 'top.c'
source_filename = "top.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

; Function Attrs: nofree norecurse nounwind
define dso_local void @top(i64 noundef %0, i64 noundef %1, i64 noundef %2, i64 noundef %3, i32 noundef %4) local_unnamed_addr #0 {
  store volatile i64 %0, ptr inttoptr (i32 788529153 to ptr), align 8, !tbaa !4
  store volatile i64 788529344, ptr inttoptr (i32 788529161 to ptr), align 8, !tbaa !4
  store volatile i32 2048, ptr inttoptr (i32 788529169 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  br label %6

6:                                                ; preds = %6, %5
  %7 = load volatile i8, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  %8 = and i8 %7, 4
  %9 = icmp eq i8 %8, 0
  br i1 %9, label %6, label %10, !llvm.loop !11

10:                                               ; preds = %6
  store volatile i64 %1, ptr inttoptr (i32 788529153 to ptr), align 8, !tbaa !4
  store volatile i64 788531456, ptr inttoptr (i32 788529161 to ptr), align 8, !tbaa !4
  store volatile i32 16384, ptr inttoptr (i32 788529169 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  br label %11

11:                                               ; preds = %11, %10
  %12 = load volatile i8, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  %13 = and i8 %12, 4
  %14 = icmp eq i8 %13, 0
  br i1 %14, label %11, label %15, !llvm.loop !14

15:                                               ; preds = %11
  store volatile i64 %2, ptr inttoptr (i32 788529153 to ptr), align 8, !tbaa !4
  store volatile i64 788547904, ptr inttoptr (i32 788529161 to ptr), align 8, !tbaa !4
  store volatile i32 256, ptr inttoptr (i32 788529169 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  br label %16

16:                                               ; preds = %16, %15
  %17 = load volatile i8, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  %18 = and i8 %17, 4
  %19 = icmp eq i8 %18, 0
  br i1 %19, label %16, label %20, !llvm.loop !15

20:                                               ; preds = %16
  store i32 %4, ptr inttoptr (i32 788529281 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529280 to ptr), align 128, !tbaa !10
  br label %21

21:                                               ; preds = %21, %20
  %22 = load volatile i8, ptr inttoptr (i32 788529280 to ptr), align 128, !tbaa !10
  %23 = and i8 %22, 4
  %24 = icmp eq i8 %23, 0
  br i1 %24, label %21, label %25, !llvm.loop !16

25:                                               ; preds = %21
  store volatile i64 788548224, ptr inttoptr (i32 788529153 to ptr), align 8, !tbaa !4
  store volatile i64 %3, ptr inttoptr (i32 788529161 to ptr), align 8, !tbaa !4
  store volatile i32 40, ptr inttoptr (i32 788529169 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  br label %26

26:                                               ; preds = %26, %25
  %27 = load volatile i8, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  %28 = and i8 %27, 4
  %29 = icmp eq i8 %28, 0
  br i1 %29, label %26, label %30, !llvm.loop !17

30:                                               ; preds = %26
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
!5 = !{!"long long", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C/C++ TBAA"}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !6, i64 0}
!10 = !{!6, !6, i64 0}
!11 = distinct !{!11, !12, !13}
!12 = !{!"llvm.loop.mustprogress"}
!13 = !{!"llvm.loop.unroll.disable"}
!14 = distinct !{!14, !12, !13}
!15 = distinct !{!15, !12, !13}
!16 = distinct !{!16, !12, !13}
!17 = distinct !{!17, !12, !13}

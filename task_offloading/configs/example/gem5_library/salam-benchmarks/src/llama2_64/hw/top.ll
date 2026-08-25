; ModuleID = 'top.c'
source_filename = "top.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

; Function Attrs: nofree noinline norecurse nounwind
define dso_local void @top(i64 noundef %0, i64 noundef %1, i64 noundef %2) local_unnamed_addr #0 {
  store volatile i64 %0, ptr inttoptr (i32 788529153 to ptr), align 8, !tbaa !4
  store volatile i64 788529344, ptr inttoptr (i32 788529161 to ptr), align 8, !tbaa !4
  store volatile i32 32768, ptr inttoptr (i32 788529169 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  br label %4

4:                                                ; preds = %4, %3
  %5 = load volatile i8, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  %6 = and i8 %5, 4
  %7 = icmp eq i8 %6, 0
  br i1 %7, label %4, label %8, !llvm.loop !11

8:                                                ; preds = %4
  store volatile i64 %1, ptr inttoptr (i32 788529153 to ptr), align 8, !tbaa !4
  store volatile i64 788562176, ptr inttoptr (i32 788529161 to ptr), align 8, !tbaa !4
  store volatile i32 32768, ptr inttoptr (i32 788529169 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  br label %9

9:                                                ; preds = %9, %8
  %10 = load volatile i8, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  %11 = and i8 %10, 4
  %12 = icmp eq i8 %11, 0
  br i1 %12, label %9, label %13, !llvm.loop !14

13:                                               ; preds = %9
  store volatile i8 1, ptr inttoptr (i32 788529280 to ptr), align 128, !tbaa !10
  br label %14

14:                                               ; preds = %14, %13
  %15 = load volatile i8, ptr inttoptr (i32 788529280 to ptr), align 128, !tbaa !10
  %16 = and i8 %15, 4
  %17 = icmp eq i8 %16, 0
  br i1 %17, label %14, label %18, !llvm.loop !15

18:                                               ; preds = %14
  store volatile i64 788595008, ptr inttoptr (i32 788529153 to ptr), align 8, !tbaa !4
  store volatile i64 %2, ptr inttoptr (i32 788529161 to ptr), align 8, !tbaa !4
  store volatile i32 32768, ptr inttoptr (i32 788529169 to ptr), align 4, !tbaa !8
  store volatile i8 1, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  br label %19

19:                                               ; preds = %19, %18
  %20 = load volatile i8, ptr inttoptr (i32 788529152 to ptr), align 16777216, !tbaa !10
  %21 = and i8 %20, 4
  %22 = icmp eq i8 %21, 0
  br i1 %22, label %19, label %23, !llvm.loop !16

23:                                               ; preds = %19
  ret void
}

attributes #0 = { nofree noinline norecurse nounwind "frame-pointer"="all" "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+armv7-a,+dsp,+soft-float,+strict-align,-aes,-bf16,-d32,-dotprod,-fp-armv8,-fp-armv8d16,-fp-armv8d16sp,-fp-armv8sp,-fp16,-fp16fml,-fp64,-fpregs,-fullfp16,-mve,-mve.fp,-neon,-sha2,-thumb-mode,-vfp2,-vfp2sp,-vfp3,-vfp3d16,-vfp3d16sp,-vfp3sp,-vfp4,-vfp4d16,-vfp4d16sp,-vfp4sp" "use-soft-float"="true" }

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

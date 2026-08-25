; ModuleID = 'gemm.c'
source_filename = "gemm.c"
target datalayout = "e-m:e-p:32:32-Fi8-i64:64-v128:64:128-a:0:32-n32-S64"
target triple = "armv7-pc-none-eabi"

; Function Attrs: nofree noinline norecurse nosync nounwind memory(readwrite, inaccessiblemem: none)
define dso_local void @gemm() local_unnamed_addr #0 {
  br label %2

1:                                                ; preds = %2
  ret void

2:                                                ; preds = %0, %2
  %3 = phi i32 [ 0, %0 ], [ %69, %2 ]
  %4 = and i32 %3, 7
  %5 = and i32 %3, 56
  %6 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %5
  %7 = load double, ptr %6, align 64, !tbaa !4
  %8 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %4
  %9 = load double, ptr %8, align 8, !tbaa !4
  %10 = fmul double %7, %9
  %11 = fadd double %10, 0.000000e+00
  %12 = or disjoint i32 %5, 1
  %13 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %12
  %14 = load double, ptr %13, align 8, !tbaa !4
  %15 = or disjoint i32 %4, 8
  %16 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %15
  %17 = load double, ptr %16, align 8, !tbaa !4
  %18 = fmul double %14, %17
  %19 = fadd double %11, %18
  %20 = or disjoint i32 %5, 2
  %21 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %20
  %22 = load double, ptr %21, align 16, !tbaa !4
  %23 = or disjoint i32 %4, 16
  %24 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %23
  %25 = load double, ptr %24, align 8, !tbaa !4
  %26 = fmul double %22, %25
  %27 = fadd double %19, %26
  %28 = or disjoint i32 %5, 3
  %29 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %28
  %30 = load double, ptr %29, align 8, !tbaa !4
  %31 = or disjoint i32 %4, 24
  %32 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %31
  %33 = load double, ptr %32, align 8, !tbaa !4
  %34 = fmul double %30, %33
  %35 = fadd double %27, %34
  %36 = or disjoint i32 %5, 4
  %37 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %36
  %38 = load double, ptr %37, align 32, !tbaa !4
  %39 = or disjoint i32 %4, 32
  %40 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %39
  %41 = load double, ptr %40, align 8, !tbaa !4
  %42 = fmul double %38, %41
  %43 = fadd double %35, %42
  %44 = or disjoint i32 %5, 5
  %45 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %44
  %46 = load double, ptr %45, align 8, !tbaa !4
  %47 = or disjoint i32 %4, 40
  %48 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %47
  %49 = load double, ptr %48, align 8, !tbaa !4
  %50 = fmul double %46, %49
  %51 = fadd double %43, %50
  %52 = or disjoint i32 %5, 6
  %53 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %52
  %54 = load double, ptr %53, align 16, !tbaa !4
  %55 = or disjoint i32 %4, 48
  %56 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %55
  %57 = load double, ptr %56, align 8, !tbaa !4
  %58 = fmul double %54, %57
  %59 = fadd double %51, %58
  %60 = or disjoint i32 %5, 7
  %61 = getelementptr inbounds double, ptr inttoptr (i32 788529344 to ptr), i32 %60
  %62 = load double, ptr %61, align 8, !tbaa !4
  %63 = or disjoint i32 %4, 56
  %64 = getelementptr inbounds double, ptr inttoptr (i32 788529920 to ptr), i32 %63
  %65 = load double, ptr %64, align 8, !tbaa !4
  %66 = fmul double %62, %65
  %67 = fadd double %59, %66
  %68 = getelementptr inbounds double, ptr inttoptr (i32 788530496 to ptr), i32 %3
  store double %67, ptr %68, align 8, !tbaa !4
  %69 = add nuw nsw i32 %3, 1
  %70 = icmp eq i32 %69, 64
  br i1 %70, label %1, label %2, !llvm.loop !8
}

attributes #0 = { nofree noinline norecurse nosync nounwind memory(readwrite, inaccessiblemem: none) "frame-pointer"="all" "no-builtins" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="generic" "target-features"="+armv7-a,+dsp,+soft-float,+strict-align,-aes,-bf16,-d32,-dotprod,-fp-armv8,-fp-armv8d16,-fp-armv8d16sp,-fp-armv8sp,-fp16,-fp16fml,-fp64,-fpregs,-fullfp16,-mve,-mve.fp,-neon,-sha2,-thumb-mode,-vfp2,-vfp2sp,-vfp3,-vfp3d16,-vfp3d16sp,-vfp3sp,-vfp4,-vfp4d16,-vfp4d16sp,-vfp4sp" "use-soft-float"="true" }

!llvm.module.flags = !{!0, !1, !2}
!llvm.ident = !{!3}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 1, !"min_enum_size", i32 4}
!2 = !{i32 7, !"frame-pointer", i32 2}
!3 = !{!"Ubuntu clang version 18.1.3 (1ubuntu1)"}
!4 = !{!5, !5, i64 0}
!5 = !{!"double", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C/C++ TBAA"}
!8 = distinct !{!8, !9, !10}
!9 = !{!"llvm.loop.mustprogress"}
!10 = !{!"llvm.loop.unroll.disable"}

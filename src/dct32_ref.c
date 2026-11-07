/*
 * dct32_ref — DCT de 32 puntos de libmad como referencia en C del kernel asm (src/synth_kernels.h).
 * GENERADO por tools/gen_dct32_ref.py desde third_party/libmad/synth.c: no editar a mano.
 * libmad: Copyright (C) 2000-2004 Underbit Technologies, Inc. (GPL v2 o posterior).
 * Aritmética de 32 bits con envoltura; MUL es mad_f_mul con FPM_64BIT: (int32)(((int64)x*y) >> 28).
 * Compilar con -fwrapv (la envoltura de enteros con signo es intencional).
 */
#include "synth_kernels.h"

typedef int32_t fx;
#define MUL(x, y)  ((fx)(((int64_t)(x) * (y)) >> 28))
#define SHIFT(x)   (x)
#define costab1 ((fx)268112114)
#define costab2 ((fx)267142866)
#define costab3 ((fx)265530048)
#define costab4 ((fx)263277544)
#define costab5 ((fx)260390782)
#define costab6 ((fx)256876715)
#define costab7 ((fx)252743810)
#define costab8 ((fx)248002024)
#define costab9 ((fx)242662778)
#define costab10 ((fx)236738937)
#define costab11 ((fx)230244771)
#define costab12 ((fx)223195925)
#define costab13 ((fx)215609380)
#define costab14 ((fx)207503414)
#define costab15 ((fx)198897553)
#define costab16 ((fx)189812531)
#define costab17 ((fx)180270234)
#define costab18 ((fx)170293651)
#define costab19 ((fx)159906814)
#define costab20 ((fx)149134749)
#define costab21 ((fx)138003405)
#define costab22 ((fx)126539598)
#define costab23 ((fx)114770946)
#define costab24 ((fx)102725802)
#define costab25 ((fx)90433181)
#define costab26 ((fx)77922700)
#define costab27 ((fx)65224495)
#define costab28 ((fx)52369160)
#define costab29 ((fx)39387662)
#define costab30 ((fx)26311276)
#define costab31 ((fx)13171504)

void mp368k_dct32_ref(const int32_t in[32], unsigned int slot, int32_t lo[16][8], int32_t hi[16][8])
{
    fx t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13, t14, t15, t16, t17, t18, t19, t20, t21, t22, t23, t24, t25, t26, t27, t28, t29, t30, t31, t32, t33, t34, t35, t36, t37, t38, t39, t40, t41, t42, t43, t44, t45, t46, t47, t48, t49, t50, t51, t52, t53, t54, t55, t56, t57, t58, t59, t60, t61, t62, t63, t64, t65, t66, t67, t68, t69, t70, t71, t72, t73, t74, t75, t76, t77, t78, t79, t80, t81, t82, t83, t84, t85, t86, t87, t88, t89, t90, t91, t92, t93, t94, t95, t96, t97, t98, t99, t100, t101, t102, t103, t104, t105, t106, t107, t108, t109, t110, t111, t112, t113, t114, t115, t116, t117, t118, t119, t120, t121, t122, t123, t124, t125, t126, t127, t128, t129, t130, t131, t132, t133, t134, t135, t136, t137, t138, t139, t140, t141, t142, t143, t144, t145, t146, t147, t148, t149, t150, t151, t152, t153, t154, t155, t156, t157, t158, t159, t160, t161, t162, t163, t164, t165, t166, t167, t168, t169, t170, t171, t172, t173, t174, t175, t176;

    t0 = in[0] + in[31];
    t16 = MUL(in[0] - in[31], costab1);
    t1 = in[15] + in[16];
    t17 = MUL(in[15] - in[16], costab31);
    t41 = t16 + t17;
    t59 = MUL(t16 - t17, costab2);
    t33 = t0 + t1;
    t50 = MUL(t0 - t1, costab2);
    t2 = in[7] + in[24];
    t18 = MUL(in[7] - in[24], costab15);
    t3 = in[8] + in[23];
    t19 = MUL(in[8] - in[23], costab17);
    t42 = t18 + t19;
    t60 = MUL(t18 - t19, costab30);
    t34 = t2 + t3;
    t51 = MUL(t2 - t3, costab30);
    t4 = in[3] + in[28];
    t20 = MUL(in[3] - in[28], costab7);
    t5 = in[12] + in[19];
    t21 = MUL(in[12] - in[19], costab25);
    t43 = t20 + t21;
    t61 = MUL(t20 - t21, costab14);
    t35 = t4 + t5;
    t52 = MUL(t4 - t5, costab14);
    t6 = in[4] + in[27];
    t22 = MUL(in[4] - in[27], costab9);
    t7 = in[11] + in[20];
    t23 = MUL(in[11] - in[20], costab23);
    t44 = t22 + t23;
    t62 = MUL(t22 - t23, costab18);
    t36 = t6 + t7;
    t53 = MUL(t6 - t7, costab18);
    t8 = in[1] + in[30];
    t24 = MUL(in[1] - in[30], costab3);
    t9 = in[14] + in[17];
    t25 = MUL(in[14] - in[17], costab29);
    t45 = t24 + t25;
    t63 = MUL(t24 - t25, costab6);
    t37 = t8 + t9;
    t54 = MUL(t8 - t9, costab6);
    t10 = in[6] + in[25];
    t26 = MUL(in[6] - in[25], costab13);
    t11 = in[9] + in[22];
    t27 = MUL(in[9] - in[22], costab19);
    t46 = t26 + t27;
    t64 = MUL(t26 - t27, costab26);
    t38 = t10 + t11;
    t55 = MUL(t10 - t11, costab26);
    t12 = in[2] + in[29];
    t28 = MUL(in[2] - in[29], costab5);
    t13 = in[13] + in[18];
    t29 = MUL(in[13] - in[18], costab27);
    t47 = t28 + t29;
    t65 = MUL(t28 - t29, costab10);
    t39 = t12 + t13;
    t56 = MUL(t12 - t13, costab10);
    t14 = in[5] + in[26];
    t30 = MUL(in[5] - in[26], costab11);
    t15 = in[10] + in[21];
    t31 = MUL(in[10] - in[21], costab21);
    t48 = t30 + t31;
    t66 = MUL(t30 - t31, costab22);
    t40 = t14 + t15;
    t57 = MUL(t14 - t15, costab22);
    t69 = t33 + t34;
    t89 = MUL(t33 - t34, costab4);
    t70 = t35 + t36;
    t90 = MUL(t35 - t36, costab28);
    t71 = t37 + t38;
    t91 = MUL(t37 - t38, costab12);
    t72 = t39 + t40;
    t92 = MUL(t39 - t40, costab20);
    t73 = t41 + t42;
    t94 = MUL(t41 - t42, costab4);
    t74 = t43 + t44;
    t95 = MUL(t43 - t44, costab28);
    t75 = t45 + t46;
    t96 = MUL(t45 - t46, costab12);
    t76 = t47 + t48;
    t97 = MUL(t47 - t48, costab20);
    t78 = t50 + t51;
    t100 = MUL(t50 - t51, costab4);
    t79 = t52 + t53;
    t101 = MUL(t52 - t53, costab28);
    t80 = t54 + t55;
    t102 = MUL(t54 - t55, costab12);
    t81 = t56 + t57;
    t103 = MUL(t56 - t57, costab20);
    t83 = t59 + t60;
    t106 = MUL(t59 - t60, costab4);
    t84 = t61 + t62;
    t107 = MUL(t61 - t62, costab28);
    t85 = t63 + t64;
    t108 = MUL(t63 - t64, costab12);
    t86 = t65 + t66;
    t109 = MUL(t65 - t66, costab20);
    t113 = t69 + t70;
    t114 = t71 + t72;
    hi[15][slot] = SHIFT(t113 + t114);
    lo[ 0][slot] = SHIFT(MUL(t113 - t114, costab16));
    t115 = t73 + t74;
    t116 = t75 + t76;
    t32 = t115 + t116;
    hi[14][slot] = SHIFT(t32);
    t118 = t78 + t79;
    t119 = t80 + t81;
    t58 = t118 + t119;
    hi[13][slot] = SHIFT(t58);
    t121 = t83 + t84;
    t122 = t85 + t86;
    t67 = t121 + t122;
    t49 = (t67 * 2) - t32;
    hi[12][slot] = SHIFT(t49);
    t125 = t89 + t90;
    t126 = t91 + t92;
    t93 = t125 + t126;
    hi[11][slot] = SHIFT(t93);
    t128 = t94 + t95;
    t129 = t96 + t97;
    t98 = t128 + t129;
    t68 = (t98 * 2) - t49;
    hi[10][slot] = SHIFT(t68);
    t132 = t100 + t101;
    t133 = t102 + t103;
    t104 = t132 + t133;
    t82 = (t104 * 2) - t58;
    hi[ 9][slot] = SHIFT(t82);
    t136 = t106 + t107;
    t137 = t108 + t109;
    t110 = t136 + t137;
    t87 = (t110 * 2) - t67;
    t77 = (t87 * 2) - t68;
    hi[ 8][slot] = SHIFT(t77);
    t141 = MUL(t69 - t70, costab8);
    t142 = MUL(t71 - t72, costab24);
    t143 = t141 + t142;
    hi[ 7][slot] = SHIFT(t143);
    lo[ 8][slot] = SHIFT((MUL(t141 - t142, costab16) * 2) - t143);
    t144 = MUL(t73 - t74, costab8);
    t145 = MUL(t75 - t76, costab24);
    t146 = t144 + t145;
    t88 = (t146 * 2) - t77;
    hi[ 6][slot] = SHIFT(t88);
    t148 = MUL(t78 - t79, costab8);
    t149 = MUL(t80 - t81, costab24);
    t150 = t148 + t149;
    t105 = (t150 * 2) - t82;
    hi[ 5][slot] = SHIFT(t105);
    t152 = MUL(t83 - t84, costab8);
    t153 = MUL(t85 - t86, costab24);
    t154 = t152 + t153;
    t111 = (t154 * 2) - t87;
    t99 = (t111 * 2) - t88;
    hi[ 4][slot] = SHIFT(t99);
    t157 = MUL(t89 - t90, costab8);
    t158 = MUL(t91 - t92, costab24);
    t159 = t157 + t158;
    t127 = (t159 * 2) - t93;
    hi[ 3][slot] = SHIFT(t127);
    t160 = (MUL(t125 - t126, costab16) * 2) - t127;
    lo[ 4][slot] = SHIFT(t160);
    lo[12][slot] = SHIFT((((MUL(t157 - t158, costab16) * 2) - t159) * 2) - t160);
    t161 = MUL(t94 - t95, costab8);
    t162 = MUL(t96 - t97, costab24);
    t163 = t161 + t162;
    t130 = (t163 * 2) - t98;
    t112 = (t130 * 2) - t99;
    hi[ 2][slot] = SHIFT(t112);
    t164 = (MUL(t128 - t129, costab16) * 2) - t130;
    t166 = MUL(t100 - t101, costab8);
    t167 = MUL(t102 - t103, costab24);
    t168 = t166 + t167;
    t134 = (t168 * 2) - t104;
    t120 = (t134 * 2) - t105;
    hi[ 1][slot] = SHIFT(t120);
    t135 = (MUL(t118 - t119, costab16) * 2) - t120;
    lo[ 2][slot] = SHIFT(t135);
    t169 = (MUL(t132 - t133, costab16) * 2) - t134;
    t151 = (t169 * 2) - t135;
    lo[ 6][slot] = SHIFT(t151);
    t170 = (((MUL(t148 - t149, costab16) * 2) - t150) * 2) - t151;
    lo[10][slot] = SHIFT(t170);
    lo[14][slot] = SHIFT((((((MUL(t166 - t167, costab16) * 2) - t168) * 2) - t169) * 2) - t170);
    t171 = MUL(t106 - t107, costab8);
    t172 = MUL(t108 - t109, costab24);
    t173 = t171 + t172;
    t138 = (t173 * 2) - t110;
    t123 = (t138 * 2) - t111;
    t139 = (MUL(t121 - t122, costab16) * 2) - t123;
    t117 = (t123 * 2) - t112;
    hi[ 0][slot] = SHIFT(t117);
    t124 = (MUL(t115 - t116, costab16) * 2) - t117;
    lo[ 1][slot] = SHIFT(t124);
    t131 = (t139 * 2) - t124;
    lo[ 3][slot] = SHIFT(t131);
    t140 = (t164 * 2) - t131;
    lo[ 5][slot] = SHIFT(t140);
    t174 = (MUL(t136 - t137, costab16) * 2) - t138;
    t155 = (t174 * 2) - t139;
    t147 = (t155 * 2) - t140;
    lo[ 7][slot] = SHIFT(t147);
    t156 = (((MUL(t144 - t145, costab16) * 2) - t146) * 2) - t147;
    lo[ 9][slot] = SHIFT(t156);
    t175 = (((MUL(t152 - t153, costab16) * 2) - t154) * 2) - t155;
    t165 = (t175 * 2) - t156;
    lo[11][slot] = SHIFT(t165);
    t176 = (((((MUL(t161 - t162, costab16) * 2) - t163) * 2) - t164) * 2) - t165;
    lo[13][slot] = SHIFT(t176);
    lo[15][slot] = SHIFT((((((((MUL(t171 - t172, costab16) * 2) - t173) * 2) - t174) * 2) - t175) * 2) - t176);
}

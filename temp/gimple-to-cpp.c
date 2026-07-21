/* GIMPLE to C++ source emitter.
   Copyright (C) 2021 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

/* 本文件实现 GIMPLE IR -> C++20 源码 的 emitter。
   在 -fo3-pre-expand 开启时，所有 GIMPLE pass（含 O3 独有）完成后，
   由 cgraphunit.c 中的 symbol_table::compile() 调用 gimple_to_cpp_emit_all，
   将所有函数与全局变量以合法 C++20 源码形式输出到指定文件，
   随后跳过 RTL/backend。

   输出原则：
   - 所有 SSA 名在函数头预声明，确保先声明后使用
   - PHI 节点采用策略 A（在每条前驱边上插入对应赋值）
   - 复杂表达式加括号避免优先级问题
   - 不认识的节点退化到 print_generic_expr 或注释 */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "tree.h"
#include "gimple.h"
#include "gimple-iterator.h"
#include "gimple-pretty-print.h"
#include "tree-pretty-print.h"
#include "tree-cfg.h"
#include "cgraph.h"
#include "dumpfile.h"
#include "gimple-ssa.h"
#include "gimple-expr.h"
#include "stringpool.h"
#include "tree-vrp.h"
#include "tree-ssanames.h"
#include "tree-dfa.h"
#include "function.h"
#include "basic-block.h"
#include "cfgloop.h"
#include "gimple-to-cpp.h"
#include "attribs.h"
#include "real.h"
#include "fold-const.h"
#include "internal-fn.h"
#include "stor-layout.h"

/* ------------------------------------------------------------------ */
/* 前向声明                                                            */
/* ------------------------------------------------------------------ */

static void emit_type (FILE *out, tree type);
static void emit_decl_name (FILE *out, tree decl);
static void emit_ssa_name (FILE *out, tree name);
static void emit_tree (FILE *out, tree node);
static int  op_priority (enum tree_code code) ATTRIBUTE_UNUSED;

static void emit_gimple_stmt (FILE *out, gimple *stmt);
static void emit_gimple_assign (FILE *out, gassign *stmt);
static void emit_gimple_call (FILE *out, gcall *stmt);
static void emit_gimple_cond (FILE *out, gcond *stmt, basic_block bb);
static void emit_gimple_return (FILE *out, greturn *stmt);
static void emit_gimple_goto (FILE *out, ggoto *stmt);
static void emit_gimple_label (FILE *out, glabel *stmt);
static void emit_gimple_switch (FILE *out, gswitch *stmt, basic_block bb);
static void emit_gimple_bind (FILE *out, gbind *stmt);
static void emit_gimple_asm (FILE *out, gasm *stmt);

static void emit_unary_rhs (FILE *out, gassign *stmt);
static void emit_binary_rhs (FILE *out, gassign *stmt);
static void emit_ternary_rhs (FILE *out, gassign *stmt);

static void emit_function (FILE *out, tree fndecl);
static void emit_global_variable (FILE *out, varpool_node *vnode);

/* PHI 策略 A：输出从 pred 到 succ 这条边上应执行的 phi 赋值  */
static void emit_phi_assignments_for_edge (FILE *out, basic_block pred,
					   basic_block succ);

/* 输出 BB 的所有非终结语句（不含 PHI 与 terminator）。
   返回 terminator 语句（若存在），否则 NULL。  */
static gimple *emit_bb_non_terminator_stmts (FILE *out, basic_block bb);

/* 判断 stmt 是否是 BB 的终结语句（goto/cond/return/switch/resx/asm）  */
static bool is_terminator_p (gimple *stmt);

/* 输出 BB 终结语句（含前驱 PHI 赋值）  */
static void emit_bb_terminator (FILE *out, basic_block bb);

/* ------------------------------------------------------------------ */
/* 类型输出                                                            */
/* ------------------------------------------------------------------ */

/* 输出类型 type 到 out。  */
static void
emit_type (FILE *out, tree type)
{
  if (type == NULL_TREE || type == error_mark_node)
    {
      fprintf (out, "void");
      return;
    }

  switch (TREE_CODE (type))
    {
    case VOID_TYPE:
      fprintf (out, "void");
      break;

    case BOOLEAN_TYPE:
      fprintf (out, "bool");
      break;

    case INTEGER_TYPE:
      {
	tree name = TYPE_NAME (type);
	const char *s = NULL;
	if (name && TREE_CODE (name) == IDENTIFIER_NODE
	    && IDENTIFIER_POINTER (name))
	  s = IDENTIFIER_POINTER (name);
	else if (name && TREE_CODE (name) == TYPE_DECL && DECL_NAME (name))
	  s = IDENTIFIER_POINTER (DECL_NAME (name));

	if (s)
	  {
	    if (strcmp (s, "_Bool") == 0)
	      fprintf (out, "bool");
	    else if (strcmp (s, "__int128") == 0)
	      fprintf (out, "__int128");
	    else if (strcmp (s, "unsigned __int128") == 0)
	      fprintf (out, "unsigned __int128");
	    else
	      fprintf (out, "%s", s);
	  }
	else
	  {
	    /* 无名整数类型，根据精度推断  */
	    unsigned prec = TYPE_PRECISION (type);
	    bool uns = TYPE_UNSIGNED (type);
	    if (prec == CHAR_TYPE_SIZE)
	      fprintf (out, uns ? "unsigned char" : "char");
	    else if (prec == SHORT_TYPE_SIZE)
	      fprintf (out, uns ? "unsigned short" : "short");
	    else if (prec == INT_TYPE_SIZE)
	      fprintf (out, uns ? "unsigned int" : "int");
	    else if (prec == LONG_TYPE_SIZE)
	      fprintf (out, uns ? "unsigned long" : "long");
	    else if (prec == LONG_LONG_TYPE_SIZE)
	      fprintf (out, uns ? "unsigned long long" : "long long");
	    else
	      fprintf (out, "int%u_t", prec);
	  }
	break;
      }

    case REAL_TYPE:
      {
	unsigned prec = TYPE_PRECISION (type);
	if (prec == FLOAT_TYPE_SIZE)
	  fprintf (out, "float");
	else if (prec == DOUBLE_TYPE_SIZE)
	  fprintf (out, "double");
	else if (prec == LONG_DOUBLE_TYPE_SIZE)
	  fprintf (out, "long double");
	else
	  fprintf (out, "double");
	break;
      }

    case POINTER_TYPE:
      emit_type (out, TREE_TYPE (type));
      fprintf (out, "*");
      break;

    case REFERENCE_TYPE:
      emit_type (out, TREE_TYPE (type));
      fprintf (out, "&");
      break;

    case ARRAY_TYPE:
      {
	tree elem = TREE_TYPE (type);
	tree dom = TYPE_DOMAIN (type);
	emit_type (out, elem);
	if (dom)
	  {
	    tree max = TYPE_MAX_VALUE (dom);
	    tree min = TYPE_MIN_VALUE (dom);
	    if (max && TREE_CODE (max) == INTEGER_CST)
	      {
		unsigned HOST_WIDE_INT sz = tree_to_uhwi (max) + 1;
		if (min && TREE_CODE (min) == INTEGER_CST
		    && tree_to_uhwi (min) != 0)
		  sz -= tree_to_uhwi (min);
		fprintf (out, " [%" PRIu64 "]", (uint64_t) sz);
	      }
	    else
	      fprintf (out, " []");
	  }
	else
	  fprintf (out, " []");
	break;
      }

    case RECORD_TYPE:
      {
	tree name = TYPE_NAME (type);
	if (name && TREE_CODE (name) == TYPE_DECL && DECL_NAME (name))
	  fprintf (out, "struct %s", IDENTIFIER_POINTER (DECL_NAME (name)));
	else if (name && TREE_CODE (name) == IDENTIFIER_NODE)
	  fprintf (out, "struct %s", IDENTIFIER_POINTER (name));
	else
	  fprintf (out, "/* anon_record */ int");
	break;
      }

    case UNION_TYPE:
    case QUAL_UNION_TYPE:
      {
	tree name = TYPE_NAME (type);
	if (name && TREE_CODE (name) == TYPE_DECL && DECL_NAME (name))
	  fprintf (out, "union %s", IDENTIFIER_POINTER (DECL_NAME (name)));
	else if (name && TREE_CODE (name) == IDENTIFIER_NODE)
	  fprintf (out, "union %s", IDENTIFIER_POINTER (name));
	else
	  fprintf (out, "/* anon_union */ int");
	break;
      }

    case ENUMERAL_TYPE:
      {
	tree name = TYPE_NAME (type);
	if (name && TREE_CODE (name) == TYPE_DECL && DECL_NAME (name))
	  fprintf (out, "enum %s", IDENTIFIER_POINTER (DECL_NAME (name)));
	else if (name && TREE_CODE (name) == IDENTIFIER_NODE)
	  fprintf (out, "enum %s", IDENTIFIER_POINTER (name));
	else
	  fprintf (out, "int");
	break;
      }

    case FUNCTION_TYPE:
    case METHOD_TYPE:
      emit_type (out, TREE_TYPE (type));
      fprintf (out, " (*)(...)");
      break;

    case COMPLEX_TYPE:
      fprintf (out, "_Complex ");
      emit_type (out, TREE_TYPE (type));
      break;

    case VECTOR_TYPE:
      {
	tree elem = TREE_TYPE (type);
	unsigned n = TYPE_VECTOR_SUBPARTS (type).to_constant ();
	fprintf (out, "/* vector<");
	emit_type (out, elem);
	fprintf (out, ",%u> */ ", n);
	emit_type (out, elem);
	break;
      }

    case NULLPTR_TYPE:
      fprintf (out, "std::nullptr_t");
      break;

    case OFFSET_TYPE:
      fprintf (out, "/* offset */ long");
      break;

    case LANG_TYPE:
      fprintf (out, "/* lang_type */ int");
      break;

    default:
      fprintf (out, "/* fallback_type_%s */ int",
	       get_tree_code_name (TREE_CODE (type)));
      break;
    }
}

/* ------------------------------------------------------------------ */
/* 名字输出                                                            */
/* ------------------------------------------------------------------ */

/* 输出 _DECL 的名字。  */
static void
emit_decl_name (FILE *out, tree decl)
{
  if (decl == NULL_TREE)
    {
      fprintf (out, "/* null_decl */");
      return;
    }

  tree name = DECL_NAME (decl);
  if (name && IDENTIFIER_POINTER (name))
    {
      fprintf (out, "%s", IDENTIFIER_POINTER (name));
      return;
    }

  if (DECL_ASSEMBLER_NAME_SET_P (decl))
    {
      const char *s = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
      fprintf (out, "_gcm_");
      for (const char *p = s; *p; ++p)
	{
	  if ((ISALNUM (*p) && *p != '.') || *p == '_')
	    fputc (*p, out);
	  else
	    fputc ('_', out);
	}
      return;
    }

  fprintf (out, "_anon_%p", (void *) decl);
}

/* 输出 LABEL_DECL 的名字（用于 goto 目标）。
   我们用 L<index> 形式，其中 index 来自 DECL_UID 或 LABEL_DECL_UID。
   实际上更简单：用 DECL_NAME 或生成 _L<uid>。  */
static void
emit_label_name (FILE *out, tree label)
{
  if (label == NULL_TREE)
    {
      fprintf (out, "/* null_label */");
      return;
    }

  tree name = DECL_NAME (label);
  if (name && IDENTIFIER_POINTER (name))
    {
      const char *s = IDENTIFIER_POINTER (name);
      /* 清洗为合法 C++ 标识符  */
      fprintf (out, "L_");
      for (const char *p = s; *p; ++p)
	{
	  if ((ISALNUM (*p) && *p != '.') || *p == '_')
	    fputc (*p, out);
	  else
	    fputc ('_', out);
	}
      return;
    }

  /* 用 UID 生成唯一名  */
  fprintf (out, "L_%u", (unsigned) DECL_UID (label));
}

/* 输出 SSA 名引用。
   - 默认定义且关联 _DECL：直接用 _DECL 名（参数/返回值的初始 SSA）
   - 关联有命名 _DECL：<name>_<version>
   - 否则：ssa_<version>  */
static void
emit_ssa_name (FILE *out, tree name)
{
  if (name == NULL_TREE || TREE_CODE (name) != SSA_NAME)
    {
      fprintf (out, "/* bad_ssa */");
      return;
    }

  unsigned ver = SSA_NAME_VERSION (name);
  tree var = SSA_NAME_VAR (name);

  /* 默认定义：直接用关联 _DECL 的名字  */
  if (SSA_NAME_IS_DEFAULT_DEF (name) && var)
    {
      if ((TREE_CODE (var) == PARM_DECL
	   || TREE_CODE (var) == RESULT_DECL
	   || TREE_CODE (var) == VAR_DECL)
	  && DECL_NAME (var)
	  && IDENTIFIER_POINTER (DECL_NAME (var)))
	{
	  fprintf (out, "%s", IDENTIFIER_POINTER (DECL_NAME (var)));
	  return;
	}
    }

  /* 关联有命名 _DECL：<name>_<version>  */
  if (var
      && (TREE_CODE (var) == VAR_DECL
	  || TREE_CODE (var) == PARM_DECL
	  || TREE_CODE (var) == RESULT_DECL)
      && DECL_NAME (var)
      && IDENTIFIER_POINTER (DECL_NAME (var)))
    {
      fprintf (out, "%s_%u", IDENTIFIER_POINTER (DECL_NAME (var)), ver);
      return;
    }

  /* 否则：ssa_<version>  */
  fprintf (out, "ssa_%u", ver);
}

/* ------------------------------------------------------------------ */
/* tree 表达式输出                                                     */
/* ------------------------------------------------------------------ */

/* 返回运算符优先级（保留给将来使用，目前 emit_tree 全程加括号）  */
static int
op_priority (enum tree_code code)
{
  switch (code)
    {
    case INTEGER_CST:
    case REAL_CST:
    case STRING_CST:
    case SSA_NAME:
    case VAR_DECL:
    case PARM_DECL:
    case RESULT_DECL:
    case FIELD_DECL:
    case CONST_DECL:
    case FUNCTION_DECL:
    case LABEL_DECL:
    case ARRAY_REF:
    case COMPONENT_REF:
    case BIT_FIELD_REF:
    case INDIRECT_REF:
    case MEM_REF:
    case TARGET_MEM_REF:
      return 15;
    case ADDR_EXPR:
    case CONVERT_EXPR:
    case NOP_EXPR:
    case FLOAT_EXPR:
    case FIX_TRUNC_EXPR:
    case NEGATE_EXPR:
    case BIT_NOT_EXPR:
    case TRUTH_NOT_EXPR:
    case ABS_EXPR:
    case ABSU_EXPR:
    case CONSTRUCTOR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
    case PAREN_EXPR:
    case VIEW_CONVERT_EXPR:
      return 14;
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case RDIV_EXPR:
    case TRUNC_MOD_EXPR:
    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
    case EXACT_DIV_EXPR:
    case POINTER_PLUS_EXPR:
    case POINTER_DIFF_EXPR:
      return 13;
    case PLUS_EXPR:
    case MINUS_EXPR:
      return 12;
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case LROTATE_EXPR:
    case RROTATE_EXPR:
      return 11;
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
      return 10;
    case EQ_EXPR:
    case NE_EXPR:
      return 9;
    case BIT_AND_EXPR:
      return 8;
    case BIT_XOR_EXPR:
    case TRUTH_XOR_EXPR:
      return 7;
    case BIT_IOR_EXPR:
      return 6;
    case TRUTH_AND_EXPR:
    case TRUTH_ANDIF_EXPR:
      return 5;
    case TRUTH_OR_EXPR:
    case TRUTH_ORIF_EXPR:
      return 4;
    case COND_EXPR:
    case VEC_COND_EXPR:
      return 3;
    case MODIFY_EXPR:
    case INIT_EXPR:
      return 2;
    default:
      return 0;
    }
}

/* 输出 tree 表达式 node。
   所有子表达式都加括号确保正确性。  */
static void
emit_tree (FILE *out, tree node)
{
  if (node == NULL_TREE)
    {
      fprintf (out, "/* null */");
      return;
    }

  switch (TREE_CODE (node))
    {
    case INTEGER_CST:
      {
	widest_int wi = wi::to_widest (node);
	print_dec (wi, out, TYPE_SIGN (TREE_TYPE (node)));
	break;
      }

    case REAL_CST:
      {
	char buf[256];
	REAL_VALUE_TYPE r = TREE_REAL_CST (node);
	real_to_decimal (buf, &r, sizeof (buf), 0, 1);
	fprintf (out, "%s", buf);
	tree rt = TREE_TYPE (node);
	if (rt && TYPE_PRECISION (rt) == FLOAT_TYPE_SIZE)
	  fprintf (out, "f");
	else if (rt && TYPE_PRECISION (rt) == LONG_DOUBLE_TYPE_SIZE)
	  fprintf (out, "L");
	break;
      }

    case FIXED_CST:
      fprintf (out, "/* fixed */ 0");
      break;

    case STRING_CST:
      {
	const char *s = TREE_STRING_POINTER (node);
	size_t len = TREE_STRING_LENGTH (node);
	fprintf (out, "\"");
	for (size_t i = 0; i < len; ++i)
	  {
	    unsigned char c = (unsigned char) s[i];
	    if (c == '"' || c == '\\')
	      fprintf (out, "\\%c", c);
	    else if (c >= 0x20 && c < 0x7f)
	      fputc (c, out);
	    else
	      fprintf (out, "\\x%02x", c);
	  }
	fprintf (out, "\"");
	break;
      }

    case SSA_NAME:
      emit_ssa_name (out, node);
      break;

    case VAR_DECL:
    case PARM_DECL:
    case RESULT_DECL:
    case CONST_DECL:
    case FUNCTION_DECL:
    case TYPE_DECL:
      emit_decl_name (out, node);
      break;

    case LABEL_DECL:
      emit_label_name (out, node);
      break;

    case FIELD_DECL:
      {
	tree name = DECL_NAME (node);
	if (name && IDENTIFIER_POINTER (name))
	  fprintf (out, "%s", IDENTIFIER_POINTER (name));
	else
	  fprintf (out, "/* anon_field */");
	break;
      }

    case ADDR_EXPR:
      fprintf (out, "(&(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, "))");
      break;

    case INDIRECT_REF:
      fprintf (out, "(*(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, "))");
      break;

    case MEM_REF:
      {
	/* MEM_REF: *(T*)(base + offset). 第二操作数是字节偏移常量。  */
	tree base = TREE_OPERAND (node, 0);
	tree off = TREE_OPERAND (node, 1);
	fprintf (out, "(*(");
	if (off && TREE_CODE (off) == INTEGER_CST
	    && wi::to_widest (off) != 0)
	  {
	    fprintf (out, "(/* memref */ (char*)(");
	    emit_tree (out, base);
	    fprintf (out, ")) + ");
	    wide_int wo = wi::to_wide (off);
	    print_dec (wo, out, SIGNED);
	    fprintf (out, ")");
	  }
	else
	  {
	    emit_tree (out, base);
	  }
	fprintf (out, "))");
	break;
      }

    case ARRAY_REF:
      fprintf (out, "(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")[");
      emit_tree (out, TREE_OPERAND (node, 1));
      fprintf (out, "]");
      break;

    case ARRAY_RANGE_REF:
      fprintf (out, "(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")[");
      emit_tree (out, TREE_OPERAND (node, 1));
      fprintf (out, "]");
      break;

    case COMPONENT_REF:
      {
	tree obj = TREE_OPERAND (node, 0);
	tree field = TREE_OPERAND (node, 1);
	fprintf (out, "(");
	emit_tree (out, obj);
	fprintf (out, ")");
	bool is_ptr = (TREE_CODE (obj) == INDIRECT_REF
		       || TREE_CODE (obj) == MEM_REF
		       || (TREE_TYPE (obj)
			   && POINTER_TYPE_P (TREE_TYPE (obj))));
	fprintf (out, is_ptr ? "->" : ".");
	if (field && TREE_CODE (field) == FIELD_DECL
	    && DECL_NAME (field))
	  fprintf (out, "%s", IDENTIFIER_POINTER (DECL_NAME (field)));
	else
	  fprintf (out, "/* anon_field */");
	break;
      }

    case BIT_FIELD_REF:
      fprintf (out, "/* bit_field_ref */ (");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")");
      break;

    case TARGET_MEM_REF:
    case VIEW_CONVERT_EXPR:
      fprintf (out, "(*((/* vview */ char*)&(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")))");
      break;

    case REALPART_EXPR:
      fprintf (out, "__builtin_real(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")");
      break;

    case IMAGPART_EXPR:
      fprintf (out, "__builtin_imag(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")");
      break;

    case PAREN_EXPR:
      fprintf (out, "(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")");
      break;

    case CONVERT_EXPR:
    case NOP_EXPR:
    case FLOAT_EXPR:
    case FIX_TRUNC_EXPR:
    case ADDR_SPACE_CONVERT_EXPR:
    case FIXED_CONVERT_EXPR:
      fprintf (out, "((/* cast */ ");
      emit_type (out, TREE_TYPE (node));
      fprintf (out, ")(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, "))");
      break;

    case NEGATE_EXPR:
      fprintf (out, "(-(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, "))");
      break;

    case BIT_NOT_EXPR:
      fprintf (out, "(~(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, "))");
      break;

    case TRUTH_NOT_EXPR:
      fprintf (out, "(!(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, "))");
      break;

    case ABS_EXPR:
      fprintf (out, "std::abs(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")");
      break;

    case ABSU_EXPR:
      fprintf (out, "((x)<0?-(x):(x)) /* absu */ (");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")");
      break;

    case CONJ_EXPR:
      fprintf (out, "__builtin_conj(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")");
      break;

    case PLUS_EXPR:
    case MINUS_EXPR:
    case MULT_EXPR:
    case TRUNC_DIV_EXPR:
    case CEIL_DIV_EXPR:
    case FLOOR_DIV_EXPR:
    case ROUND_DIV_EXPR:
    case RDIV_EXPR:
    case EXACT_DIV_EXPR:
    case TRUNC_MOD_EXPR:
    case CEIL_MOD_EXPR:
    case FLOOR_MOD_EXPR:
    case ROUND_MOD_EXPR:
    case LSHIFT_EXPR:
    case RSHIFT_EXPR:
    case LROTATE_EXPR:
    case RROTATE_EXPR:
    case BIT_AND_EXPR:
    case BIT_IOR_EXPR:
    case BIT_XOR_EXPR:
    case TRUTH_AND_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_OR_EXPR:
    case TRUTH_ORIF_EXPR:
    case TRUTH_XOR_EXPR:
    case LT_EXPR:
    case LE_EXPR:
    case GT_EXPR:
    case GE_EXPR:
    case EQ_EXPR:
    case NE_EXPR:
    case UNEQ_EXPR:
    case UNLT_EXPR:
    case UNLE_EXPR:
    case UNGT_EXPR:
    case UNGE_EXPR:
    case LTGT_EXPR:
    case UNORDERED_EXPR:
    case ORDERED_EXPR:
    case POINTER_PLUS_EXPR:
    case POINTER_DIFF_EXPR:
    case MAX_EXPR:
    case MIN_EXPR:
      {
	enum tree_code code = TREE_CODE (node);

	if (code == MIN_EXPR)
	  {
	    fprintf (out, "std::min((");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, "), (");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, "))");
	    break;
	  }
	if (code == MAX_EXPR)
	  {
	    fprintf (out, "std::max((");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, "), (");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, "))");
	    break;
	  }

	if (code == UNORDERED_EXPR)
	  {
	    fprintf (out, "(std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") || std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, "))");
	    break;
	  }
	if (code == ORDERED_EXPR)
	  {
	    fprintf (out, "(!std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") && !std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, "))");
	    break;
	  }
	if (code == LTGT_EXPR)
	  {
	    fprintf (out, "(!std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") && !std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, ") && (");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") != (");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, "))");
	    break;
	  }
	if (code == UNLT_EXPR || code == UNLE_EXPR
	    || code == UNGT_EXPR || code == UNGE_EXPR
	    || code == UNEQ_EXPR)
	  {
	    const char *real_op = "==";
	    if (code == UNLT_EXPR) real_op = "<";
	    else if (code == UNLE_EXPR) real_op = "<=";
	    else if (code == UNGT_EXPR) real_op = ">";
	    else if (code == UNGE_EXPR) real_op = ">=";
	    fprintf (out, "((std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") || std::isnan(");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, ")) ? false : ((");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") %s (", real_op);
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, ")))");
	    break;
	  }

	if (code == POINTER_PLUS_EXPR)
	  {
	    fprintf (out, "((/* p+ */ char*)(");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") + (");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, ")/*@bytes*/)");
	    break;
	  }
	if (code == POINTER_DIFF_EXPR)
	  {
	    fprintf (out, "((/* p- */ char*)(");
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") - (char*)(");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, "))");
	    break;
	  }

	const char *op = op_symbol_code (code);
	if (code == FLOOR_DIV_EXPR || code == CEIL_DIV_EXPR
	    || code == ROUND_DIV_EXPR || code == EXACT_DIV_EXPR)
	  op = "/";
	if (code == FLOOR_MOD_EXPR || code == CEIL_MOD_EXPR
	    || code == ROUND_MOD_EXPR)
	  op = "%";

	if (strcmp (op, "<<< ??? >>>") == 0)
	  {
	    fprintf (out, "/* unhandled %s */ (",
		     get_tree_code_name (code));
	    emit_tree (out, TREE_OPERAND (node, 0));
	    fprintf (out, ") /* ? */ (");
	    emit_tree (out, TREE_OPERAND (node, 1));
	    fprintf (out, ")");
	    break;
	  }

	fprintf (out, "((");
	emit_tree (out, TREE_OPERAND (node, 0));
	fprintf (out, ") %s (", op);
	emit_tree (out, TREE_OPERAND (node, 1));
	fprintf (out, "))");
	break;
      }

    case COND_EXPR:
      fprintf (out, "(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, " ? ");
      emit_tree (out, TREE_OPERAND (node, 1));
      fprintf (out, " : ");
      emit_tree (out, TREE_OPERAND (node, 2));
      fprintf (out, ")");
      break;

    case VEC_COND_EXPR:
      fprintf (out, "/* vec_cond */ ((");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ") ? (");
      emit_tree (out, TREE_OPERAND (node, 1));
      fprintf (out, ") : (");
      emit_tree (out, TREE_OPERAND (node, 2));
      fprintf (out, "))");
      break;

    case CONSTRUCTOR:
      {
	fprintf (out, "{ ");
	unsigned len = CONSTRUCTOR_NELTS (node);
	vec<constructor_elt, va_gc> *elts = CONSTRUCTOR_ELTS (node);
	for (unsigned i = 0; i < len; ++i)
	  {
	    if (i) fprintf (out, ", ");
	    emit_tree (out, (*elts)[i].value);
	  }
	fprintf (out, " }");
	break;
      }

    case COMPOUND_EXPR:
      fprintf (out, "(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ", ");
      emit_tree (out, TREE_OPERAND (node, 1));
      fprintf (out, ")");
      break;

    case MODIFY_EXPR:
    case INIT_EXPR:
      fprintf (out, "(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, " = ");
      emit_tree (out, TREE_OPERAND (node, 1));
      fprintf (out, ")");
      break;

    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
      {
	const char *op = op_symbol_code (TREE_CODE (node));
	fprintf (out, "(");
	emit_tree (out, TREE_OPERAND (node, 0));
	fprintf (out, "%s)", op);
	break;
      }

    case NON_LVALUE_EXPR:
      emit_tree (out, TREE_OPERAND (node, 0));
      break;

    case SIZEOF_EXPR:
    case ALIGNOF_EXPR:
      fprintf (out, "sizeof(");
      if (TREE_OPERAND (node, 0))
	emit_tree (out, TREE_OPERAND (node, 0));
      else
	emit_type (out, TREE_TYPE (node));
      fprintf (out, ")");
      break;

    case ARROW_EXPR:
      fprintf (out, "(");
      emit_tree (out, TREE_OPERAND (node, 0));
      fprintf (out, ")/* -> */");
      break;

    case OBJ_TYPE_REF:
      emit_tree (out, TREE_OPERAND (node, 0));
      break;

    case ASSERT_EXPR:
      emit_tree (out, TREE_OPERAND (node, 0));
      break;

    case DOT_PROD_EXPR:
    case SAD_EXPR:
    case WIDEN_MULT_PLUS_EXPR:
    case WIDEN_MULT_MINUS_EXPR:
    case VEC_PACK_TRUNC_EXPR:
    case VEC_PACK_SAT_EXPR:
    case VEC_PACK_FIX_TRUNC_EXPR:
    case VEC_PACK_FLOAT_EXPR:
    case VEC_WIDEN_LSHIFT_HI_EXPR:
    case VEC_WIDEN_LSHIFT_LO_EXPR:
    case VEC_WIDEN_PLUS_HI_EXPR:
    case VEC_WIDEN_PLUS_LO_EXPR:
    case VEC_WIDEN_MINUS_HI_EXPR:
    case VEC_WIDEN_MINUS_LO_EXPR:
    case VEC_SERIES_EXPR:
    case VEC_PERM_EXPR:
    case REALIGN_LOAD_EXPR:
    case BIT_INSERT_EXPR:
    case WIDEN_SUM_EXPR:
    case WIDEN_MINUS_EXPR:
    case WIDEN_LSHIFT_EXPR:
    case WIDEN_PLUS_EXPR:
    case WIDEN_MULT_EXPR:
    case MULT_HIGHPART_EXPR:
      {
	const char *p = get_tree_code_name (TREE_CODE (node));
	fprintf (out, "/* %s */ (0", p);
	int n = TREE_OPERAND_LENGTH (node);
	for (int i = 0; i < n; ++i)
	  {
	    fprintf (out, " /* + ");
	    emit_tree (out, TREE_OPERAND (node, i));
	    fprintf (out, " */");
	  }
	fprintf (out, ")");
	break;
      }

    case CALL_EXPR:
      {
	tree fn = CALL_EXPR_FN (node);
	fprintf (out, "(");
	emit_tree (out, fn);
	fprintf (out, ")(");
	int n = call_expr_nargs (node);
	for (int i = 0; i < n; ++i)
	  {
	    if (i) fprintf (out, ", ");
	    emit_tree (out, CALL_EXPR_ARG (node, i));
	  }
	fprintf (out, ")");
	break;
      }

    case TARGET_EXPR:
      fprintf (out, "(/* target */ ");
      emit_tree (out, TREE_OPERAND (node, 1));
      fprintf (out, ")");
      break;

    case STMT_EXPR:
      fprintf (out, "(/* stmt_expr */ 0)");
      break;

    case BIND_EXPR:
      fprintf (out, "(/* bind_expr */ 0)");
      break;

    case CLEANUP_POINT_EXPR:
      emit_tree (out, TREE_OPERAND (node, 0));
      break;

    case PLACEHOLDER_EXPR:
      fprintf (out, "/* placeholder */ 0");
      break;

    case EMPTY_CLASS_EXPR:
      fprintf (out, "{}");
      break;

    case LABEL_EXPR:
      fprintf (out, "/* label */");
      break;

    default:
      fprintf (out, "(/* fallback_%s */ 0)",
	       get_tree_code_name (TREE_CODE (node)));
      break;
    }
}

/* ------------------------------------------------------------------ */
/* GIMPLE 语句输出                                                     */
/* ------------------------------------------------------------------ */

/* GIMPLE_ASSIGN 处理  */
static void
emit_gimple_assign (FILE *out, gassign *stmt)
{
  tree lhs = gimple_assign_lhs (stmt);
  enum gimple_rhs_class cls = gimple_assign_rhs_class (stmt);

  emit_tree (out, lhs);
  fprintf (out, " = ");

  switch (cls)
    {
    case GIMPLE_SINGLE_RHS:
      emit_tree (out, gimple_assign_rhs1 (stmt));
      break;
    case GIMPLE_UNARY_RHS:
      emit_unary_rhs (out, stmt);
      break;
    case GIMPLE_BINARY_RHS:
      emit_binary_rhs (out, stmt);
      break;
    case GIMPLE_TERNARY_RHS:
      emit_ternary_rhs (out, stmt);
      break;
    default:
      fprintf (out, "/* unknown rhs class %d */ 0", (int) cls);
      break;
    }

  fprintf (out, ";\n");
}

/* 单目 RHS 输出  */
static void
emit_unary_rhs (FILE *out, gassign *stmt)
{
  enum tree_code code = gimple_assign_rhs_code (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  tree lhs = gimple_assign_lhs (stmt);

  switch (code)
    {
    case FIXED_CONVERT_EXPR:
    case ADDR_SPACE_CONVERT_EXPR:
    case FIX_TRUNC_EXPR:
    case FLOAT_EXPR:
    case CONVERT_EXPR:
    case NOP_EXPR:
      fprintf (out, "((/* cast */ ");
      emit_type (out, TREE_TYPE (lhs));
      fprintf (out, ")(");
      emit_tree (out, rhs1);
      fprintf (out, "))");
      break;

    case PAREN_EXPR:
      fprintf (out, "(");
      emit_tree (out, rhs1);
      fprintf (out, ")");
      break;

    case ABS_EXPR:
      fprintf (out, "std::abs(");
      emit_tree (out, rhs1);
      fprintf (out, ")");
      break;

    case ABSU_EXPR:
      fprintf (out, "((/* absu */");
      emit_tree (out, rhs1);
      fprintf (out, ")<0?-(");
      emit_tree (out, rhs1);
      fprintf (out, "):(");
      emit_tree (out, rhs1);
      fprintf (out, "))");
      break;

    case BIT_NOT_EXPR:
      fprintf (out, "(~(");
      emit_tree (out, rhs1);
      fprintf (out, "))");
      break;

    case TRUTH_NOT_EXPR:
      fprintf (out, "(!(");
      emit_tree (out, rhs1);
      fprintf (out, "))");
      break;

    case NEGATE_EXPR:
      fprintf (out, "(-(");
      emit_tree (out, rhs1);
      fprintf (out, "))");
      break;

    case ADDR_EXPR:
      fprintf (out, "(&(");
      emit_tree (out, rhs1);
      fprintf (out, "))");
      break;

    case INDIRECT_REF:
      fprintf (out, "(*(");
      emit_tree (out, rhs1);
      fprintf (out, "))");
      break;

    case VIEW_CONVERT_EXPR:
    case ASSERT_EXPR:
      emit_tree (out, rhs1);
      break;

    default:
      if (TREE_CODE_CLASS (code) == tcc_declaration
	  || TREE_CODE_CLASS (code) == tcc_constant
	  || TREE_CODE_CLASS (code) == tcc_reference
	  || code == SSA_NAME
	  || code == ADDR_EXPR
	  || code == CONSTRUCTOR)
	{
	  emit_tree (out, rhs1);
	}
      else
	{
	  /* 其他一元运算符：直接构造一个 tree 节点不便，
	     用 emit_tree 风格输出  */
	  if (code == REALPART_EXPR)
	    {
	      fprintf (out, "__builtin_real(");
	      emit_tree (out, rhs1);
	      fprintf (out, ")");
	    }
	  else if (code == IMAGPART_EXPR)
	    {
	      fprintf (out, "__builtin_imag(");
	      emit_tree (out, rhs1);
	      fprintf (out, ")");
	    }
	  else
	    {
	      fprintf (out, "/* unary %s */ (", get_tree_code_name (code));
	      emit_tree (out, rhs1);
	      fprintf (out, ")");
	    }
	}
      break;
    }
}

/* 双目 RHS 输出  */
static void
emit_binary_rhs (FILE *out, gassign *stmt)
{
  enum tree_code code = gimple_assign_rhs_code (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  tree rhs2 = gimple_assign_rhs2 (stmt);

  if (code == MIN_EXPR)
    {
      fprintf (out, "std::min((");
      emit_tree (out, rhs1);
      fprintf (out, "), (");
      emit_tree (out, rhs2);
      fprintf (out, "))");
      return;
    }
  if (code == MAX_EXPR)
    {
      fprintf (out, "std::max((");
      emit_tree (out, rhs1);
      fprintf (out, "), (");
      emit_tree (out, rhs2);
      fprintf (out, "))");
      return;
    }

  if (code == COMPLEX_EXPR)
    {
      fprintf (out, "/* complex */ {");
      emit_tree (out, rhs1);
      fprintf (out, ", ");
      emit_tree (out, rhs2);
      fprintf (out, "}");
      return;
    }

  if (code == POINTER_PLUS_EXPR)
    {
      fprintf (out, "((/* p+ */ char*)(");
      emit_tree (out, rhs1);
      fprintf (out, ") + (");
      emit_tree (out, rhs2);
      fprintf (out, ")/*@bytes*/)");
      return;
    }

  /* 向量化/宽化运算符：用注释兜底  */
  switch (code)
    {
    case VEC_WIDEN_MULT_HI_EXPR:
    case VEC_WIDEN_MULT_LO_EXPR:
    case VEC_WIDEN_MULT_EVEN_EXPR:
    case VEC_WIDEN_MULT_ODD_EXPR:
    case VEC_PACK_TRUNC_EXPR:
    case VEC_PACK_SAT_EXPR:
    case VEC_PACK_FIX_TRUNC_EXPR:
    case VEC_PACK_FLOAT_EXPR:
    case VEC_WIDEN_LSHIFT_HI_EXPR:
    case VEC_WIDEN_LSHIFT_LO_EXPR:
    case VEC_WIDEN_PLUS_HI_EXPR:
    case VEC_WIDEN_PLUS_LO_EXPR:
    case VEC_WIDEN_MINUS_HI_EXPR:
    case VEC_WIDEN_MINUS_LO_EXPR:
    case VEC_SERIES_EXPR:
    case WIDEN_MULT_EXPR:
    case WIDEN_PLUS_EXPR:
    case WIDEN_MINUS_EXPR:
    case WIDEN_LSHIFT_EXPR:
    case WIDEN_SUM_EXPR:
    case MULT_HIGHPART_EXPR:
      {
	const char *p = get_tree_code_name (code);
	fprintf (out, "/* %s */ ((", p);
	emit_tree (out, rhs1);
	fprintf (out, ") /* op */ (");
	emit_tree (out, rhs2);
	fprintf (out, "))");
	return;
      }
    default:
      break;
    }

  const char *op = op_symbol_code (code);

  if (code == FLOOR_DIV_EXPR || code == CEIL_DIV_EXPR
      || code == ROUND_DIV_EXPR || code == EXACT_DIV_EXPR)
    op = "/";
  if (code == FLOOR_MOD_EXPR || code == CEIL_MOD_EXPR
      || code == ROUND_MOD_EXPR)
    op = "%";

  if (strcmp (op, "<<< ??? >>>") == 0)
    {
      fprintf (out, "/* unhandled binary %s */ (",
	       get_tree_code_name (code));
      emit_tree (out, rhs1);
      fprintf (out, ") ? (");
      emit_tree (out, rhs2);
      fprintf (out, ") : 0");
      return;
    }

  fprintf (out, "((");
  emit_tree (out, rhs1);
  fprintf (out, ") %s (", op);
  emit_tree (out, rhs2);
  fprintf (out, "))");
}

/* 三元 RHS 输出  */
static void
emit_ternary_rhs (FILE *out, gassign *stmt)
{
  enum tree_code code = gimple_assign_rhs_code (stmt);
  tree rhs1 = gimple_assign_rhs1 (stmt);
  tree rhs2 = gimple_assign_rhs2 (stmt);
  tree rhs3 = gimple_assign_rhs3 (stmt);

  if (code == COND_EXPR)
    {
      fprintf (out, "((");
      emit_tree (out, rhs1);
      fprintf (out, ") ? (");
      emit_tree (out, rhs2);
      fprintf (out, ") : (");
      emit_tree (out, rhs3);
      fprintf (out, "))");
      return;
    }

  if (code == VEC_COND_EXPR)
    {
      fprintf (out, "/* vec_cond */ ((");
      emit_tree (out, rhs1);
      fprintf (out, ") ? (");
      emit_tree (out, rhs2);
      fprintf (out, ") : (");
      emit_tree (out, rhs3);
      fprintf (out, "))");
      return;
    }

  if (code == BIT_INSERT_EXPR)
    {
      fprintf (out, "/* bit_insert */ (");
      emit_tree (out, rhs1);
      fprintf (out, ")");
      return;
    }

  if (code == WIDEN_MULT_PLUS_EXPR || code == WIDEN_MULT_MINUS_EXPR
      || code == DOT_PROD_EXPR || code == SAD_EXPR)
    {
      const char *p = get_tree_code_name (code);
      fprintf (out, "/* %s */ (((", p);
      emit_tree (out, rhs1);
      fprintf (out, ") * (");
      emit_tree (out, rhs2);
      fprintf (out, ")) + (");
      emit_tree (out, rhs3);
      fprintf (out, "))");
      return;
    }

  if (code == VEC_PERM_EXPR || code == REALIGN_LOAD_EXPR)
    {
      fprintf (out, "/* %s */ (", get_tree_code_name (code));
      emit_tree (out, rhs1);
      fprintf (out, ")");
      return;
    }

  fprintf (out, "/* unhandled ternary %s */ 0", get_tree_code_name (code));
}

/* GIMPLE_CALL 处理  */
static void
emit_gimple_call (FILE *out, gcall *stmt)
{
  tree lhs = gimple_call_lhs (stmt);

  if (gimple_call_internal_p (stmt))
    {
      enum internal_fn ifn = gimple_call_internal_fn (stmt);
      const char *name = internal_fn_name (ifn);
      if (lhs)
	{
	  emit_tree (out, lhs);
	  fprintf (out, " = /* IFN_%s */ 0;\n", name);
	}
      else
	fprintf (out, "/* IFN_%s */;\n", name);
      return;
    }

  if (lhs)
    {
      emit_tree (out, lhs);
      fprintf (out, " = ");
    }

  tree fndecl = gimple_call_fndecl (stmt);
  if (fndecl && DECL_NAME (fndecl))
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (fndecl));
      fprintf (out, "%s", name);
    }
  else
    {
      tree fn = gimple_call_fn (stmt);
      fprintf (out, "(");
      emit_tree (out, fn);
      fprintf (out, ")");
    }

  fprintf (out, "(");
  unsigned n = gimple_call_num_args (stmt);
  for (unsigned i = 0; i < n; ++i)
    {
      if (i) fprintf (out, ", ");
      emit_tree (out, gimple_call_arg (stmt, i));
    }
  fprintf (out, ");\n");
}

/* GIMPLE_COND 处理：if (lhs op rhs) goto true_bb; else goto false_bb;
   PHI 赋值按边分组插入。  */
static void
emit_gimple_cond (FILE *out, gcond *stmt, basic_block bb)
{
  tree lhs = gimple_cond_lhs (stmt);
  tree rhs = gimple_cond_rhs (stmt);
  enum tree_code code = gimple_cond_code (stmt);
  const char *op = op_symbol_code (code);

  edge true_edge, false_edge;
  extract_true_false_edges_from_block (bb, &true_edge, &false_edge);

  basic_block true_bb = true_edge ? true_edge->dest : NULL;
  basic_block false_bb = false_edge ? false_edge->dest : NULL;

  fprintf (out, "if ((");
  emit_tree (out, lhs);
  fprintf (out, ") %s (", op);
  emit_tree (out, rhs);
  fprintf (out, ")) {\n");

  /* true 分支的 PHI 赋值  */
  if (true_bb)
    emit_phi_assignments_for_edge (out, bb, true_bb);

  if (true_bb && true_bb != EXIT_BLOCK_PTR_FOR_FN (cfun))
    fprintf (out, "  goto bb_%d;\n", true_bb->index);
  else
    fprintf (out, "  /* true exit */\n");

  fprintf (out, "} else {\n");

  /* false 分支的 PHI 赋值  */
  if (false_bb)
    emit_phi_assignments_for_edge (out, bb, false_bb);

  if (false_bb && false_bb != EXIT_BLOCK_PTR_FOR_FN (cfun))
    fprintf (out, "  goto bb_%d;\n", false_bb->index);
  else
    fprintf (out, "  /* false exit */\n");

  fprintf (out, "}\n");
}

/* GIMPLE_RETURN 处理  */
static void
emit_gimple_return (FILE *out, greturn *stmt)
{
  tree t = gimple_return_retval (stmt);
  if (t)
    {
      fprintf (out, "return (");
      emit_tree (out, t);
      fprintf (out, ");\n");
    }
  else
    fprintf (out, "return;\n");
}

/* GIMPLE_GOTO 处理：goto bb_<dest>;
   PHI 赋值先输出（单后继）。  */
static void
emit_gimple_goto (FILE *out, ggoto *stmt)
{
  tree label = gimple_goto_dest (stmt);
  basic_block dest = label_to_block (cfun, label);
  if (dest && dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
    fprintf (out, "goto bb_%d;\n", dest->index);
  else
    {
      fprintf (out, "goto ");
      emit_label_name (out, label);
      fprintf (out, ";\n");
    }
}

/* GIMPLE_LABEL 处理：输出标签（用作 bb_<index> 的别名）  */
static void
emit_gimple_label (FILE *out, glabel *stmt)
{
  tree label = gimple_label_label (stmt);
  emit_label_name (out, label);
  fprintf (out, ":\n");
}

/* GIMPLE_SWITCH 处理  */
static void
emit_gimple_switch (FILE *out, gswitch *stmt, basic_block bb)
{
  tree index = gimple_switch_index (stmt);
  unsigned n = gimple_switch_num_labels (stmt);

  fprintf (out, "switch (");
  emit_tree (out, index);
  fprintf (out, ") {\n");
  for (unsigned i = 0; i < n; ++i)
    {
      tree elt = gimple_switch_label (stmt, i);
      tree low = CASE_LOW (elt);
      tree high = CASE_HIGH (elt);
      tree lab = CASE_LABEL (elt);
      basic_block case_bb = label_to_block (cfun, lab);

      if (low && high)
	{
	  fprintf (out, "  case ");
	  emit_tree (out, low);
	  fprintf (out, " ... ");
	  emit_tree (out, high);
	  fprintf (out, ": ");
	}
      else if (low)
	{
	  fprintf (out, "  case ");
	  emit_tree (out, low);
	  fprintf (out, ": ");
	}
      else
	fprintf (out, "  default: ");

      /* PHI 赋值  */
      if (case_bb)
	{
	  fprintf (out, "{\n");
	  emit_phi_assignments_for_edge (out, bb, case_bb);
	  if (case_bb != EXIT_BLOCK_PTR_FOR_FN (cfun))
	    fprintf (out, "    goto bb_%d;\n", case_bb->index);
	  fprintf (out, "  }\n");
	}
      else
	{
	  fprintf (out, "goto ");
	  emit_label_name (out, lab);
	  fprintf (out, ";\n");
	}
    }
  fprintf (out, "}\n");
}

/* GIMPLE_BIND 处理：嵌套作用域  */
static void
emit_gimple_bind (FILE *out, gbind *stmt)
{
  gimple_seq body = gimple_bind_body (stmt);
  gimple_stmt_iterator gsi;
  for (gsi = gsi_start (body); !gsi_end_p (gsi); gsi_next (&gsi))
    emit_gimple_stmt (out, gsi_stmt (gsi));
}

/* GIMPLE_ASM 处理  */
static void
emit_gimple_asm (FILE *out, gasm *stmt)
{
  const char *s = gimple_asm_string (stmt);
  fprintf (out, "__asm__ __volatile__(\"");
  if (s)
    {
      for (const char *p = s; *p; ++p)
	{
	  if (*p == '"' || *p == '\\')
	    fprintf (out, "\\%c", *p);
	  else if (*p == '\n')
	    fprintf (out, "\\n");
	  else if (*p == '\t')
	    fprintf (out, "\\t");
	  else
	    fputc (*p, out);
	}
    }
  fprintf (out, "\");\n");
}

/* GIMPLE 语句分发器  */
static void
emit_gimple_stmt (FILE *out, gimple *stmt)
{
  if (!stmt) return;

  switch (gimple_code (stmt))
    {
    case GIMPLE_ASSIGN:
      emit_gimple_assign (out, as_a <gassign *> (stmt));
      break;
    case GIMPLE_CALL:
      emit_gimple_call (out, as_a <gcall *> (stmt));
      break;
    case GIMPLE_COND:
      /* GIMPLE_COND 需要 BB 上下文以提取 true/false 边，
	 由 emit_bb_terminator 单独处理  */
      fprintf (out, "/* cond handled elsewhere */\n");
      break;
    case GIMPLE_RETURN:
      emit_gimple_return (out, as_a <greturn *> (stmt));
      break;
    case GIMPLE_GOTO:
      /* GIMPLE_GOTO 通常由 emit_bb_terminator 处理（需 PHI 赋值）。
	 此处仅在 GIMPLE_BIND 体内出现时作为兜底（不含 PHI 赋值）。  */
      emit_gimple_goto (out, as_a <ggoto *> (stmt));
      break;
    case GIMPLE_LABEL:
      emit_gimple_label (out, as_a <glabel *> (stmt));
      break;
    case GIMPLE_SWITCH:
      fprintf (out, "/* switch handled elsewhere */\n");
      break;
    case GIMPLE_BIND:
      emit_gimple_bind (out, as_a <gbind *> (stmt));
      break;
    case GIMPLE_ASM:
      /* ASM 也作为 terminator 处理（避免 PHI 赋值与 asm 顺序问题）  */
      fprintf (out, "/* asm handled elsewhere */\n");
      break;
    case GIMPLE_NOP:
      break;
    case GIMPLE_DEBUG:
      break;
    case GIMPLE_PHI:
      break;
    case GIMPLE_PREDICT:
      break;
    case GIMPLE_EH_DISPATCH:
      fprintf (out, "/* eh_dispatch */\n");
      break;
    case GIMPLE_RESX:
      fprintf (out, "/* resx */ /* throw */\n");
      break;
    case GIMPLE_TRY:
      fprintf (out, "/* gimple_try */\n");
      break;
    case GIMPLE_CATCH:
      fprintf (out, "/* gimple_catch */\n");
      break;
    case GIMPLE_EH_FILTER:
      fprintf (out, "/* eh_filter */\n");
      break;
    case GIMPLE_TRANSACTION:
      fprintf (out, "/* transaction */\n");
      break;
    case GIMPLE_OMP_PARALLEL:
    case GIMPLE_OMP_TASK:
    case GIMPLE_OMP_FOR:
    case GIMPLE_OMP_SECTIONS:
    case GIMPLE_OMP_SINGLE:
    case GIMPLE_OMP_CRITICAL:
    case GIMPLE_OMP_ATOMIC_LOAD:
    case GIMPLE_OMP_ATOMIC_STORE:
    case GIMPLE_OMP_RETURN:
    case GIMPLE_OMP_CONTINUE:
    case GIMPLE_OMP_SECTIONS_SWITCH:
    case GIMPLE_OMP_MASTER:
    case GIMPLE_OMP_TASKGROUP:
    case GIMPLE_OMP_SECTION:
      fprintf (out, "/* omp stmt: %s */\n",
	       gimple_code_name[gimple_code (stmt)]);
      break;
    default:
      fprintf (out, "/* unsupported gimple code: %d */\n",
	       (int) gimple_code (stmt));
      break;
    }
}

/* ------------------------------------------------------------------ */
/* PHI 节点策略 A                                                      */
/* ------------------------------------------------------------------ */

/* 输出从 pred 到 succ 这条边上应执行的 phi 赋值。
   即：对于 succ BB 中的每个非 virtual PHI，
   找到对应 pred 边的 arg，输出 result = arg;  */
static void
emit_phi_assignments_for_edge (FILE *out, basic_block pred,
			       basic_block succ)
{
  if (succ == NULL || succ == EXIT_BLOCK_PTR_FOR_FN (cfun))
    return;

  for (gphi_iterator gsi = gsi_start_phis (succ);
       !gsi_end_p (gsi);
       gsi_next (&gsi))
    {
      gphi *phi = gsi.phi ();
      tree result = gimple_phi_result (phi);
      if (virtual_operand_p (result))
	continue;

      unsigned nargs = gimple_phi_num_args (phi);
      for (unsigned i = 0; i < nargs; ++i)
	{
	  edge e = gimple_phi_arg_edge (phi, i);
	  if (e->src == pred)
	    {
	      tree def = gimple_phi_arg_def (phi, i);
	      fprintf (out, "  ");
	      emit_tree (out, result);
	      fprintf (out, " = ");
	      emit_tree (out, def);
	      fprintf (out, ";\n");
	      break;
	    }
	}
    }
}

/* 判断 stmt 是否是 BB 的终结语句  */
static bool
is_terminator_p (gimple *stmt)
{
  if (!stmt) return false;
  enum gimple_code gc = gimple_code (stmt);
  return (gc == GIMPLE_GOTO
	  || gc == GIMPLE_COND
	  || gc == GIMPLE_RETURN
	  || gc == GIMPLE_SWITCH
	  || gc == GIMPLE_RESX
	  || gc == GIMPLE_ASM);
}

/* 输出 BB 的所有非终结语句（不含 PHI 与 terminator）。
   返回 terminator 语句（若存在），否则 NULL。  */
static gimple *
emit_bb_non_terminator_stmts (FILE *out, basic_block bb)
{
  gimple *terminator = NULL;
  gimple_stmt_iterator gsi;
  for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
    {
      gimple *stmt = gsi_stmt (gsi);
      enum gimple_code gc = gimple_code (stmt);

      /* 跳过 PHI（已在前驱 BB 处理）  */
      if (gc == GIMPLE_PHI)
	continue;

      /* 跳过 DEBUG  */
      if (gc == GIMPLE_DEBUG)
	continue;

      if (is_terminator_p (stmt))
	{
	  terminator = stmt;
	  /* terminator 不在此处输出  */
	  continue;
	}

      /* 普通语句：直接调用 emit_gimple_stmt。
	 但 emit_gimple_stmt 内部会输出 "\n"，前导缩进需手动加。  */
      fprintf (out, "  ");
      emit_gimple_stmt (out, stmt);
    }
  return terminator;
}

/* 输出 BB 终结语句（含前驱边 PHI 赋值）  */
static void
emit_bb_terminator (FILE *out, basic_block bb)
{
  gimple *terminator = NULL;
  /* 重新找 terminator  */
  gimple_stmt_iterator gsi;
  for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi))
    {
      gimple *stmt = gsi_stmt (gsi);
      if (gimple_code (stmt) == GIMPLE_PHI) continue;
      if (gimple_code (stmt) == GIMPLE_DEBUG) continue;
      if (is_terminator_p (stmt))
	{
	  terminator = stmt;
	  break;
	}
    }

  fprintf (out, "  ");

  if (terminator)
    {
      enum gimple_code gc = gimple_code (terminator);
      switch (gc)
	{
	case GIMPLE_COND:
	  emit_gimple_cond (out, as_a <gcond *> (terminator), bb);
	  break;
	case GIMPLE_RETURN:
	  /* return 边到 EXIT，无 PHI  */
	  emit_gimple_return (out, as_a <greturn *> (terminator));
	  break;
	case GIMPLE_GOTO:
	  {
	    ggoto *g = as_a <ggoto *> (terminator);
	    tree label = gimple_goto_dest (g);
	    basic_block dest = label_to_block (cfun, label);
	    /* PHI 赋值先输出  */
	    if (dest && dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
	      emit_phi_assignments_for_edge (out, bb, dest);
	    fprintf (out, "goto ");
	    if (dest && dest != EXIT_BLOCK_PTR_FOR_FN (cfun))
	      fprintf (out, "bb_%d", dest->index);
	    else
	      emit_label_name (out, label);
	    fprintf (out, ";\n");
	    break;
	  }
	case GIMPLE_SWITCH:
	  emit_gimple_switch (out, as_a <gswitch *> (terminator), bb);
	  break;
	case GIMPLE_RESX:
	  fprintf (out, "/* resx */ /* throw */\n");
	  break;
	case GIMPLE_ASM:
	  emit_gimple_asm (out, as_a <gasm *> (terminator));
	  break;
	default:
	  fprintf (out, "/* unknown terminator */\n");
	  break;
	}
    }
  else
    {
      /* 无 terminator：可能是 fall-through  */
      /* 输出 PHI 赋值（针对单后继）  */
      int n_succ = bb->succs ? bb->succs->length () : 0;
      if (n_succ >= 1)
	{
	  edge e = (*bb->succs)[0];
	  basic_block succ = e->dest;
	  emit_phi_assignments_for_edge (out, bb, succ);

	  /* 若 fall-through 标志未设置或目标非直接后继，
	     补 goto  */
	  if (succ == EXIT_BLOCK_PTR_FOR_FN (cfun))
	    {
	      /* 到 exit：无需 goto（函数自然结束），
		 但若非 fall-through 需 return  */
	      if (!(e->flags & EDGE_FALLTHRU))
		fprintf (out, "return; /* to exit */\n");
	    }
	  else if (e->flags & EDGE_FALLTHRU
		   && succ->index == bb->index + 1)
	    {
	      /* 自然 fall-through 到下一个 BB：无需 goto  */
	    }
	  else
	    {
	      fprintf (out, "goto bb_%d;\n", succ->index);
	    }
	}
    }
}

/* ------------------------------------------------------------------ */
/* 单函数输出                                                          */
/* ------------------------------------------------------------------ */

/* 输出单个函数  */
static void
emit_function (FILE *out, tree fndecl)
{
  if (!fndecl || TREE_CODE (fndecl) != FUNCTION_DECL)
    return;

  struct function *fn = DECL_STRUCT_FUNCTION (fndecl);
  if (!fn || !fn->cfg || !gimple_has_body_p (fndecl))
    return;

  push_cfun (fn);

  /* 函数属性  */
  if (TREE_PUBLIC (fndecl) && DECL_EXTERNAL (fndecl))
    fprintf (out, "extern ");
  else if (TREE_STATIC (fndecl))
    fprintf (out, "static ");
  else if (!TREE_PUBLIC (fndecl))
    fprintf (out, "static ");

  /* 返回类型  */
  tree fntype = TREE_TYPE (fndecl);
  tree ret_type = TREE_TYPE (fntype);
  if (ret_type == NULL_TREE)
    ret_type = void_type_node;
  emit_type (out, ret_type);

  /* 函数名  */
  fprintf (out, " ");
  tree name = DECL_NAME (fndecl);
  if (name && IDENTIFIER_POINTER (name))
    fprintf (out, "%s", IDENTIFIER_POINTER (name));
  else if (DECL_ASSEMBLER_NAME_SET_P (fndecl))
    {
      const char *s = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (fndecl));
      for (const char *p = s; *p; ++p)
	{
	  if ((ISALNUM (*p) && *p != '.') || *p == '_')
	    fputc (*p, out);
	  else
	    fputc ('_', out);
	}
    }
  else
    fprintf (out, "_anon_fn_%p", (void *) fndecl);

  /* 参数列表  */
  fprintf (out, "(");
  tree arg = DECL_ARGUMENTS (fndecl);
  bool first = true;
  for (; arg; arg = DECL_CHAIN (arg))
    {
      if (!first) fprintf (out, ", ");
      first = false;
      tree atype = TREE_TYPE (arg);
      if (TREE_CODE (atype) == ARRAY_TYPE)
	{
	  /* C 风格：T x[] 转为 T* x  */
	  emit_type (out, TREE_TYPE (atype));
	  fprintf (out, "*");
	}
      else
	emit_type (out, atype);
      fprintf (out, " ");
      emit_decl_name (out, arg);
    }
  if (first && TREE_CODE (fntype) == METHOD_TYPE)
    {
      tree cls = TYPE_METHOD_BASETYPE (fntype);
      emit_type (out, cls);
      fprintf (out, "* /* this */");
      first = false;
    }
  if (first)
    fprintf (out, "void");
  fprintf (out, ")\n");

  /* 函数体开始  */
  fprintf (out, "{\n");

  /* 预声明所有 SSA 名  */
  if (gimple_in_ssa_p (fn))
    {
      unsigned ix;
      tree ssa;
      FOR_EACH_SSA_NAME (ix, ssa, fn)
	{
	  if (!ssa) continue;
	  if (virtual_operand_p (ssa))
	    continue;
	  /* 跳过默认定义  */
	  if (SSA_NAME_IS_DEFAULT_DEF (ssa))
	    {
	      tree var = SSA_NAME_VAR (ssa);
	      if (var
		  && (TREE_CODE (var) == PARM_DECL
		      || TREE_CODE (var) == RESULT_DECL
		      || TREE_CODE (var) == VAR_DECL)
		  && DECL_NAME (var)
		  && IDENTIFIER_POINTER (DECL_NAME (var)))
		continue;
	    }
	  tree st = TREE_TYPE (ssa);
	  if (st == NULL_TREE)
	    st = void_type_node;
	  fprintf (out, "  ");
	  emit_type (out, st);
	  fprintf (out, " ");
	  emit_ssa_name (out, ssa);
	  fprintf (out, ";\n");
	}
    }

  /* 输出局部变量（非 SSA）  */
  if (fn->local_decls && !vec_safe_is_empty (fn->local_decls))
    {
      unsigned ix;
      tree var;
      FOR_EACH_LOCAL_DECL (fn, ix, var)
	{
	  if (!var || TREE_CODE (var) != VAR_DECL) continue;
	  fprintf (out, "  ");
	  tree vt = TREE_TYPE (var);
	  if (TREE_CODE (vt) == ARRAY_TYPE)
	    {
	      tree elem = TREE_TYPE (vt);
	      tree dom = TYPE_DOMAIN (vt);
	      emit_type (out, elem);
	      fprintf (out, " ");
	      emit_decl_name (out, var);
	      if (dom)
		{
		  tree max = TYPE_MAX_VALUE (dom);
		  if (max && TREE_CODE (max) == INTEGER_CST)
		    {
		      unsigned HOST_WIDE_INT sz = tree_to_uhwi (max) + 1;
		      tree min = TYPE_MIN_VALUE (dom);
		      if (min && TREE_CODE (min) == INTEGER_CST
			  && tree_to_uhwi (min) != 0)
			sz -= tree_to_uhwi (min);
		      fprintf (out, "[%" PRIu64 "]", (uint64_t) sz);
		    }
		  else
		    fprintf (out, "[]");
		}
	      else
		fprintf (out, "[]");
	    }
	  else
	    {
	      emit_type (out, vt);
	      fprintf (out, " ");
	      emit_decl_name (out, var);
	    }
	  if (DECL_INITIAL (var)
	      && TREE_CODE (DECL_INITIAL (var)) != ERROR_MARK)
	    {
	      fprintf (out, " = ");
	      emit_tree (out, DECL_INITIAL (var));
	    }
	  fprintf (out, ";\n");
	}
    }

  fprintf (out, "\n");

  /* 遍历 CFG，输出每个 BB  */
  basic_block bb;
  FOR_EACH_BB_FN (bb, fn)
    {
      /* BB 标签  */
      fprintf (out, "bb_%d:;\n", bb->index);

      /* 输出非终结语句  */
      emit_bb_non_terminator_stmts (out, bb);

      /* 输出终结语句（含 PHI 赋值）  */
      emit_bb_terminator (out, bb);

      fprintf (out, "\n");
    }

  fprintf (out, "}\n\n");

  pop_cfun ();
}

/* ------------------------------------------------------------------ */
/* 全局变量输出                                                        */
/* ------------------------------------------------------------------ */

/* 输出单个全局变量  */
static void
emit_global_variable (FILE *out, varpool_node *vnode)
{
  if (!vnode) return;
  tree decl = vnode->decl;
  if (!decl || TREE_CODE (decl) != VAR_DECL) return;

  /* 属性  */
  if (TREE_PUBLIC (decl) && DECL_EXTERNAL (decl))
    fprintf (out, "extern ");
  else if (TREE_STATIC (decl))
    fprintf (out, "static ");

  tree vt = TREE_TYPE (decl);
  if (TREE_CODE (vt) == ARRAY_TYPE)
    {
      tree elem = TREE_TYPE (vt);
      tree dom = TYPE_DOMAIN (vt);
      emit_type (out, elem);
      fprintf (out, " ");
      emit_decl_name (out, decl);
      if (dom)
	{
	  tree max = TYPE_MAX_VALUE (dom);
	  if (max && TREE_CODE (max) == INTEGER_CST)
	    {
	      unsigned HOST_WIDE_INT sz = tree_to_uhwi (max) + 1;
	      tree min = TYPE_MIN_VALUE (dom);
	      if (min && TREE_CODE (min) == INTEGER_CST
		  && tree_to_uhwi (min) != 0)
		sz -= tree_to_uhwi (min);
	      fprintf (out, "[%" PRIu64 "]", (uint64_t) sz);
	    }
	  else
	    fprintf (out, "[]");
	}
      else
	fprintf (out, "[]");
    }
  else
    {
      emit_type (out, vt);
      fprintf (out, " ");
      emit_decl_name (out, decl);
    }

  /* 初始值  */
  if (DECL_INITIAL (decl)
      && TREE_CODE (DECL_INITIAL (decl)) != ERROR_MARK
      && !DECL_EXTERNAL (decl))
    {
      fprintf (out, " = ");
      emit_tree (out, DECL_INITIAL (decl));
    }
  fprintf (out, ";\n");
}

/* ------------------------------------------------------------------ */
/* 主入口                                                              */
/* ------------------------------------------------------------------ */

void
gimple_to_cpp_emit_all (FILE *file)
{
  if (!file) return;

  /* 文件头注释  */
  fprintf (file,
	   "/* Generated by GCC -fo3-pre-expand.  "
	   "Do not edit.  */\n");
  fprintf (file,
	   "/* This C++20 source encodes the program state after all O3 "
	   "GIMPLE passes.  */\n\n");

  /* 标准 include  */
  fprintf (file, "#include <cstdint>\n");
  fprintf (file, "#include <cstdlib>\n");
  fprintf (file, "#include <cstdio>\n");
  fprintf (file, "#include <cstring>\n");
  fprintf (file, "#include <algorithm>\n");
  fprintf (file, "#include <utility>\n");
  fprintf (file, "#include <cmath>\n");
  fprintf (file, "#include <new>\n\n");

  /* 提供 std::isnan / std::isinf 等所需 <cmath> 已包含。
     提供 _Complex 等内置支持无需 include。  */

  /* 输出全局变量  */
  fprintf (file, "/* ===== Global variables ===== */\n\n");
  varpool_node *vnode;
  FOR_EACH_DEFINED_VARIABLE (vnode)
    emit_global_variable (file, vnode);
  fprintf (file, "\n");

  /* 输出所有函数  */
  fprintf (file, "/* ===== Functions ===== */\n\n");
  cgraph_node *cnode;
  FOR_EACH_FUNCTION_WITH_GIMPLE_BODY (cnode)
    emit_function (file, cnode->decl);

  fprintf (file, "/* End of generated code.  */\n");
}

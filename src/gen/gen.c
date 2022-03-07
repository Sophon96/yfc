#include "gen.h"

#include <stdarg.h>
#include <stdio.h>

#include <api/abstract-tree.h>
#include <api/operator.h>
#include <gen/typegen.h>
#include <util/yfc-out.h>

static void yf_gen_program(struct yfa_program * node, FILE * out);
static void yf_gen_vardecl(struct yfa_vardecl * node, FILE * out);
static void yf_gen_funcdecl(struct yfa_funcdecl * node, FILE * out);
static void yf_gen_expr(struct yfa_expr * node, FILE * out);
static void yf_gen_bstmt(struct yfa_bstmt * node, FILE * out);
static void yf_gen_return(struct yfa_return * node, FILE * out);
static void yf_gen_if(struct yfa_if * node, FILE * out);

/* TODO - non-static this, maybe by putting in a struct or something. */
static int yf_dump_indent = 0;

static void indent() { ++yf_dump_indent; }
static void dedent() { --yf_dump_indent; }

static void yfg_print_line(FILE * out, char * data, ...) {
    int i;
    va_list args;
    va_start(args, data);
    vfprintf(out, data, args);
    fprintf(out, "\n");
    for (i = 0; i < yf_dump_indent; i++) {
        fprintf(out, "\t");
    }
    va_end(args);
}

void yf_gen_node(struct yf_ast_node * root, FILE *out) {

    switch (root->type) {
        case YFA_PROGRAM:
            yf_gen_program(&root->program, out);
            break;
        case YFA_VARDECL:
            yf_gen_vardecl(&root->vardecl, out);
            break;
        case YFA_FUNCDECL:
            yf_gen_funcdecl(&root->funcdecl, out);
            break;
        case YFA_EXPR:
            yf_gen_expr(&root->expr, out);
            break;
        case YFA_BSTMT:
            yf_gen_bstmt(&root->bstmt, out);
            break;
        case YFA_RETURN:
            yf_gen_return(&root->ret, out);
            break;
        case YFA_IF:
            yf_gen_if(&root->ifstmt, out);
            break;
        case YFA_EMPTY:
            fprintf(out, ";\n");
            break;
    }

}

static void yf_gen_program(struct yfa_program * node, FILE *out) {

    struct yf_ast_node * child;

    YF_LIST_FOREACH(node->decls, child) {
        yf_gen_node(child, out);
        if (child->type == YFA_VARDECL)
            yfg_print_line(out, ";");
        else
            yfg_print_line(out, "");
    }

}

static void yf_gen_vardecl(struct yfa_vardecl * node, FILE * out) {
    char typebuf[256];
    yfg_ctype(256, typebuf, node->name->var.dtype);
    fprintf(
        out, "%s /* %s */ %s",
        typebuf,
        node->name->var.dtype->name,
        node->name->var.name
    );
    if (node->expr) {
        fprintf(out, " = ");
        yf_gen_node(node->expr, out);
    }
}

static void yf_gen_funcdecl(struct yfa_funcdecl * node, FILE * out) {

    struct yf_ast_node * child;
    int argct = 0;
    char typebuf[256];
    yfg_ctype(256, typebuf, node->name->fn.rtype);

    fprintf(
        out, "%s /* %s */ %s",
        typebuf,
        node->name->fn.rtype->name,
        node->name->fn.name
    );
    fprintf(out, "(");

    /* Generate param list */

    YF_LIST_FOREACH(node->params, child) {
        if (argct)
            fprintf(out, ", ");
        if (!child) break;
        yf_gen_node(child, out);
        ++argct;
    }

    fprintf(out, ") ");

    yf_gen_node(node->body, out);

}

static void yf_gen_expr(struct yfa_expr * node, FILE * out) {

    struct yf_ast_node * call_arg;
    int argct;

    /* All expressions are surrounded in parens so C's operator precedence
     * is ignored. */
    fprintf(out, "(");

    switch (node->type) {
        case YFA_VALUE:
            switch (node->as.value.type) {
                case YFA_LITERAL:
                    fprintf(out, "%d", node->as.value.as.literal.val);
                    break;
                case YFA_IDENT:
                    fprintf(out, "%s", node->as.value.as.identifier->var.name);
                    break;
            }
            break;
        case YFA_BINARY:
            yf_gen_expr(node->as.binary.left, out);
            fprintf(out, " %s ", get_op_string(node->as.binary.op));
            yf_gen_expr(node->as.binary.right, out);
            break;
        case YFA_FUNCCALL:
            fprintf(out, "%s(", node->as.call.name->fn.name);
            argct = 0;
            YF_LIST_FOREACH(node->as.call.args, call_arg) {
                if (argct)
                    fprintf(out, ", ");
                if (!call_arg) break;
                yf_gen_node(call_arg, out);
                ++argct;
            }
            fprintf(out, ")");
            break;
    }

    fprintf(out, ")");

}

static void yf_gen_bstmt(struct yfa_bstmt * node, FILE * out) {
    struct yf_ast_node * child;
    fprintf(out, "{");
    indent();
    YF_LIST_FOREACH(node->stmts, child) {
        yfg_print_line(out, "");
        yf_gen_node(child, out);
        fprintf(out, ";");
    }
    dedent();
    yfg_print_line(out, "");
    fprintf(out, "}");
}

static void yf_gen_return(struct yfa_return * node, FILE * out) {
    fprintf(out, "return ");
    if (node->expr)
        yf_gen_node(node->expr, out);
}

static void yf_gen_if(struct yfa_if * node, FILE * out) {
    fprintf(out, "if (");
    yf_gen_node(node->cond, out);
    yfg_print_line(out, ") {");
        yf_gen_node(node->code, out);
    yfg_print_line(out, ";");
    fprintf(out, "}");
    if (node->elsebranch) {
        yfg_print_line(out, " else {");
        yf_gen_node(node->elsebranch, out);
        yfg_print_line(out, ";");
        fprintf(out, "}");
    }
}

int yfg_gen(struct yf_compile_analyse_job * data) {

    FILE * out;
    out = fopen(data->unit_info->output_file, "w");
    if (!out) {
        YF_PRINT_ERROR("could not open output file %s", data->unit_info->output_file);
        return 1;
    }

    fprintf(out, "/* Generated by yfc. */\n\n");
    fprintf(out, "#include <stdint.h>\n\n");

    yf_gen_node(&data->ast_tree, out);

    fclose(out);
    return 0;

}

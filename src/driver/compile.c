#include "compile.h"

#include <stdio.h> /* fopen, etc. */
#include <stdlib.h> /* malloc */

#include <api/compilation-data.h>
#include <api/lexer-input.h>
#include <driver/compile.h>
#include <parser/parser.h>

/* Forward decls for whole file */
static int yf_compile_project(struct yf_args *);
static int yf_compile_files(struct yf_args *);
static int yf_run_frontend(struct yf_file_compilation_data *);

/**
 * This is it. This is the actual compile function for a set of arguments. It
 * just defers compilation to one of two functions, depending on whether
 * --project is enabled or not.
 */
int yf_run_compiler(struct yf_args * args) {
    
    if (args->project) {
        return yf_compile_project(args);
    } else {
        return yf_compile_files(args);
    }

}

static int yf_compile_project(struct yf_args * args) {
    
    struct yf_individual_compilation_data data;
    int i;

    for (i = 0; i < args->num_files; ++i) {
        data.files[i] = malloc(sizeof (struct yf_file_compilation_data));
        data.files[i]->file_name = args->files[i];
        /* TODO - more data */
    }
    data.num_files = args->num_files;

    /* Parse the frontend for all */
    for (i = 0; i < args->num_files; ++i) {
        yf_run_frontend(data.files[i]);
    }

    /* TODO - semantic analysis, code gen */

}

static int yf_compile_files(struct yf_args * args) {
    /* TODO */
    return 0;
}

/**
 * Run the lexing and parsing on one file.
 */
static int yf_run_frontend(struct yf_file_compilation_data * file) {

    struct yf_lexer_input input;
    struct yf_lexer lexer;
    
    input = (struct yf_lexer_input) {
        .input = fopen(file->file_name, "r"),
        .getc = (int (*)(void*)) getc,
        .ungetc = (int (*)(int, void*)) ungetc
    };

    yfl_init(&lexer, &input);

    return yf_parse(&lexer, &file->parse_tree);

}

#include <R.h>
#include <Rinternals.h>
#include <ctype.h>
#include "trackinghash.h"
#include "alike.h"

// - Objects We Install Once ---------------------------------------------------

// One question: are static objects not garbage collected?  The examples from
// "Writing R Extensions" don't seem to require the protection of these

#ifndef VALC_MAX_CHAR
#define VALC_MAX_CHAR 10000
#endif

#ifndef _VETR_H
#define  VETR_H

  SEXP VALC_SYM_one_dot;
  SEXP VALC_SYM_deparse;
  SEXP VALC_SYM_paren;
  SEXP VALC_SYM_quote;
  SEXP VALC_SYM_current;
  SEXP VALC_TRUE;
  SEXP VALC_SYM_errmsg;

  // QUESTION, WHAT TYPE SHOULD ALL THE NUMBERS HERE BE, LONG? THAT WOULD SEEM
  // TO MAKE SENSE

  struct VALC_settings {

    // Original alike settings

    int type_mode, attr_mode, lang_mode, fun_mode;
    int fuzzy_int_max_len;
    int suppress_warnings;
    const char * prepend;     // no longer in use

    int in_attr;
    int width;      // Tell alike what screen width to assume
    int env_limit;  // how many envs to track when searching for env loop

    // what env to look for functions to match call in, substitute, etc, used boty
    // by alike and by vet funs

    SEXP env;

    // Additional vet settings

    long max_nchar;       // when do we stop looking for NULL?
    long max_sub_depth;   // how deep do we allow recursive substitution to go?
    long max_symbol_size;
    long track_hash_content_size;
  };

  SEXP VALC_validate(
    SEXP target, SEXP current, SEXP cur_sub, SEXP par_call, SEXP rho,
    SEXP ret_mode_sxp, SEXP stop
  );

  SEXP VALC_validate_args(SEXP fun, SEXP fun_call, SEXP val_call, SEXP fun_frame);
  SEXP VALC_remove_parens(SEXP lang);
  SEXP VALC_name_sub_ext(SEXP symb, SEXP arg_name);
  SEXP VALC_test(SEXP a, SEXP b);
  SEXP VALC_test1(SEXP a);
  SEXP VALC_test2(SEXP a, SEXP b);
  void VALC_stop(SEXP call, const char * msg);
  void VALC_stop2(SEXP call, const char * msg, SEXP rho);
  SEXP VALC_all_ext(SEXP vec);
  int VALC_all(SEXP vec);
  int IS_TRUE(SEXP x);
  int IS_LANG(SEXP x);
  SEXP VALC_parse(SEXP lang, SEXP var_name, SEXP rho);
  void VALC_parse_recurse(
    SEXP lang, SEXP lang_track, SEXP var_name, SEXP rho, int eval_as_is,
    SEXP first_fun, struct track_hash * track_hash
  );
  SEXP VALC_sub_symbol(SEXP lang, SEXP rho, struct track_hash * track_hash);
  SEXP VALC_sub_symbol_ext(SEXP lang, SEXP rho);
  void VALC_install_objs();
  SEXP VALC_evaluate(
    SEXP lang, SEXP arg_lang, SEXP arg_tag, SEXP arg_value, SEXP lang_full,
    SEXP rho
  );
  void VALC_arg_error(SEXP tag, SEXP fun_call, const char * err_base);
  void psh(const char * lab);

#endif

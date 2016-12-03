#include "validate.h"

/* -------------------------------------------------------------------------- *\
\* -------------------------------------------------------------------------- */
/*
 * @param arg_lang the substituted language corresponding to the argument
 * @param arg_tag the argument name
 */

SEXP VALC_evaluate_recurse(
  SEXP lang, SEXP act_codes, SEXP arg_value, SEXP arg_lang, SEXP arg_tag,
  SEXP lang_full, SEXP rho
) {
  /*
  check act_codes:
    if 1 or 2
      recurse and:
        if return value is TRUE
          and act_code == 2, return TRUE
          and act_code == 1, continue
        if return value is character
          and act_code == 1,
            return
          and act_code == 2,
            record for later return if no TRUEs are met
        if return value is TRUE,
        if with mode set to corresponding value (does it matter)
    if 10, eval as is
      if returns character then return character
      if returns FALSE deparse into something like (`x` does not eval to TRUE`)
    if 999, eval as alike
  */
  int mode;

  if(TYPEOF(act_codes) == LISTSXP) {
    if(TYPEOF(lang) != LANGSXP && TYPEOF(lang) != LISTSXP) {
      error("Logic Error: mismatched language and eval type tracking 1; contact maintainer.");
    }
    if(TYPEOF(CAR(act_codes)) != INTSXP) {
      if(TYPEOF(CAR(lang)) != LANGSXP) {
        error("Logic error: call should have been evaluated at previous level; contact maintainer.");
      }
      if(TYPEOF(CAR(act_codes)) != LISTSXP || TYPEOF(CAAR(act_codes)) != INTSXP) {
        error("Logic error: unexpected act_code structure; contact maintainer");
      }
      mode=asInteger(CAAR(act_codes));
    } else {
      mode=asInteger(CAR(act_codes));
    }
  } else {
    if(TYPEOF(lang) == LANGSXP || TYPEOF(lang) == LISTSXP) {
      error("Logic Error: mismatched language and eval type tracking 2; contact maintainer.");
    }
    mode = asInteger(act_codes);
  }
  if(mode == 1 || mode == 2) {
    // Dealing with && or ||, so recurse on each element

    if(TYPEOF(lang) == LANGSXP) {
      int parse_count = 0;
      SEXP err_list = PROTECT(allocList(0)); // Track errors
      SEXP eval_res;
      lang = CDR(lang);
      act_codes = CDR(act_codes);

      while(lang != R_NilValue) {
        eval_res = PROTECT(
          VALC_evaluate_recurse(
            CAR(lang), CAR(act_codes), arg_value, arg_lang, arg_tag, lang_full,
            rho
        ) );
        if(TYPEOF(eval_res) == LISTSXP) {
          if(mode == 1) {// At least one non-TRUE result, which is a failure, so return
            UNPROTECT(2);
            return(eval_res);
          }
          if(mode == 2)  {// All need to fail, so store errors for now
            err_list = listAppend(err_list, eval_res);
          }
        } else if (VALC_all(eval_res) > 0) {
          if(mode == 2) {
            UNPROTECT(2);
            return(ScalarLogical(1)); // At least one TRUE value in or mode
          }
        } else {
          error("Logic Error: unexpected return value when recursively evaluating parse; contact maintainer.");
        }
        lang = CDR(lang);
        act_codes = CDR(act_codes);
        parse_count++;
        UNPROTECT(1);
      }
      if(parse_count != 2)
        error("Logic Error: unexpected language structure for modes 1/2; contact maintainer.");
      UNPROTECT(1);
      if(mode == 2) {  // Only way to get here is if none of previous actually returned TRUE and mode is OR
        return(err_list);
      }
      return(eval_res);  // Or if all returned TRUE and mode is AND
    } else {
      error("Logic Error: in mode c(1, 2), but not a language object; contact maintainer.");
    }
  } else if(mode == 10 || mode == 999) {
    SEXP eval_res, eval_tmp;
    int err_val = 0, eval_res_c;
    int * err_point = &err_val;
    eval_tmp = PROTECT(R_tryEval(lang, rho, err_point));
    if(* err_point) {
      VALC_arg_error(
        arg_tag, lang_full,
        "Validation expression for argument `%s` produced an error (see previous error)."
      );
    }
    if(mode == 10) {
      eval_res_c = VALC_all(eval_tmp);
      eval_res = PROTECT(ScalarLogical(eval_res_c > 0));
    } else {
      eval_res = PROTECT(VALC_alike(eval_tmp, arg_value, arg_lang, rho));
    }
    // Sanity checks

    if(
      (TYPEOF(eval_res) != LGLSXP && TYPEOF(eval_res) != STRSXP) ||
      XLENGTH(eval_res) != 1L || (
        TYPEOF(eval_res) == LGLSXP && asInteger(eval_res) == NA_INTEGER
      )
    )
      error("Logic error: token eval must be TRUE, FALSE, or character(1L), contact maintainer (is type: %s)", type2char(TYPEOF(eval_res)));

    // Note we're handling both user exp and template eval here

    if(
      TYPEOF(eval_res) != LGLSXP || !asLogical(eval_res)
    ) {
      SEXP err_msg = PROTECT(allocList(1));
      // mode == 10 is user eval, special treatment to produce err msg

      if(mode == 10) {
        // Tried to do this as part of init originally so we don't have to repeat
        // wholesale creation of call, but the call elements kept getting over
        // written by other stuff.  Not sure how to protect in calls defined at
        // top level

        SEXP err_attrib;
        const char * err_call;

        SEXP dep_call = PROTECT(allocList(2));
        SETCAR(dep_call, VALC_SYM_deparse);
        SEXP quot_call = PROTECT(allocList(2));
        SETCAR(quot_call, VALC_SYM_quote);
        SETCADR(quot_call, lang);
        SET_TYPEOF(quot_call, LANGSXP);
        SETCADR(dep_call, quot_call);
        SET_TYPEOF(dep_call, LANGSXP);
        err_call = CHAR(STRING_ELT(eval(dep_call, rho), 0));  // Could span multiple lines...; need to address

        // If message attribute defined, this is easy:

        if((err_attrib = getAttrib(lang, VALC_SYM_errmsg)) != R_NilValue) {
          if(TYPEOF(err_attrib) != STRSXP || XLENGTH(err_attrib) != 1) {
            error(
              "\"err.msg\" attribute for `%s` token must be a one length character vector",
              err_call
          );}
          SETCAR(err_msg, err_attrib);
        } else {
          // message attribute not defined, must construct error message based
          // on result of evaluation

          char * err_str;
          char * err_tok;
          switch(eval_res_c) {
            case -2: {
                const char * err_tok_tmp = type2char(TYPEOF(eval_tmp));
                const char * err_tok_base = "is \"%s\" instead of a \"logical\"";
                err_tok = R_alloc(
                  strlen(err_tok_tmp) + strlen(err_tok_base), sizeof(char)
                );
                if(sprintf(err_tok, err_tok_base, err_tok_tmp) < 0)
                  error("Logic error: could not build token error; contact maintainer");
              }
              break;
            case -1: err_tok = "FALSE"; break;
            case -3: err_tok = "NA"; break;
            case -4: err_tok = "contains NAs"; break;
            case -5: err_tok = "zero length"; break;
            case 0: err_tok = "contains non-TRUE values"; break;
            default:
              error("Logic Error: unexpected user exp eval value %d; contact maintainer.", eval_res_c);
          }
          const char * err_extra_a = "is not all TRUE";
          const char * err_extra_b = "is not TRUE"; // must be shorter than _a
          const char * err_extra;
          if(eval_res_c == 0) {
            err_extra = err_extra_a;
          } else {
            err_extra = err_extra_b;
          }
          const char * err_base = "`%s` %s (%s)";
          err_str = R_alloc(
            strlen(err_call) + strlen(err_base) + strlen(err_tok) +
            strlen(err_extra), sizeof(char)
          );
          // not sure why we're not using cstringr here
          if(sprintf(err_str, err_base, err_call, err_extra, err_tok) < 0)
            error("Logic Error: could not construct error message; contact maintainer.");
          SETCAR(err_msg, mkString(err_str));
        }
        UNPROTECT(2);
      } else { // must have been `alike` eval
        SETCAR(err_msg, eval_res);
      }
      UNPROTECT(3);
      return(err_msg);
    }
    UNPROTECT(2);
    return(eval_res);  // this should be `TRUE`
  } else {
    error("Logic Error: unexpected parse mode %d", mode);
  }
  error("Logic Error: should never get here");
  return(R_NilValue);  // Should never get here
}
/* -------------------------------------------------------------------------- *\
\* -------------------------------------------------------------------------- */
/*
TBD if this should call `VALC_parse` directly or not

@param lang the validator expression
@param arg_lang the substituted language being validated
@param arg_tag the variable name being validated
@param arg_value the value being validated
@param lang_full solely so that we can produce error message with original call
@param rho the environment in which to evaluate the validation function
*/
SEXP VALC_evaluate(
  SEXP lang, SEXP arg_lang, SEXP arg_tag, SEXP arg_value, SEXP lang_full,
  SEXP rho
) {
  if(!IS_LANG(arg_lang))
    error("Argument `arg_lang` must be language.");
  if(TYPEOF(rho) != ENVSXP)
    error("Argument `rho` must be an environment.");
  SEXP lang_parsed = PROTECT(VALC_parse(lang, arg_lang, rho));
  SEXP res = PROTECT(
    VALC_evaluate_recurse(
      VECTOR_ELT(lang_parsed, 0),
      VECTOR_ELT(lang_parsed, 1),
      arg_value, arg_lang, arg_tag, lang_full, rho
  ) );
  // Remove duplicates, if any

  switch(TYPEOF(res)) {
    case LGLSXP:  break;
    case LISTSXP: {
      SEXP res_cpy, res_next, res_next_next, res_val;
      for(
        res_cpy = res; res_cpy != R_NilValue;
        res_cpy = res_next
      ) {
        res_next = CDR(res_cpy);
        res_val = CAR(res_cpy);
        if(TYPEOF(res_val) != STRSXP)
          error("Logic Error: non string value in return pairlist; contact maintainer.");
        if(res_next == R_NilValue) break;
        if(R_compute_identical(CAR(res_next), res_val, 16)) { // Need to remove res_next
          res_next_next = CDR(res_next);
          SETCDR(res_cpy, res_next_next);
      } }
    } break;
    default:
      error("Logic Error: unexpected evaluating return type; contact maintainer.");
  }
  UNPROTECT(2);  // This seems a bit stupid, PROTECT/UNPROTECT
  return(res);
}

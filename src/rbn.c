#include "common.h"

void rbn_discrete_root(SEXP result, int cur, SEXP cpt, int *num, int ordinal,
    SEXP fixed);
void rbn_discrete_cond(SEXP result, SEXP nodes, int cur, SEXP parents, SEXP cpt,
    int *num, int ordinal, SEXP fixed);
void rbn_gaussian(SEXP result, int cur, SEXP parents, SEXP coefs, SEXP sigma,
    int *num, SEXP fixed);

#define CATEGORICAL      1
#define ORDINAL          2
#define GAUSSIAN         3

/* generate random observations from a bayesian network. */
SEXP rbn_master(SEXP fitted, SEXP n, SEXP fix, SEXP debug) {

int debuglevel = isTRUE(debug);
SEXP result;

  /* allocate the return value. */
  PROTECT(result = fit2df(fitted, INT(n)));
  /* perform the simulation. */
  c_rbn_master(fitted, result, n, fix, debuglevel);

  UNPROTECT(1);

  return result;

}/*RBN_MASTER*/

void c_rbn_master(SEXP fitted, SEXP result, SEXP n, SEXP fix, int debuglevel) {

int *num = INTEGER(n), *poset = NULL, *mf = NULL, type = 0;
int has_fixed = (TYPEOF(fix) != LGLSXP);
int i = 0, k = 0, cur = 0, nnodes = length(fitted), nparents = 0;
const char *cur_class = NULL;
SEXP nodes, roots, node_depth, cpt, coefs, sd, parents, parent_vars;
SEXP cur_node, cur_fixed, match_fixed;

  /* allocate and initialize the return value. */
  nodes = getAttrib(fitted, R_NamesSymbol);

  /* order the nodes according to their depth in the graph. */
  PROTECT(roots = root_nodes(fitted, FALSESEXP));
  PROTECT(node_depth = schedule(fitted, roots, FALSESEXP, FALSESEXP));
  poset = alloc1dcont(nnodes);
  for (i = 0; i < nnodes; i++)
    poset[i] = i;
  R_qsort_int_I(INTEGER(node_depth), poset, 1, nnodes);

  /* unprotect roots and node_depth, they are not needed any more. */
  UNPROTECT(2);

  /* match fixed nodes, if any, with the variables in the fitted network. */
  if (has_fixed) {

    PROTECT(match_fixed = match(getAttrib(fix, R_NamesSymbol), nodes, 0));
    mf = INTEGER(match_fixed);

  }/*THEN*/

  if (debuglevel > 0) {

    Rprintf("* partial node ordering is:");

    for (i = 0; i < nnodes; i++)
      Rprintf(" %s", NODE(poset[i]));

    Rprintf(".\n");

  }/*THEN*/

  /* initialize the random number generator. */
  GetRNGstate();

  for (i = 0; i < nnodes; i++) {

    /* get the index of the node we have to generate random observations from,
     * its conditional probability table/regression parameters and the number
     * of its parents. */
    cur = poset[i];
    cur_node = VECTOR_ELT(fitted, cur);
    cur_class = CHAR(STRING_ELT(getAttrib(cur_node, R_ClassSymbol), 0));
    parents = getListElement(cur_node, "parents");
    nparents = length(parents);

    /* check whether the value of the node is fixed, and if so retrieve it from
     * the list. */
    if (has_fixed && mf[cur] != 0)
      cur_fixed = VECTOR_ELT(fix, mf[cur] - 1);
    else
      cur_fixed = R_NilValue;

    /* find out whether the node corresponds to an ordered factor or not. */
    if (strcmp(cur_class, "bn.fit.onode") == 0) {

      cpt = getListElement(cur_node, "prob");
      type = ORDINAL;

    }/*THEN*/
    else if (strcmp(cur_class, "bn.fit.dnode") == 0) {

      cpt = getListElement(cur_node, "prob");
      type = CATEGORICAL;

    }/*THEN*/
    else if (strcmp(cur_class, "bn.fit.gnode") == 0) {

      coefs = getListElement(cur_node, "coefficients");
      sd = getListElement(cur_node, "sd");
      type = GAUSSIAN;

    }/*THEN*/

    /* generate the random observations for the current node. */
    if (nparents == 0) {

      if (debuglevel > 0) {

        if (cur_fixed != R_NilValue)
          Rprintf("* node %s is fixed.\n", NODE(cur));
        else
          Rprintf("* simulating node %s, which doesn't have any parent.\n",
            NODE(cur));

      }/*THEN*/

      switch(type) {

        case CATEGORICAL:
          rbn_discrete_root(result, cur, cpt, num, FALSE, cur_fixed);
          break;

        case ORDINAL:
          rbn_discrete_root(result, cur, cpt, num, TRUE, cur_fixed);
          break;

        case GAUSSIAN:
          rbn_gaussian(result, cur, NULL, coefs, sd, num, cur_fixed);
          break;

      }/*SWITCH*/

    }/*THEN*/
    else {

      if (debuglevel > 0) {

        if (cur_fixed != R_NilValue) {

          Rprintf("* node %s is fixed, ignoring parents.\n", NODE(cur));

        }/*THEN*/
        else {

          Rprintf("* simulating node %s with parents ", NODE(cur));
          for (k = 0; k < nparents - 1; k++)
            Rprintf("%s, ", CHAR(STRING_ELT(parents, k)));
          Rprintf("%s.\n", CHAR(STRING_ELT(parents, nparents - 1)));

        }/*ELSE*/

      }/*THEN*/

      PROTECT(parent_vars = dataframe_column(result, parents, FALSESEXP));

      switch(type) {

        case CATEGORICAL:
          rbn_discrete_cond(result, nodes, cur, parent_vars, cpt, num, FALSE, cur_fixed);
          break;

        case ORDINAL:
          rbn_discrete_cond(result, nodes, cur, parent_vars, cpt, num, TRUE, cur_fixed);
          break;

        case GAUSSIAN:
          rbn_gaussian(result, cur, parent_vars, coefs, sd, num, cur_fixed);
          break;

      }/*SWITCH*/

      UNPROTECT(1);

    }/*ELSE*/

  }/*FOR*/

  PutRNGstate();

  UNPROTECT(has_fixed);

}/*C_RBN_MASTER*/

void rbn_discrete_fixed(SEXP fixed, SEXP lvls, int *gen, int *num) {

  if (length(fixed) == 1) {

    int constant = 0;

    /* fixed can be either a label to be matched with the factor's levels or
     * the corresponding numeric index, which can be used as it is. */
    if (TYPEOF(fixed) == INTSXP)
      constant = INT(fixed);
    else
      constant = INT(match(lvls, fixed, 0));

    for (int i = 0; i < *num; i++)
      gen[i] = constant;

  }/*THEN*/
  else {

    SEXP fixed_levels;

    /* fixed is a set of labels here. */
    PROTECT(fixed_levels = match(lvls, fixed, 0));
    SampleReplace(*num, length(fixed_levels), gen, INTEGER(fixed_levels));
    UNPROTECT(1);

  }/*ELSE*/

}/*RBN_DISCRETE_FIXED*/

/* unconditional discrete sampling. */
void rbn_discrete_root(SEXP result, int cur, SEXP cpt, int *num, int ordinal,
    SEXP fixed) {

int np = length(cpt), *gen = NULL, *workplace = NULL;
double *p = NULL;
SEXP generated, lvls;

  /* get the levels of the curent variable .*/
  lvls = VECTOR_ELT(getAttrib(cpt, R_DimNamesSymbol), 0);
  /* get the column for the generated observations. */
  generated = VECTOR_ELT(result, cur);
  gen = INTEGER(generated);

  if (fixed != R_NilValue) {

    rbn_discrete_fixed(fixed, lvls, gen, num);

  }/*THEN*/
  else {

    workplace = alloc1dcont(np);

    /* duplicate the probability table to save the original copy from tampering. */
    p = alloc1dreal(np);
    memcpy(p, REAL(cpt), np * sizeof(double));

    /* perform the random sampling. */
    ProbSampleReplace(np, p, workplace, *num, gen);

  }/*ELSE*/

}/*RBN_DISCRETE_ROOT*/

/* conditional discrete sampling. */
void rbn_discrete_cond(SEXP result, SEXP nodes, int cur, SEXP parents, SEXP cpt,
    int *num, int ordinal, SEXP fixed) {

int np = length(cpt), nlevels = 0, warn = 0;
int *workplace = NULL, *configurations = NULL, *gen = NULL;
double *p = NULL;
SEXP generated, lvls;

  /* get the number of levels of the curent variable .*/
  lvls = VECTOR_ELT(getAttrib(cpt, R_DimNamesSymbol), 0);
  nlevels = length(lvls);
  /* get the column for the generated observations. */
  generated = VECTOR_ELT(result, cur);
  gen = INTEGER(generated);

  if (fixed != R_NilValue) {

    rbn_discrete_fixed(fixed, lvls, gen, num);

  }/*THEN*/
  else {

    workplace = alloc1dcont(np);

    /* allocate and initialize the parents' configurations. */
    configurations = alloc1dcont(*num);
    cfg(parents, configurations, NULL);

    /* duplicate the probability table to save the original copy from tampering. */
    p = alloc1dreal(np);
    memcpy(p, REAL(cpt), np * sizeof(double));
    /* perform the random sampling. */
    CondProbSampleReplace(nlevels, length(cpt)/nlevels, p, configurations,
      workplace, *num, gen, &warn);

  }/*ELSE*/

  /* warn when returning missing values. */
  if (warn == TRUE)
    warning("some configurations of the parents of %s are not present in the original data. NAs will be generated.",
      NODE(cur));

}/*RBN_DISCRETE_COND*/

/* conditional and unconditional normal sampling. */
void rbn_gaussian(SEXP result, int cur, SEXP parents, SEXP coefs, SEXP sigma,
    int *num, SEXP fixed) {

int i = 0, j = 0, p = length(coefs);
double *beta = REAL(coefs), *sd = REAL(sigma), *gen = NULL, *Xj = NULL;
SEXP generated;

  /* get the column for the generated observations. */
  generated = VECTOR_ELT(result, cur);
  gen = REAL(generated);

  if (fixed != R_NilValue) {

    double *constant = REAL(fixed);

    if (length(fixed) == 1) {

      /* conditioning on a single value. */
      for (i = 0; i < *num; i++)
        gen[i] = constant[0];

    }/*THEN*/
    else {

      double offset = constant[0], range = constant[1] - constant[0];

      /* conditioning on an interval, picking a value at random
       * from a uniform distribution. */
      for (i = 0; i < *num; i++)
        gen[i] = offset + unif_rand() * range;

    }/*ELSE*/

  }/*THEN*/
  else {

    /* initialize with intercept and standard error. */
    for (i = 0; i < *num; i++)
      gen[i] = beta[0] + norm_rand() * (*sd);

    /* add the contributions of the other regressors (if any). */
    for (j = 1; j < p; j++) {

      Xj = REAL(VECTOR_ELT(parents, j - 1));

      for (i = 0; i < *num; i++)
        gen[i] += Xj[i] * beta[j];

    }/*FOR*/

  }/*ELSE*/

}/*RBN_GAUSSIAN*/


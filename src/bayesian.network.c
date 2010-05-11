#include "common.h"

/* macro for the number of levels of the j-th node. */
#define BNLEARN_NLEVELS(j) \
  LENGTH(getAttrib(VECTOR_ELT(data, j), R_LevelsSymbol))

/* get the number of parameters of the whole network (discrete case). */
SEXP nparams_dnet(SEXP graph, SEXP data, SEXP real, SEXP debug) {

int i = 0, j = 0, nnodes = 0, node_params = 0;
int *nlvls = NULL, *index = NULL, *res = NULL;
int *r = LOGICAL(real), *debuglevel = LOGICAL(debug);
SEXP nodes, node_data, parents, try, result;

  /* get nodes' number and data. */
  node_data = getListElement(graph, "nodes");
  nodes = getAttrib(node_data, R_NamesSymbol);
  nnodes = LENGTH(node_data);

  /* get the level count for each node. */
  nlvls = alloc1dcont(nnodes);
  for (i = 0; i < nnodes; i++)
    nlvls[i] = BNLEARN_NLEVELS(i);

  /* allocate and initialize the return value. */
  PROTECT(result = allocVector(INTSXP, 1));
  res = INTEGER(result);
  res[0] = 0;

  /* for each node... */
  for (i = 0; i < nnodes; i++) {

    /* reset the parameter counter. */
    node_params = 1;

    /* match the parents of the node. */
    parents = getListElement(VECTOR_ELT(node_data, i), "parents");
    PROTECT(try = match(nodes, parents, 0));
    index = INTEGER(try);

    /* compute the number of configurations. */
    for (j = 0; j < LENGTH(try); j++)
      node_params *= nlvls[index[j] - 1];

    UNPROTECT(1);

    /* multiply by the number of free parameters. */
    if (*r > 0)
      node_params *= nlvls[i] - 1;
    else
      node_params *= nlvls[i];

    if (*debuglevel > 0)
      Rprintf("* node %s has %d parameter(s).\n", NODE(i), node_params);

    /* update the return value. */
    res[0] += node_params;

  }/*FOR*/

  UNPROTECT(1);

  return result;

}/*NPARAMS_DNET*/

/* get the number of parameters of a single node (discrete case). */
SEXP nparams_dnode(SEXP graph, SEXP node, SEXP data, SEXP real) {

int i = 0, j = 0, length_nodes = 0;
int *nlevels = NULL, *r = LOGICAL(real);
SEXP temp, names, result;

  /* get the entry for the parents of the node.*/
  temp = getListElement(graph, "nodes");
  temp = getListElement(temp, (char *)CHAR(STRING_ELT(node, 0)));
  temp = getListElement(temp, "parents");

  /* get the column names from the data set and the length of the
       relevant vectors. */
  names = getAttrib(data, R_NamesSymbol);
  length_nodes = LENGTH(temp);

  /* allocate and initialize the result. */
  PROTECT(result = allocVector(INTSXP, 1));
  nlevels = INTEGER(result);
  *nlevels = 1;

  /* sum (multiply, actually) up the levels. */
  for (i = 0; i < LENGTH(names); i++) {

    for (j = 0; j < length_nodes; j++) {

      /* this is a parent. */
      if (!strcmp(CHAR(STRING_ELT(names, i)), CHAR(STRING_ELT(temp, j)))) {

        *nlevels *= BNLEARN_NLEVELS(i);

      }/*THEN*/

    }/*FOR*/

    /* this is the node. */
    if (!strcmp(CHAR(STRING_ELT(names, i)), CHAR(STRING_ELT(node, 0)))) {

      *nlevels *= BNLEARN_NLEVELS(i) - 1 * (*r);

    }/*THEN*/

  }/*FOR*/

  UNPROTECT(1);

return result;

}/*NPARAMS_DNODE*/

/* get the number of parameters of the whole network (continuous case). */
SEXP nparams_gnet(SEXP graph, SEXP debug) {

int i = 0, node_params = 0, *res = NULL, *debuglevel = LOGICAL(debug);
SEXP result, nodes = R_NilValue, temp = getListElement(graph, "nodes");

  /* allocate and initialize the result. */
  PROTECT(result = allocVector(INTSXP, 1));
  res = INTEGER(result);
  res[0] = 0;

  if (*debuglevel > 0) 
    nodes = getAttrib(temp, R_NamesSymbol);

  /* add one parameter for each regressor, which means one for each
   * parent for each node plus the intercept. */
  for (i = 0; i < LENGTH(temp); i++) {

    node_params = LENGTH(getListElement(VECTOR_ELT(temp, i), "parents")) + 1;

    if (*debuglevel > 0)
      Rprintf("* node %s has %d parameter(s).\n", NODE(i), node_params);

    /* update the return value. */
    res[0] += node_params;

  }/*FOR*/

  UNPROTECT(1);

  return(result);

}/*NPARAMS_GNET*/

/* get the number of parameters of a single node (continuous case). */
SEXP nparams_gnode(SEXP graph, SEXP node) {

char *name = (char *)CHAR(STRING_ELT(node, 0));
SEXP temp, result;

  temp = getListElement(graph, "nodes");
  temp = getListElement(temp, name);
  temp = getListElement(temp, "parents");

  PROTECT(result = allocVector(INTSXP, 1));
  INT(result) = LENGTH(temp) + 1;
  UNPROTECT(1);

  return result;

}/*NPARAMS_GNODE*/

/* convert a set of neighbourhoods into an arc set. */
SEXP nbr2arcs(SEXP nbr) {

int i = 0, j = 0, k = 0, narcs = 0;
int length_names = 0;
SEXP arcs, dimnames, colnames, temp, names;

  /* get the names of the nodes. */
  names = getAttrib(nbr, R_NamesSymbol);
  length_names = LENGTH(names);

  /* scan the structure to determine the number of arcs.  */
  for (i = 0; i < length_names; i++) {

    /* get the entry for the neighbours of the node.*/
    temp = getListElement(nbr, (char *)CHAR(STRING_ELT(names, i)));
    temp = getListElement(temp, "nbr");

    narcs += LENGTH(temp);

  }/*FOR*/

  /* allocate colnames. */
  PROTECT(dimnames = allocVector(VECSXP, 2));
  PROTECT(colnames = allocVector(STRSXP, 2));
  SET_STRING_ELT(colnames, 0, mkChar("from"));
  SET_STRING_ELT(colnames, 1, mkChar("to"));
  SET_VECTOR_ELT(dimnames, 1, colnames);

  /* if there are no arcs, return an empty arc set. */
  if (narcs == 0) {

    /* allocate an empty arc set. */
    PROTECT(arcs = allocMatrix(STRSXP, 0, 2));
    /* set the column names. */
    setAttrib(arcs, R_DimNamesSymbol, dimnames);

    UNPROTECT(3);

    return arcs;

  }/*THEN*/
  else {

    /* allocate the arc set. */
    PROTECT(arcs = allocMatrix(STRSXP, narcs, 2));
    /* set the column names. */
    setAttrib(arcs, R_DimNamesSymbol, dimnames);

  }/*ELSE*/

  /* rescan the structure to build the arc set. */
  for (i = 0; i < length_names; i++) {

    /* get the entry for the neighbours of the node.*/
    temp = getListElement(nbr, (char *)CHAR(STRING_ELT(names, i)));
    temp = getListElement(temp, "nbr");

    for (j = 0; j < LENGTH(temp); j++) {

      SET_STRING_ELT(arcs, k, STRING_ELT(names, i));
      SET_STRING_ELT(arcs, k + 1 * narcs , STRING_ELT(temp, j));
      k++;

    }/*FOR*/

  }/*FOR*/

  UNPROTECT(3);

return arcs;

}/*NBR2ARCS*/


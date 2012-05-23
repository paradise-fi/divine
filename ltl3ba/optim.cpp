/***** ltl3ba : optim.c *****/

/* Written by Tomas Babiak, FI MU, Brno, Czech Republic                   */
/* Copyright (c) 2012  Tomas Babiak                                       */
/*                                                                        */

#include "ltl3ba.h"

#define false 0
#define true 1

int is_G(Node *n) {
  if (!n) return false;
  
  if (n->ntyp == V_OPER && n->lft && n->lft->ntyp == FALSE) return true;
  else return false;
}

int is_F(Node *n) {
  if (!n) return false;
  
  if (n->ntyp == U_OPER && n->lft && n->lft->ntyp == TRUE) return true;
  else return false;
}

int is_Falpha(Node *n) {
  if (!n) return false;
      
  if (is_F(n) && is_LTL(n->rgt))
    return true;
  else
    return false;
}

int is_UXp(Node *n) {
  if (!n) return false;
  
  if (n->ntyp == U_OPER
#ifdef NXT
      || n->ntyp == NEXT
#endif
      || is_LTL(n)) return true;
  else return false;
}

// Checks whether it is a fromula of LTL() fragment (boolean combination of APs)
int is_LTL(Node *n) {
  if (!n) return false;
        
  switch(n->ntyp) {
  case OR:  
  case AND:
    return is_LTL(n->lft) && is_LTL(n->rgt);
    break;
  case U_OPER:
  case V_OPER:
    return false;
    break;
#ifdef NXT
  case NEXT:
    return false;
    break;
#endif
  case NOT:
    return is_LTL(n->lft);
    break;
  case FALSE:
  case TRUE:
  case PREDICATE:
    return true;
    break;
  default:
    printf("Unknown token: ");
    tl_explain(n->ntyp);
    break;
  }
}

int is_EVE(Node *n) {
  if (!n) return false;
        
  if (is_F(n)) return true;
        
  switch(n->ntyp) {
  case OR:  
  case AND:
    return is_EVE(n->lft) && is_EVE(n->rgt);
    break;
  case U_OPER:
    return is_EVE(n->rgt);
    break;
  case V_OPER:
    if (is_G(n))
      return is_EVE(n->rgt);
    else
      return is_EVE(n->lft) && is_EVE(n->rgt);
    break;
#ifdef NXT
  case NEXT:
    return is_EVE(n->lft);
    break;
#endif
  case NOT:
    return is_EVE(n->lft);
    break;
  case FALSE:
  case TRUE:
  case PREDICATE:
    return false;
    break;
  default:
    printf("Unknown token: ");
    tl_explain(n->ntyp);
    break;
  }
}

int is_UNI(Node *n) {
  if (!n) return false;
        
  if (is_G(n)) return true;
        
  switch(n->ntyp) {
  case OR:  
  case AND:
    return is_UNI(n->lft) && is_UNI(n->rgt);
    break;
  case U_OPER:
    if (is_F(n))
      return is_UNI(n->rgt);
    else
      return is_UNI(n->lft) && is_UNI(n->rgt);
    break;
  case V_OPER:
    return is_UNI(n->rgt);
    break;
#ifdef NXT
  case NEXT:
    return is_UNI(n->lft);
    break;
#endif
  case NOT:
    return is_UNI(n->lft);
    break;
  case FALSE:
  case TRUE:
  case PREDICATE:
    return false;
    break;
  default:
    printf("Unknown token: ");
    tl_explain(n->ntyp);
    break;
  }
}

int is_FIN(Node *n) {
  if (!n) return false;
        
  switch(n->ntyp) {
  case OR:  
  case AND:
    return is_FIN(n->lft) && is_FIN(n->rgt);
    break;
  case U_OPER:
    return is_FIN(n->lft) && is_FIN(n->rgt);
    break;
  case V_OPER:
    return false;
    break;
#ifdef NXT
  case NEXT:
    return is_FIN(n->lft);
    break;
#endif
  case NOT:
    return is_FIN(n->lft);
    break;
  case FALSE:
  case TRUE:
  case PREDICATE:
    return true;
    break;
  default:
    printf("Unknown token: ");
    tl_explain(n->ntyp);
    break;
  }
}

int is_INFp(Node *n) {
  if (!n) return false;
        
  switch(n->ntyp) {
  case OR:  
  case AND:
    return is_INFp(n->lft) && is_INFp(n->rgt);
    break;
  case U_OPER:
    if (is_F(n))
      return is_UNI(n->rgt);
    else 
      return is_INFp(n->rgt);
    break;
  case V_OPER:
    if (is_G(n))
      return is_EVE(n->rgt);
    else
      return is_INFp(n->rgt);
    break;
#ifdef NXT
  case NEXT:
    return is_INFp(n->lft);
    break;
#endif
  case NOT:
    return is_INFp(n->lft);
    break;
  case FALSE:
  case TRUE:
  case PREDICATE:
    return false;
    break;
  default:
    printf("Unknown token: ");
    tl_explain(n->ntyp);
    break;
  }
}

int is_INFd(Node *n) {
  if (!n) return false;
        
  if (is_INFp(n)) return true;
        
  switch(n->ntyp) {
  case OR:
    return false;
    break;  
  case AND:
    return is_INFd(n->lft) && is_INFd(n->rgt);
    break;
  case U_OPER:
    return false;
    break;
  case V_OPER:
    if (is_G(n)) {
      if (is_LTL(n->rgt))
        return true;
      else 
        return is_INFd(n->rgt);
    } else
        return false;
    break;
#ifdef NXT
  case NEXT:
    return false;
    break;
#endif
  case NOT:
    return is_INFd(n->lft);
    break;
  case FALSE:
  case TRUE:
  case PREDICATE:
    return false;
    break;
  default:
    printf("Unknown token: ");
    tl_explain(n->ntyp);
    break;
  }
}

int is_GF_inside(Node *n) {
  if (!n) return false;

  switch(n->ntyp) {
  case OR:
    return is_LTL(n->lft) && is_LTL(n->rgt);
    break;  
  case AND:
    return is_GF_inside(n->lft) && is_GF_inside(n->rgt);
    break;
  case U_OPER:
    if (is_F(n) && is_LTL(n->rgt))
      return true;
    else return false;
    break;
  case V_OPER:
    return false;
    break;
#ifdef NXT
  case NEXT:
    return false;
    break;
#endif
  case NOT:
    return is_GF_inside(n->lft);
    break;
  case FALSE:
  case TRUE:
  case PREDICATE:
    return true;
    break;
  default:
    printf("Unknown token: ");
    tl_explain(n->ntyp);
    break;
  }
}

int is_GF_component(Node *n) {
  if (!n) return false;
  
  if (!is_G(n)) return false;
  
  if (is_GF_inside(n->rgt)) return true;
    else return false;
}

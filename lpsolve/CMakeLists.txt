add_definitions( -static -Bsymbolic )
include_directories( . bfp bfp/bfp_LUSOL bfp/bfp_LUSOL/LUSOL colamd shared )

add_definitions( -DYY_NEVER_INTERACTIVE -DPARSER_LP -DINVERSE_ACTIVE=INVERSE_LUSOL -DRoleIsExternalInvEngine )

# LFLAGS = -L -l
# YFLAGS = --no-lines -y
# LDFLAGS = -lm -ldl

add_library( lpsolve STATIC
  bfp/bfp_LUSOL/lp_LUSOL.c bfp/bfp_LUSOL/LUSOL/lusol.c

  lp_rlp.l lp_rlp.y

  lp_MDO.c shared/commonlib.c colamd/colamd.c
  shared/mmio.c shared/myblas.c ini.c fortify.c lp_rlp.c lp_crash.c
  lp_Hash.c lp_lib.c lp_wlp.c lp_matrix.c lp_mipbb.c lp_MPS.c
  lp_params.c lp_presolve.c lp_price.c lp_pricePSE.c lp_report.c
  lp_scale.c lp_simplex.c lp_SOS.c lp_utils.c yacc_read.c )

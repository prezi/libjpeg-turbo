
#define JPEG_INTERNALS
#include "jinclude.h"
#include "jpeglib.h"
#include "jdhuff.h"

#if defined(D_MULTISCAN_FILES_SUPPORTED) && defined(LOWMEM_PROGRESSIVE_DECODE)

// this should be enough - increase if necessary
#define MAX_SCANS 16

/* Private state */
struct jpeg_scan_context {
  size_t f_offset;
  size_t bytes_in_buffer;
  int comps_in_scan;
  jpeg_component_info * cur_comp_info[MAX_COMPS_IN_SCAN];
  int dc_tbl_no[MAX_COMPS_IN_SCAN];		/* DC entropy table selector (0..3) */
  int ac_tbl_no[MAX_COMPS_IN_SCAN];		/* AC entropy table selector (0..3) */
  int Ss, Se, Ah, Al;		/* progressive JPEG parameters for scan */
  int unread_marker;
  entropy_scan_context entropy;
  JDIMENSION input_iMCU_row;
  JHUFF_TBL hufftables[NUM_HUFF_TBLS];
};

typedef struct {
  struct jpeg_scan_context_controller pub; /* public fields */

  struct jpeg_scan_context scan_contexts[MAX_SCANS];
} my_scan_context_controller;

typedef my_scan_context_controller * my_scancontextctl_ptr;

METHODDEF(void)
save_scan_state(j_decompress_ptr cinfo, int scan_no)
{
  boolean is_DC_band = (cinfo->Ss == 0);
  my_scancontextctl_ptr my_scan_context = (my_scancontextctl_ptr)cinfo->scancontextctl;
  struct jpeg_scan_context* context;
  int i;

  if (scan_no>=MAX_SCANS) 
    ERREXIT1(cinfo, JERR_MAX_SCANS_EXCEEDED, MAX_SCANS);	/* safety check */

  context = &my_scan_context->scan_contexts[scan_no];
  context->f_offset = (*cinfo->src->get_src_offset)(cinfo);
  context->bytes_in_buffer = cinfo->src->bytes_in_buffer;
  context->comps_in_scan = cinfo->comps_in_scan;
  for (i=0;i<cinfo->comps_in_scan;i++) {
    context->cur_comp_info[i]=cinfo->cur_comp_info[i];
    context->dc_tbl_no[i]=cinfo->cur_comp_info[i]->dc_tbl_no;
    context->ac_tbl_no[i]=cinfo->cur_comp_info[i]->ac_tbl_no;
  }
  context->Ss=cinfo->Ss;
  context->Se=cinfo->Se;
  context->Ah=cinfo->Ah;
  context->Al=cinfo->Al;
  context->input_iMCU_row=cinfo->input_iMCU_row;
  context->unread_marker = cinfo->unread_marker;
  save_entropy_state(cinfo,&context->entropy);
  for (i=0;i<NUM_HUFF_TBLS;i++) {
    if (is_DC_band) {
      if (cinfo->dc_huff_tbl_ptrs[i]) {
    MEMCOPY(&context->hufftables[i],cinfo->dc_huff_tbl_ptrs[i],sizeof(JHUFF_TBL));
      } else {
    MEMZERO(&context->hufftables[i],sizeof(JHUFF_TBL));
      }
    } else {
      if (cinfo->ac_huff_tbl_ptrs[i]) {
    MEMCOPY(&context->hufftables[i],cinfo->ac_huff_tbl_ptrs[i],sizeof(JHUFF_TBL));
      } else {
    MEMZERO(&context->hufftables[i],sizeof(JHUFF_TBL));
      }
    }
  }
}

METHODDEF(void)
restore_scan_state_pre(j_decompress_ptr cinfo, int scan_no)
{
  my_scancontextctl_ptr my_scan_context = (my_scancontextctl_ptr)cinfo->scancontextctl;
  struct jpeg_scan_context* context = &my_scan_context->scan_contexts[scan_no];
  boolean is_DC_band = (context->Ss == 0);
  int i;
 
  (*cinfo->src->restore_src)(cinfo,context->f_offset,context->bytes_in_buffer);
  cinfo->comps_in_scan = context->comps_in_scan;
  for (i=0;i<cinfo->comps_in_scan;i++) {
    cinfo->cur_comp_info[i]=context->cur_comp_info[i];
    cinfo->cur_comp_info[i]->dc_tbl_no=context->dc_tbl_no[i];
    cinfo->cur_comp_info[i]->ac_tbl_no=context->ac_tbl_no[i];
  }
  cinfo->Ss=context->Ss;
  cinfo->Se=context->Se;
  cinfo->Ah=context->Ah;
  cinfo->Al=context->Al;
  cinfo->unread_marker = context->unread_marker;
  for (i=0;i<NUM_HUFF_TBLS;i++) {
    if (is_DC_band) {
      if (cinfo->dc_huff_tbl_ptrs[i]) {
    MEMCOPY(cinfo->dc_huff_tbl_ptrs[i],&context->hufftables[i],sizeof(JHUFF_TBL));
      }
    } else {
      if (cinfo->ac_huff_tbl_ptrs[i]) {
    MEMCOPY(cinfo->ac_huff_tbl_ptrs[i],&context->hufftables[i],sizeof(JHUFF_TBL));
      }
    }
  }
}

METHODDEF(void)
restore_scan_state_post(j_decompress_ptr cinfo, int scan_no)
{
  my_scancontextctl_ptr my_scan_context = (my_scancontextctl_ptr)cinfo->scancontextctl;
  struct jpeg_scan_context* context = &my_scan_context->scan_contexts[scan_no];
  cinfo->input_iMCU_row=context->input_iMCU_row;

  restore_entropy_state(cinfo,&context->entropy);
}

#endif /* D_MULTISCAN_FILES_SUPPORTED && LOWMEM_PROGRESSIVE_DECODE */

/*
 * Initialize the scan context controller module.
 */

GLOBAL(void)
jinit_scan_context_controller (j_decompress_ptr cinfo)
{
#if defined(D_MULTISCAN_FILES_SUPPORTED) && defined(LOWMEM_PROGRESSIVE_DECODE)
  my_scancontextctl_ptr scancontextctl;

  /* Create subobject in permanent pool */
  scancontextctl = (my_scancontextctl_ptr)
    (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				sizeof(my_scan_context_controller));
  cinfo->scancontextctl = (struct jpeg_scan_context_controller *) scancontextctl;
  /* Initialize method pointers */
  scancontextctl->pub.save_scan_state = save_scan_state;
  scancontextctl->pub.restore_scan_state_pre = restore_scan_state_pre;
  scancontextctl->pub.restore_scan_state_post = restore_scan_state_post;
 #else
  ERREXIT(cinfo, JERR_NOT_COMPILED);
 #endif /* D_MULTISCAN_FILES_SUPPORTED && LOWMEM_PROGRESSIVE_DECODE */
}


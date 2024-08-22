#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <errno.h>

#define CAML_NAME_SPACE
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/signals.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <caml/custom.h>

#include <xen-tools/common-macros.h>

#include <string.h>

#define XC_WANT_COMPAT_MAP_FOREIGN_API
#include <xenctrl.h>

#define ERR_MSG_LEN (XC_MAX_ERROR_MSG_LEN + 6)
#define MAX_FUNC_LINE_LEN 64
#define failwith_xc_v1(xch) xsglue_failwith_xc(xch, __FUNCTION__, __LINE__)

// This is a minimal stub to xenctrl for oxenstored's purposes
// For the full xenctrl stubs, see tools/ocaml/libs/xc/xenctrl_stubs.c

static inline xc_interface *xsglue_xch_of_val_v1(value v)
{
	xc_interface *xch = *(xc_interface **)Data_custom_val(v);

	return xch;
}

static void xsglue_xenctrl_finalize(value v)
{
	xc_interface *xch = xsglue_xch_of_val_v1(v);

	xc_interface_close(xch);
}

static struct custom_operations xsglue_xenctrl_ops = {
	.identifier  = "xenctrl",
	.finalize    = xsglue_xenctrl_finalize,
	.compare     = custom_compare_default,     /* Can't compare     */
	.hash        = custom_hash_default,        /* Can't hash        */
	.serialize   = custom_serialize_default,   /* Can't serialize   */
	.deserialize = custom_deserialize_default, /* Can't deserialize */
	.compare_ext = custom_compare_ext_default, /* Can't compare     */
};

static void Noreturn xsglue_failwith_xc(xc_interface *xch,
		char const* func,
		int line)
{
	char error_str[ERR_MSG_LEN + MAX_FUNC_LINE_LEN];
	size_t str_len = 0;
	if (xch) {
		const xc_error *error = xc_get_last_error(xch);
		if (error->code == XC_ERROR_NONE)
			str_len = snprintf(error_str, ERR_MSG_LEN,
				           "%d: %s", errno, strerror(errno));
		else
			str_len = snprintf(error_str, ERR_MSG_LEN,
				      	   "%d: %s: %s", error->code,
					   xc_error_code_to_desc(error->code),
					   error->message);
	} else {
		str_len = snprintf(error_str, ERR_MSG_LEN,
			           "Unable to open XC interface");
	}
	str_len = str_len < ERR_MSG_LEN ? str_len : ERR_MSG_LEN;
	// Log caller's source code function and line
	snprintf(error_str+str_len, MAX_FUNC_LINE_LEN,
                 " - called from %s:%d", func, line);
	caml_raise_with_string(*caml_named_value("xsg.error"), error_str);
}

CAMLprim value stub_xsglue_xc_interface_open(value unit)
{
	CAMLparam1(unit);
	CAMLlocal1(result);
	xc_interface *xch;

	result = caml_alloc_custom(&xsglue_xenctrl_ops, sizeof(xch), 0, 1);

	caml_enter_blocking_section();
	xch = xc_interface_open(NULL, NULL, 0);
	caml_leave_blocking_section();

	if ( !xch )
		failwith_xc_v1(xch);

	*(xc_interface **)Data_custom_val(result) = xch;

	CAMLreturn(result);
}

static value xsglue_alloc_domaininfo_v1(xc_domaininfo_t * info)
{
	CAMLparam0();
	CAMLlocal1(result);
	result = caml_alloc_tuple(4);

	Store_field(result,  0, Val_int(info->domain));
	Store_field(result,  1, Val_bool(info->flags & XEN_DOMINF_dying));
	Store_field(result,  2, Val_bool(info->flags & XEN_DOMINF_shutdown));
	Store_field(result,  3, Val_int(MASK_EXTR(info->flags, XEN_DOMINF_shutdownmask)));

	CAMLreturn(result);
}

CAMLprim value stub_xsglue_xc_domain_getinfo(value xch_val, value domid)
{
	CAMLparam2(xch_val, domid);
	CAMLlocal1(result);
	xc_interface *xch = xsglue_xch_of_val_v1(xch_val);
	xc_domaininfo_t info;
	int ret;

	ret = xc_domain_getinfo_single(xch, Int_val(domid), &info);
	if (ret < 0)
		failwith_xc_v1(xch);

	result = xsglue_alloc_domaininfo_v1(&info);

	CAMLreturn(result);
}

CAMLprim value stub_xsglue_xc_domain_getinfolist(value xch_val, value first_domain, value nb)
{
	CAMLparam3(xch_val, first_domain, nb);
	CAMLlocal2(result, temp);
	xc_interface *xch = xsglue_xch_of_val_v1(xch_val);
	xc_domaininfo_t * info;
	int i, ret, toalloc, retval;
	unsigned int c_max_domains;
	uint32_t c_first_domain;

	/* get the minimum number of allocate byte we need and bump it up to page boundary */
	toalloc = (sizeof(xc_domaininfo_t) * Int_val(nb)) | 0xfff;
	ret = posix_memalign((void **) ((void *) &info), 4096, toalloc);
	if (ret)
		caml_raise_out_of_memory();

	result = temp = Val_emptylist;

	c_first_domain = Int_val(first_domain);
	c_max_domains = Int_val(nb);
	caml_enter_blocking_section();
	retval = xc_domain_getinfolist(xch, c_first_domain,
				       c_max_domains, info);
	caml_leave_blocking_section();

	if (retval < 0) {
		free(info);
		failwith_xc_v1(xch);
	}
	for (i = 0; i < retval; i++) {
		result = caml_alloc_small(2, Tag_cons);
		Field(result, 0) = Val_int(0);
		Field(result, 1) = temp;
		temp = result;

		Store_field(result, 0, xsglue_alloc_domaininfo_v1(info + i));
	}

	free(info);
	CAMLreturn(result);
}

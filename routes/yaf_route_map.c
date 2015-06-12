/*
  +----------------------------------------------------------------------+
  | Yet Another Framework                                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Xinchen Hui  <laruence@php.net>                              |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "zend_smart_str.h" /* for smart_str */

#include "php_yaf.h"
#include "yaf_namespace.h"
#include "yaf_exception.h"
#include "yaf_request.h"

#include "yaf_router.h"
#include "routes/yaf_route_interface.h"
#include "routes/yaf_route_map.h"

zend_class_entry *yaf_route_map_ce;

/** {{{ ARG_INFO
 */
ZEND_BEGIN_ARG_INFO_EX(yaf_route_map_construct_arginfo, 0, 0, 0)
    ZEND_ARG_INFO(0, controller_prefer)
	ZEND_ARG_INFO(0, delimiter)
ZEND_END_ARG_INFO()
/* }}} */

yaf_route_t * yaf_route_map_instance(yaf_route_t *this_ptr, zend_bool controller_prefer, char *delim, uint len) /* {{{ */{
	if (Z_ISUNDEF_P(this_ptr)) {
		object_init_ex(this_ptr, yaf_route_map_ce);
	} 

	if (controller_prefer) {
		zend_update_property_bool(yaf_route_map_ce, this_ptr,
				ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_CTL_PREFER), 1);
	}

	if (delim && len) {
		zend_update_property_stringl(yaf_route_map_ce, this_ptr,
				ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_DELIMETER), delim, len);
	}

	return this_ptr;
}
/* }}} */

/** {{{ int yaf_route_map_route(yaf_route_t *route, yaf_request_t *request)
*/
int yaf_route_map_route(yaf_route_t *route, yaf_request_t *request) {
	zval *ctl_prefer, *delimer, *zuri, *base_uri, params;
	char *req_uri, *tmp, *rest, *ptrptr, *seg;
	char *query_str = NULL;
	uint seg_len = 0;

	smart_str route_result = {0};

	zuri = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_URI), 1, NULL);
	base_uri = zend_read_property(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_BASE), 1, NULL);

	ctl_prefer = zend_read_property(yaf_route_map_ce, route, ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_CTL_PREFER), 1, NULL);
	delimer	   = zend_read_property(yaf_route_map_ce, route, ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_DELIMETER), 1, NULL);

	if (base_uri && IS_STRING == Z_TYPE_P(base_uri)
			&& !strncasecmp(Z_STRVAL_P(zuri), Z_STRVAL_P(base_uri), Z_STRLEN_P(base_uri))) {
		req_uri  = estrdup(Z_STRVAL_P(zuri) + Z_STRLEN_P(base_uri));
	} else {
		req_uri  = estrdup(Z_STRVAL_P(zuri));
	}

	if (Z_TYPE_P(delimer) == IS_STRING
			&& Z_STRLEN_P(delimer)) {
		if ((query_str = strstr(req_uri, Z_STRVAL_P(delimer))) != NULL
			&& *(query_str - 1) == '/') {
			tmp  = req_uri;
			rest = query_str + Z_STRLEN_P(delimer);
			if (*rest == '\0') {
				req_uri 	= estrndup(req_uri, query_str - req_uri);
				query_str 	= NULL;
				efree(tmp);
			} else if (*rest == '/') {
				req_uri 	= estrndup(req_uri, query_str - req_uri);
				query_str   = estrdup(rest);
				efree(tmp);
			} else {
				query_str = NULL;
			}
		}
	}

	seg = php_strtok_r(req_uri, YAF_ROUTER_URL_DELIMIETER, &ptrptr);
	while (seg) {
		seg_len = strlen(seg);
		if (seg_len) {
			smart_str_appendl(&route_result, seg, seg_len);
		}
		smart_str_appendc(&route_result, '_');
		seg = php_strtok_r(NULL, YAF_ROUTER_URL_DELIMIETER, &ptrptr);
	}

	if (route_result.s) {
		if (Z_TYPE_P(ctl_prefer) == IS_TRUE) {
			zend_update_property_stringl(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_CONTROLLER), route_result.s->val, route_result.s->len - 1);
		} else {
			zend_update_property_stringl(yaf_request_ce, request, ZEND_STRL(YAF_REQUEST_PROPERTY_NAME_ACTION), route_result.s->val, route_result.s->len - 1);
		}
		zend_string_release(route_result.s);
	}

	if (query_str) {
		(void)yaf_router_parse_parameters(query_str, &params);
		(void)yaf_request_set_params_multi(request, &params);
		zval_ptr_dtor(&params);
		efree(query_str);
	}

	efree(req_uri);

	return 1;
}
/* }}} */

/** {{{ proto public Yaf_Route_Simple::route(Yaf_Request $req)
*/
PHP_METHOD(yaf_route_map, route) {
	yaf_request_t *request;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "O", &request, yaf_request_ce) == FAILURE) {
		return;
	} else {
		RETURN_BOOL(yaf_route_map_route(getThis(), request));
	}
}
/* }}} */

/** {{{ zend_string * yaf_route_map_assemble(zval *info, zval *query)
 */
zend_string * yaf_route_map_assemble(yaf_route_t *this_ptr, zval *info, zval *query) {
	char *tmp, *ptrptr, *pname;
	smart_str tvalue = {0};
	uint tmp_len, has_delim = 0;
	zval *delim, *ctl_prefer, *tmp_data;

	ctl_prefer = zend_read_property(yaf_route_map_ce, this_ptr, ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_CTL_PREFER), 1, NULL);
	delim = zend_read_property(yaf_route_map_ce, this_ptr, ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_DELIMETER), 1, NULL);
	if (IS_STRING == Z_TYPE_P(delim) && Z_STRLEN_P(delim)) {
		has_delim = 1;
	}

	do {
		if (Z_TYPE_P(ctl_prefer) == IS_TRUE) {
			if ((tmp_data = zend_hash_str_find(Z_ARRVAL_P(info), ZEND_STRL(YAF_ROUTE_ASSEMBLE_ACTION_FORMAT))) != NULL) {
				pname = estrndup(Z_STRVAL_P(tmp_data), Z_STRLEN_P(tmp_data));
			} else {
				yaf_trigger_error(YAF_ERR_TYPE_ERROR, "%s", "Undefined the 'action' parameter for the 1st parameter");
				break;
			}
		} else {
			if ((tmp_data = zend_hash_str_find(Z_ARRVAL_P(info), ZEND_STRL(YAF_ROUTE_ASSEMBLE_CONTROLLER_FORMAT))) != NULL) {
				pname = estrndup(Z_STRVAL_P(tmp_data), Z_STRLEN_P(tmp_data));
			} else {
				yaf_trigger_error(YAF_ERR_TYPE_ERROR, "%s", "Undefined the 'controller' parameter for the 1st parameter");
				break;
			}
		}

		tmp = php_strtok_r(pname, "_", &ptrptr);	
		while(tmp) {
			tmp_len = strlen(tmp);
			if (tmp_len) {
				smart_str_appendc(&tvalue, '/');
				smart_str_appendl(&tvalue, tmp, tmp_len);
			}
			tmp = php_strtok_r(NULL, "_", &ptrptr);
		}
		efree(pname);

		if (query && IS_ARRAY == Z_TYPE_P(query)) {
			uint i = 0;
			zend_string *key;
			ulong key_idx;
			zval *tmp_data;

			if (has_delim) {
				smart_str_appendc(&tvalue, '/');
				smart_str_appendl(&tvalue, Z_STRVAL_P(delim), Z_STRLEN_P(delim));
			}

            ZEND_HASH_FOREACH_KEY_VAL(Z_ARRVAL_P(query), key_idx, key, tmp_data) {

				if (IS_STRING == Z_TYPE_P(tmp_data) && key) {

					if (has_delim) {
						smart_str_appendc(&tvalue, '/');
						smart_str_appendl(&tvalue, key->val, key->len);
						smart_str_appendc(&tvalue, '/');
						smart_str_appendl(&tvalue, Z_STRVAL_P(tmp_data), Z_STRLEN_P(tmp_data));
					} else {
						if (i == 0) {
							smart_str_appendc(&tvalue, '?');
							smart_str_appendl(&tvalue, key->val, key->len);
							smart_str_appendc(&tvalue, '=');
							smart_str_appendl(&tvalue, Z_STRVAL_P(tmp_data), Z_STRLEN_P(tmp_data));
						} else {
							smart_str_appendc(&tvalue, '&');
							smart_str_appendl(&tvalue, key->val, key->len);
							smart_str_appendc(&tvalue, '=');
							smart_str_appendl(&tvalue, Z_STRVAL_P(tmp_data), Z_STRLEN_P(tmp_data));
						}
					}
				}
				i += 1;
			} ZEND_HASH_FOREACH_END();
		}

		smart_str_0(&tvalue);
		return tvalue.s;
	} while (0);

	return NULL;
}

/** {{{ proto public Yaf_Route_Simple::__construct(bool $controller_prefer=FALSE, string $delimer = '#!')
*/
PHP_METHOD(yaf_route_map, __construct) {
	char *delim	= NULL;
	size_t delim_len = 0;
	zend_bool controller_prefer = 0;
	zval rself, *self = getThis();

	if (zend_parse_parameters_throw(ZEND_NUM_ARGS(), "|bs",
			   	&controller_prefer, &delim, &delim_len) == FAILURE) {
		return;
	}

    if (!self) {
        ZVAL_NULL(&rself);
        self = &rself;
    }
	(void)yaf_route_map_instance(self, controller_prefer, delim, delim_len);
}
/* }}} */

/** {{{ proto public Yaf_Route_Map::assemble(array $info[, array $query = NULL])
*/
PHP_METHOD(yaf_route_map, assemble) {
	zval *info, *query = NULL;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "a|a", &info, &query) == FAILURE) {
        return;
    } else {
		zend_string *str;
        if ((str = yaf_route_map_assemble(getThis(), info, query)) != NULL) {
			RETURN_STR(str);
		}
		RETURN_NULL();
    }
}
/* }}} */

/** {{{ yaf_route_map_methods
*/
zend_function_entry yaf_route_map_methods[] = {
	PHP_ME(yaf_route_map, __construct, yaf_route_map_construct_arginfo, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
	PHP_ME(yaf_route_map, route, yaf_route_route_arginfo, ZEND_ACC_PUBLIC)
	PHP_ME(yaf_route_map, assemble, yaf_route_assemble_arginfo, ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */

/** {{{ YAF_STARTUP_FUNCTION
*/
YAF_STARTUP_FUNCTION(route_map) {
	zend_class_entry ce;

	YAF_INIT_CLASS_ENTRY(ce, "Yaf_Route_Map", "Yaf\\Route\\Map", yaf_route_map_methods);
	yaf_route_map_ce = zend_register_internal_class(&ce);
	zend_class_implements(yaf_route_map_ce, 1, yaf_route_ce);

	yaf_route_map_ce->ce_flags |= ZEND_ACC_FINAL;

	zend_declare_property_bool(yaf_route_map_ce, ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_CTL_PREFER), 0, ZEND_ACC_PROTECTED);
	zend_declare_property_null(yaf_route_map_ce, ZEND_STRL(YAF_ROUTE_MAP_VAR_NAME_DELIMETER),  ZEND_ACC_PROTECTED);

	return SUCCESS;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

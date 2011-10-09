#include <sys/stat.h>
#include <libxml/HTMLtree.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include "curllog/curl_obj.h"
#include "nl_assert.h"

curl_form::curl_form(curl_obj *co_in, xmlNodePtr top) {
  co = co_in;
  form = top;
  submit_buf = 0;
  submit_buf_size = 0;
  submit_size = 0;
}

curl_form::~curl_form() {
}

/**
 * @return true if the element is found and updated.
 */
int curl_form::set( xmlNodePtr xp, const char *name, const char *value ) {
  for ( ; xp != NULL; xp = xp->next ) {
    if ( xp->name != NULL && strcasecmp((const char *)xp->name, "input") == 0 ) {
      const char *nm = (const char *)xmlGetProp(xp, (const xmlChar *)"name" );
      if ( nm != 0 && strcmp(nm, name) == 0 ) {
        xmlSetProp(xp, (const xmlChar *)"value", (const xmlChar *)value );
        return 1;
      }
    }
    if ( set(xp->children, name, value) )
      return 1;
  }
  return 0;
}

void curl_form::set( const char *name, const char *value ) {
  if ( ! set( form, name, value ) ) {
    // Need to add an input element with name and value
    xmlNodePtr field = xmlNewChild(form, NULL, (const xmlChar *) "input", NULL);
    xmlNewProp(field, (const xmlChar *)"name", (const xmlChar *)name );
    xmlNewProp(field, (const xmlChar *)"value", (const xmlChar *)value );
  }
}

/**
 * @return true if the element is found and updated.
 */
int curl_form::checkbox( xmlNodePtr xp, const char *name, const char *value, int checked ) {
  for ( ; xp != NULL; xp = xp->next ) {
    if ( xp->name != NULL && strcasecmp((const char *)xp->name, "input") == 0 ) {
      const char *nm = (const char *)xmlGetProp(xp, (const xmlChar *)"name" );
      const char *val = (const char *)xmlGetProp(xp, (const xmlChar *)"value" );
      if ( nm != 0 && strcmp(nm, name) == 0 && val != 0 && strcmp(val, value) == 0) {
        if ( checked ) {
          xmlSetProp(xp, (const xmlChar *)"checked", (const xmlChar *)"checked" );
        } else {
          xmlUnsetProp(xp, (const xmlChar *)"checked");
        }
        return 1;
      }
    }
    if ( checkbox(xp->children, name, value, checked) )
      return 1;
  }
  return 0;
}

void curl_form::checkbox( const char *name, const char *value, int checked ) {
  if ( ! checkbox( form, name, value, checked ) ) {
    // Need to add an input element with name and value
    xmlNodePtr field = xmlNewChild(form, NULL, (const xmlChar *) "input", NULL);
    xmlNewProp(field, (const xmlChar *)"name", (const xmlChar *)name );
    xmlNewProp(field, (const xmlChar *)"value", (const xmlChar *)value );
    xmlNewProp(field, (const xmlChar *)"type", (const xmlChar *)"checkbox" );
    if ( checked )
      xmlNewProp(field, (const xmlChar *)"checked", (const xmlChar *)"checked" );
  }
}

void curl_form::realloc_submit() {
  submit_buf_size = submit_buf_size ? 2*submit_buf_size : 1024;
  submit_buf = (char *)realloc(submit_buf, submit_buf_size);
  if ( submit_buf == NULL )
    nl_error( 3, "Out of memory in curl_form::append_to_submit()" );
}

void curl_form::quote_to_submit( const char *text ) {
  char *s = curl_easy_escape(co->handle, text, 0);
  if ( s == NULL ) {
    nl_error( 1, "curl_easy_escape() returned NULL" );
  } else {
    append_to_submit(s);
    curl_free(s);
  }
}

void curl_form::append_to_submit( const char *text ) {
  int newlen = strlen(text);
  while ( submit_size + newlen >= submit_buf_size ) {
    realloc_submit();
  }
  strcpy(submit_buf+submit_size, text);
  submit_size += newlen;
}

void curl_form::append_pair_to_submit( const char *nm, const char *val ) {
  if (need_amp) append_to_submit("&");
  else need_amp = 1;
  quote_to_submit(nm);
  append_to_submit("=");
  quote_to_submit(val);
}

void curl_form::submit_int(xmlNodePtr xp) {
  for ( ; xp != NULL; xp = xp->next ) {
    if ( xp->name != NULL && strcasecmp((const char *)xp->name, "input") == 0 ) {
      const char *nm = (const char *)xmlGetProp(xp, (const xmlChar *)"name" );
      const char *typ = (const char *)xmlGetProp(xp, (const xmlChar *)"type" );
      const char *val = (const char *)xmlGetProp(xp, (const xmlChar *)"value" );
      if ( typ != NULL && strcasecmp(typ, "checkbox") == 0 ) {
        const char *checked = (const char *)xmlGetProp(xp, (const xmlChar *)"checked" );
        if ( checked != NULL && strcasecmp( checked, "checked") == 0 ) {
          append_pair_to_submit(nm, val);
        }
      } else if ( typ == NULL || strcasecmp(typ, "submit") != 0 ) {
        if ( nm == NULL ) {
          nl_error(1, "Input with no name" );
        } else if ( val != NULL ) {
          nl_error(-2, "Input %s=%s", nm, val );
          append_pair_to_submit(nm, val);
        }
      }
    }
    submit_int(xp->children);
  }
}

void curl_form::submit_setup( const char *name, const char *value ) {
  const char *method = (const char *)xmlGetProp(form, (const xmlChar *)"method");
  const char *action = (const char *)xmlGetProp(form, (const xmlChar *)"action");
  int is_post = 0;
  submit_size = 0;
  need_amp = 0;
  action = co->relative_url(action);
  if ( strcasecmp( method, "post" ) == 0 ) {
    is_post = 1;
  } else {
    append_to_submit( action );
    append_to_submit( "?" );
  }
  submit_int(form);
  append_pair_to_submit( name, value );
  if ( is_post ) {
    co->set_url(action);
    co->set_postfields(submit_buf, submit_size);
  } else {
    co->set_url(submit_buf);
  }
  nl_error(-2, "Submit %s=%s", name, value);
}

/**
 * easy handle version.
 */
void curl_form::submit( const char *desc, const char *name, const char *value ) {
  submit_setup( name, value );
  co->perform(desc);
}

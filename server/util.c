/* ====================================================================
 * The Apache Software License, Version 1.1
 *
 * Copyright (c) 2000 The Apache Software Foundation.  All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The end-user documentation included with the redistribution,
 *    if any, must include the following acknowledgment:
 *       "This product includes software developed by the
 *        Apache Software Foundation (http://www.apache.org/)."
 *    Alternately, this acknowledgment may appear in the software itself,
 *    if and wherever such third-party acknowledgments normally appear.
 *
 * 4. The names "Apache" and "Apache Software Foundation" must
 *    not be used to endorse or promote products derived from this
 *    software without prior written permission. For written
 *    permission, please contact apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache",
 *    nor may "Apache" appear in their name, without prior written
 *    permission of the Apache Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE APACHE SOFTWARE FOUNDATION OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 * Portions of this software are based upon public domain software
 * originally written at the National Center for Supercomputing Applications,
 * University of Illinois, Urbana-Champaign.
 */

/*
 * util.c: string utility things
 * 
 * 3/21/93 Rob McCool
 * 1995-96 Many changes by the Apache Software Foundation
 * 
 */

/* Debugging aid:
 * #define DEBUG            to trace all cfg_open*()/cfg_closefile() calls
 * #define DEBUG_CFG_LINES  to trace every line read from the config files
 */

#define CORE_PRIVATE

#include "ap_config.h"
#include "ap_base64.h"
#include "httpd.h"
#include "http_main.h"
#include "http_log.h"
#include "http_protocol.h"
#include "http_config.h"
#include "util_ebcdic.h"

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#include <grp.h>
#endif

/* A bunch of functions in util.c scan strings looking for certain characters.
 * To make that more efficient we encode a lookup table.  The test_char_table
 * is generated automatically by gen_test_char.c.
 */
#include "test_char.h"

/* we assume the folks using this ensure 0 <= c < 256... which means
 * you need a cast to (unsigned char) first, you can't just plug a
 * char in here and get it to work, because if char is signed then it
 * will first be sign extended.
 */
#define TEST_CHAR(c, f)	(test_char_table[(unsigned)(c)] & (f))

/*
 * Examine a field value (such as a media-/content-type) string and return
 * it sans any parameters; e.g., strip off any ';charset=foo' and the like.
 */
API_EXPORT(char *) ap_field_noparam(ap_pool_t *p, const char *intype)
{
    const char *semi;

    if (intype == NULL) return NULL;

    semi = ap_strchr_c(intype, ';');
    if (semi == NULL) {
	return ap_pstrdup(p, intype);
    } 
    else {
	while ((semi > intype) && ap_isspace(semi[-1])) {
	    semi--;
	}
	return ap_pstrndup(p, intype, semi - intype);
    }
}

API_EXPORT(char *) ap_ht_time(ap_pool_t *p, ap_time_t t, const char *fmt, int gmt)
{
    ap_size_t retcode;
    char ts[MAX_STRING_LEN];
    char tf[MAX_STRING_LEN];
    ap_exploded_time_t xt;

    if (gmt) {
	const char *f;
	char *strp;

        ap_explode_gmt(&xt, t);
	/* Convert %Z to "GMT" and %z to "+0000";
	 * on hosts that do not have a time zone string in struct tm,
	 * strftime must assume its argument is local time.
	 */
	for(strp = tf, f = fmt; strp < tf + sizeof(tf) - 6 && (*strp = *f)
	    ; f++, strp++) {
	    if (*f != '%') continue;
	    switch (f[1]) {
	    case '%':
		*++strp = *++f;
		break;
	    case 'Z':
		*strp++ = 'G';
		*strp++ = 'M';
		*strp = 'T';
		f++;
		break;
	    case 'z': /* common extension */
		*strp++ = '+';
		*strp++ = '0';
		*strp++ = '0';
		*strp++ = '0';
		*strp = '0';
		f++;
		break;
	    }
	}
	*strp = '\0';
	fmt = tf;
    }
    else {
        ap_explode_localtime(&xt, t);
    }

    /* check return code? */
    ap_strftime(ts, &retcode, MAX_STRING_LEN, fmt, &xt);
    ts[MAX_STRING_LEN - 1] = '\0';
    return ap_pstrdup(p, ts);
}

/* Roy owes Rob beer. */
/* Rob owes Roy dinner. */

/* These legacy comments would make a lot more sense if Roy hadn't
 * replaced the old later_than() routine with util_date.c.
 *
 * Well, okay, they still wouldn't make any sense.
 */

/* Match = 0, NoMatch = 1, Abort = -1
 * Based loosely on sections of wildmat.c by Rich Salz
 * Hmmm... shouldn't this really go component by component?
 */
API_EXPORT(int) ap_strcmp_match(const char *str, const char *exp)
{
    int x, y;

    for (x = 0, y = 0; exp[y]; ++y, ++x) {
	if ((!str[x]) && (exp[y] != '*'))
	    return -1;
	if (exp[y] == '*') {
	    while (exp[++y] == '*');
	    if (!exp[y])
		return 0;
	    while (str[x]) {
		int ret;
		if ((ret = ap_strcmp_match(&str[x++], &exp[y])) != 1)
		    return ret;
	    }
	    return -1;
	}
	else if ((exp[y] != '?') && (str[x] != exp[y]))
	    return 1;
    }
    return (str[x] != '\0');
}

API_EXPORT(int) ap_strcasecmp_match(const char *str, const char *exp)
{
    int x, y;

    for (x = 0, y = 0; exp[y]; ++y, ++x) {
	if ((!str[x]) && (exp[y] != '*'))
	    return -1;
	if (exp[y] == '*') {
	    while (exp[++y] == '*');
	    if (!exp[y])
		return 0;
	    while (str[x]) {
		int ret;
		if ((ret = ap_strcasecmp_match(&str[x++], &exp[y])) != 1)
		    return ret;
	    }
	    return -1;
	}
	else if ((exp[y] != '?') && (ap_tolower(str[x]) != ap_tolower(exp[y])))
	    return 1;
    }
    return (str[x] != '\0');
}

API_EXPORT(int) ap_is_matchexp(const char *str)
{
    register int x;

    for (x = 0; str[x]; x++)
	if ((str[x] == '*') || (str[x] == '?'))
	    return 1;
    return 0;
}

/*
 * Here's a pool-based interface to POSIX regex's regcomp().
 * Note that we return regex_t instead of being passed one.
 * The reason is that if you use an already-used regex_t structure,
 * the memory that you've already allocated gets forgotten, and
 * regfree() doesn't clear it. So we don't allow it.
 */

static ap_status_t regex_cleanup(void *preg)
{
    regfree((regex_t *) preg);
    return APR_SUCCESS;
}

API_EXPORT(regex_t *) ap_pregcomp(ap_pool_t *p, const char *pattern,
				   int cflags)
{
    regex_t *preg = ap_palloc(p, sizeof(regex_t));

    if (regcomp(preg, pattern, cflags)) {
	return NULL;
    }

    ap_register_cleanup(p, (void *) preg, regex_cleanup, regex_cleanup);

    return preg;
}

API_EXPORT(void) ap_pregfree(ap_pool_t *p, regex_t * reg)
{
    regfree(reg);
    ap_kill_cleanup(p, (void *) reg, regex_cleanup);
}

/*
 * Similar to standard strstr() but we ignore case in this version.
 * Based on the strstr() implementation further below.
 */
API_EXPORT(char *) ap_strcasestr(const char *s1, const char *s2)
{
    char *p1, *p2;
    if (*s2 == '\0') {
	/* an empty s2 */
        return((char *)s1);
    }
    while(1) {
	for ( ; (*s1 != '\0') && (ap_tolower(*s1) != ap_tolower(*s2)); s1++);
	if (*s1 == '\0') return(NULL);
	/* found first character of s2, see if the rest matches */
        p1 = (char *)s1;
        p2 = (char *)s2;
        while (ap_tolower(*++p1) == ap_tolower(*++p2)) {
            if (*p1 == '\0') {
                /* both strings ended together */
                return((char *)s1);
            }
        }
        if (*p2 == '\0') {
            /* second string ended, a match */
            break;
        }
	/* didn't find a match here, try starting at next character in s1 */
        s1++;
    }
    return((char *)s1);
}
/* 
 * Apache stub function for the regex libraries regexec() to make sure the
 * whole regex(3) API is available through the Apache (exported) namespace.
 * This is especially important for the DSO situations of modules.
 * DO NOT MAKE A MACRO OUT OF THIS FUNCTION!
 */
API_EXPORT(int) ap_regexec(regex_t *preg, const char *string,
                           size_t nmatch, regmatch_t pmatch[], int eflags)
{
    return regexec(preg, string, nmatch, pmatch, eflags);
}

API_EXPORT(size_t) ap_regerror(int errcode, const regex_t *preg, char *errbuf, size_t errbuf_size)
{
    return regerror(errcode, preg, errbuf, errbuf_size);
}


/* This function substitutes for $0-$9, filling in regular expression
 * submatches. Pass it the same nmatch and pmatch arguments that you
 * passed ap_regexec(). pmatch should not be greater than the maximum number
 * of subexpressions - i.e. one more than the re_nsub member of regex_t.
 *
 * input should be the string with the $-expressions, source should be the
 * string that was matched against.
 *
 * It returns the substituted string, or NULL on error.
 *
 * Parts of this code are based on Henry Spencer's regsub(), from his
 * AT&T V8 regexp package.
 */

API_EXPORT(char *) ap_pregsub(ap_pool_t *p, const char *input, const char *source,
			   size_t nmatch, regmatch_t pmatch[])
{
    const char *src = input;
    char *dest, *dst;
    char c;
    size_t no;
    int len;

    if (!source)
	return NULL;
    if (!nmatch)
	return ap_pstrdup(p, src);

    /* First pass, find the size */

    len = 0;

    while ((c = *src++) != '\0') {
	if (c == '&')
	    no = 0;
	else if (c == '$' && ap_isdigit(*src))
	    no = *src++ - '0';
	else
	    no = 10;

	if (no > 9) {		/* Ordinary character. */
	    if (c == '\\' && (*src == '$' || *src == '&'))
		c = *src++;
	    len++;
	}
	else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
	    len += pmatch[no].rm_eo - pmatch[no].rm_so;
	}

    }

    dest = dst = ap_pcalloc(p, len + 1);

    /* Now actually fill in the string */

    src = input;

    while ((c = *src++) != '\0') {
	if (c == '&')
	    no = 0;
	else if (c == '$' && ap_isdigit(*src))
	    no = *src++ - '0';
	else
	    no = 10;

	if (no > 9) {		/* Ordinary character. */
	    if (c == '\\' && (*src == '$' || *src == '&'))
		c = *src++;
	    *dst++ = c;
	}
	else if (no < nmatch && pmatch[no].rm_so < pmatch[no].rm_eo) {
	    len = pmatch[no].rm_eo - pmatch[no].rm_so;
	    memcpy(dst, source + pmatch[no].rm_so, len);
	    dst += len;
	}

    }
    *dst = '\0';

    return dest;
}

/*
 * Parse .. so we don't compromise security
 */
API_EXPORT(void) ap_getparents(char *name)
{
    int l, w;

    /* Four paseses, as per RFC 1808 */
    /* a) remove ./ path segments */

    for (l = 0, w = 0; name[l] != '\0';) {
	if (name[l] == '.' && name[l + 1] == '/' && (l == 0 || name[l - 1] == '/'))
	    l += 2;
	else
	    name[w++] = name[l++];
    }

    /* b) remove trailing . path, segment */
    if (w == 1 && name[0] == '.')
	w--;
    else if (w > 1 && name[w - 1] == '.' && name[w - 2] == '/')
	w--;
    name[w] = '\0';

    /* c) remove all xx/../ segments. (including leading ../ and /../) */
    l = 0;

    while (name[l] != '\0') {
	if (name[l] == '.' && name[l + 1] == '.' && name[l + 2] == '/' &&
	    (l == 0 || name[l - 1] == '/')) {
	    register int m = l + 3, n;

	    l = l - 2;
	    if (l >= 0) {
		while (l >= 0 && name[l] != '/')
		    l--;
		l++;
	    }
	    else
		l = 0;
	    n = l;
	    while ((name[n] = name[m]))
		(++n, ++m);
	}
	else
	    ++l;
    }

    /* d) remove trailing xx/.. segment. */
    if (l == 2 && name[0] == '.' && name[1] == '.')
	name[0] = '\0';
    else if (l > 2 && name[l - 1] == '.' && name[l - 2] == '.' && name[l - 3] == '/') {
	l = l - 4;
	if (l >= 0) {
	    while (l >= 0 && name[l] != '/')
		l--;
	    l++;
	}
	else
	    l = 0;
	name[l] = '\0';
    }
}

API_EXPORT(void) ap_no2slash(char *name)
{
    char *d, *s;

    s = d = name;

#ifdef WIN32
    /* Check for UNC names.  Leave leading two slashes. */
    if (s[0] == '/' && s[1] == '/')
        *d++ = *s++;
#endif

    while (*s) {
	if ((*d++ = *s) == '/') {
	    do {
		++s;
	    } while (*s == '/');
	}
	else {
	    ++s;
	}
    }
    *d = '\0';
}


/*
 * copy at most n leading directories of s into d
 * d should be at least as large as s plus 1 extra byte
 * assumes n > 0
 * the return value is the ever useful pointer to the trailing \0 of d
 *
 * examples:
 *    /a/b, 1  ==> /
 *    /a/b, 2  ==> /a/
 *    /a/b, 3  ==> /a/b/
 *    /a/b, 4  ==> /a/b/
 */
API_EXPORT(char *) ap_make_dirstr_prefix(char *d, const char *s, int n)
{
    for (;;) {
	if (*s == '\0' || (*s == '/' && (--n) == 0)) {
	    *d = '/';
	    break;
	}
	*d++ = *s++;
    }
    *++d = 0;
    return (d);
}


/*
 * return the parent directory name including trailing / of the file s
 */
API_EXPORT(char *) ap_make_dirstr_parent(ap_pool_t *p, const char *s)
{
    const char *last_slash = ap_strrchr_c(s, '/');
    char *d;
    int l;

    if (last_slash == NULL) {
	/* XXX: well this is really broken if this happens */
	return (ap_pstrdup(p, "/"));
    }
    l = (last_slash - s) + 1;
    d = ap_palloc(p, l + 1);
    memcpy(d, s, l);
    d[l] = 0;
    return (d);
}


/*
 * This function is deprecated.  Use one of the preceeding two functions
 * which are faster.
 */
API_EXPORT(char *) ap_make_dirstr(ap_pool_t *p, const char *s, int n)
{
    register int x, f;
    char *res;

    for (x = 0, f = 0; s[x]; x++) {
	if (s[x] == '/')
	    if ((++f) == n) {
		res = ap_palloc(p, x + 2);
		memcpy(res, s, x);
		res[x] = '/';
		res[x + 1] = '\0';
		return res;
	    }
    }

    if (s[strlen(s) - 1] == '/')
	return ap_pstrdup(p, s);
    else
	return ap_pstrcat(p, s, "/", NULL);
}

API_EXPORT(int) ap_count_dirs(const char *path)
{
    register int x, n;

    for (x = 0, n = 0; path[x]; x++)
	if (path[x] == '/')
	    n++;
    return n;
}


API_EXPORT(void) ap_chdir_file(const char *file)
{
    const char *x;
    char buf[HUGE_STRING_LEN];

    x = ap_strrchr_c(file, '/');
    if (x == NULL) {
	chdir(file);
    }
    else if (x - file < sizeof(buf) - 1) {
	memcpy(buf, file, x - file);
	buf[x - file] = '\0';
	chdir(buf);
    }
    /* XXX: well, this is a silly function, no method of reporting an
     * error... ah well. */
}

API_EXPORT(char *) ap_getword_nc(ap_pool_t *atrans, char **line, char stop)
{
    return ap_getword(atrans, (const char **) line, stop);
}

API_EXPORT(char *) ap_getword(ap_pool_t *atrans, const char **line, char stop)
{
    const char *pos = ap_strchr_c(*line, stop);
    char *res;

    if (!pos) {
	res = ap_pstrdup(atrans, *line);
	*line += strlen(*line);
	return res;
    }

    res = ap_pstrndup(atrans, *line, pos - *line);

    while (*pos == stop) {
	++pos;
    }

    *line = pos;

    return res;
}

API_EXPORT(char *) ap_getword_white_nc(ap_pool_t *atrans, char **line)
{
    return ap_getword_white(atrans, (const char **) line);
}

API_EXPORT(char *) ap_getword_white(ap_pool_t *atrans, const char **line)
{
    int pos = -1, x;
    char *res;

    for (x = 0; (*line)[x]; x++) {
	if (ap_isspace((*line)[x])) {
	    pos = x;
	    break;
	}
    }

    if (pos == -1) {
	res = ap_pstrdup(atrans, *line);
	*line += strlen(*line);
	return res;
    }

    res = ap_palloc(atrans, pos + 1);
    ap_cpystrn(res, *line, pos + 1);

    while (ap_isspace((*line)[pos]))
	++pos;

    *line += pos;

    return res;
}

API_EXPORT(char *) ap_getword_nulls_nc(ap_pool_t *atrans, char **line, char stop)
{
    return ap_getword_nulls(atrans, (const char **) line, stop);
}

API_EXPORT(char *) ap_getword_nulls(ap_pool_t *atrans, const char **line, char stop)
{
    const char *pos = ap_strchr_c(*line, stop);
    char *res;

    if (!pos) {
	res = ap_pstrdup(atrans, *line);
	*line += strlen(*line);
	return res;
    }

    res = ap_pstrndup(atrans, *line, pos - *line);

    ++pos;

    *line = pos;

    return res;
}

/* Get a word, (new) config-file style --- quoted strings and backslashes
 * all honored
 */

static char *substring_conf(ap_pool_t *p, const char *start, int len, char quote)
{
    char *result = ap_palloc(p, len + 2);
    char *resp = result;
    int i;

    for (i = 0; i < len; ++i) {
	if (start[i] == '\\' && (start[i + 1] == '\\'
				 || (quote && start[i + 1] == quote)))
	    *resp++ = start[++i];
	else
	    *resp++ = start[i];
    }

    *resp++ = '\0';
#if RESOLVE_ENV_PER_TOKEN
    return ap_resolve_env(p,result);
#else
    return result;
#endif
}

API_EXPORT(char *) ap_getword_conf_nc(ap_pool_t *p, char **line)
{
    return ap_getword_conf(p, (const char **) line);
}

API_EXPORT(char *) ap_getword_conf(ap_pool_t *p, const char **line)
{
    const char *str = *line, *strend;
    char *res;
    char quote;

    while (*str && ap_isspace(*str))
	++str;

    if (!*str) {
	*line = str;
	return "";
    }

    if ((quote = *str) == '"' || quote == '\'') {
	strend = str + 1;
	while (*strend && *strend != quote) {
	    if (*strend == '\\' && strend[1] && strend[1] == quote)
		strend += 2;
	    else
		++strend;
	}
	res = substring_conf(p, str + 1, strend - str - 1, quote);

	if (*strend == quote)
	    ++strend;
    }
    else {
	strend = str;
	while (*strend && !ap_isspace(*strend))
	    ++strend;

	res = substring_conf(p, str, strend - str, 0);
    }

    while (*strend && ap_isspace(*strend))
	++strend;
    *line = strend;
    return res;
}

/* Check a string for any ${ENV} environment variable
 * construct and replace each them by the value of
 * that environment variable, if it exists. If the
 * environment value does not exist, leave the ${ENV}
 * construct alone; it means something else.
 */
API_EXPORT(const char *) ap_resolve_env(ap_pool_t *p, const char * word)
{
       char tmp[ MAX_STRING_LEN ];
       const char *s, *e;
       tmp[0] = '\0';

       if (!(s=ap_strchr_c(word,'$')))
               return word;

       do {
               /* XXX - relies on strncat() to add '\0'
                */
	       strncat(tmp,word,s - word);
               if ((s[1] == '{') && (e=ap_strchr_c(s,'}'))) {
                       const char *e2 = e;
                       word = e + 1;
                       e = getenv(s+2);
                       if (e) {
                           strcat(tmp,e);
                       } else {
                           strncat(tmp, s, e2-s);
                           strcat(tmp,"}");
                       }
               } else {
                       /* ignore invalid strings */
                       word = s+1;
                       strcat(tmp,"$");
               };
       } while ((s=ap_strchr_c(word,'$')));
       strcat(tmp,word);

       return ap_pstrdup(p,tmp);
}
API_EXPORT(int) ap_cfg_closefile(configfile_t *cfp)
{
#ifdef DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, NULL, 
        "Done with config file %s", cfp->name);
#endif
    return (cfp->close == NULL) ? 0 : cfp->close(cfp->param);
}

static ap_status_t cfg_close(void *param)
{
    ap_file_t *cfp = (ap_file_t *) param;
    return (ap_close(cfp));
}

static int cfg_getch(void *param)
{
    char ch;
    ap_file_t *cfp = (ap_file_t *) param;
    if (ap_getc(&ch, cfp) == APR_SUCCESS)
        return ch;
    return (int)EOF;
}

static void *cfg_getstr(void *buf, size_t bufsiz, void *param)
{
    ap_file_t *cfp = (ap_file_t *) param;
    if (ap_fgets(buf, bufsiz, cfp) == APR_SUCCESS)
        return buf;
    return NULL;
}

/* Open a configfile_t as FILE, return open configfile_t struct pointer */
API_EXPORT(ap_status_t) ap_pcfg_openfile(configfile_t **ret_cfg, ap_pool_t *p, const char *name)
{
    configfile_t *new_cfg;
    ap_file_t *file = NULL;
    ap_finfo_t finfo;
    ap_status_t status;
#ifdef DEBUG
    char buf[120];
#endif

    if (name == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, NULL,
               "Internal error: pcfg_openfile() called with NULL filename");
        return APR_EBADF;
    }

    if (!ap_os_is_filename_valid(name)) {
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, NULL,
                    "Access to config file %s denied: not a valid filename",
                    name);
        return APR_EACCES;
    }

    status = ap_open(&file, name, APR_READ | APR_BUFFERED, APR_OS_DEFAULT, p);
#ifdef DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, NULL,
                "Opening config file %s (%s)",
                name, (status != APR_SUCCESS) ? 
                ap_strerror(status, buf, sizeof(buf)) : "successful");
#endif
    if (status != APR_SUCCESS)
        return status;

    status = ap_getfileinfo(&finfo, file);
    if (status != APR_SUCCESS)
        return status;

    if (finfo.filetype != APR_REG &&
#if defined(WIN32) || defined(OS2)
        !(strcasecmp(name, "nul") == 0 ||
          (strlen(name) >= 4 &&
           strcasecmp(name + strlen(name) - 4, "/nul") == 0))) {
#else
        strcmp(name, "/dev/null") != 0) {
#endif /* WIN32 || OS2 */
        ap_log_error(APLOG_MARK, APLOG_ERR | APLOG_NOERRNO, 0, NULL,
                    "Access to file %s denied by server: not a regular file",
                    name);
        ap_close(file);
        return APR_EBADF;
    }

    new_cfg = ap_palloc(p, sizeof(*new_cfg));
    new_cfg->param = file;
    new_cfg->name = ap_pstrdup(p, name);
    new_cfg->getch = (int (*)(void *)) cfg_getch;
    new_cfg->getstr = (void *(*)(void *, size_t, void *)) cfg_getstr;
    new_cfg->close = (int (*)(void *)) cfg_close;
    new_cfg->line_number = 0;
    *ret_cfg = new_cfg;
    return APR_SUCCESS;
}


/* Allocate a configfile_t handle with user defined functions and params */
API_EXPORT(configfile_t *) ap_pcfg_open_custom(ap_pool_t *p, const char *descr,
    void *param,
    int(*getch)(void *param),
    void *(*getstr) (void *buf, size_t bufsiz, void *param),
    int(*close_func)(void *param))
{
    configfile_t *new_cfg = ap_palloc(p, sizeof(*new_cfg));
#ifdef DEBUG
    ap_log_error(APLOG_MARK, APLOG_DEBUG | APLOG_NOERRNO, NULL, "Opening config handler %s", descr);
#endif
    new_cfg->param = param;
    new_cfg->name = descr;
    new_cfg->getch = getch;
    new_cfg->getstr = getstr;
    new_cfg->close = close_func;
    new_cfg->line_number = 0;
    return new_cfg;
}


/* Read one character from a configfile_t */
API_EXPORT(int) ap_cfg_getc(configfile_t *cfp)
{
    register int ch = cfp->getch(cfp->param);
    if (ch == LF) 
	++cfp->line_number;
    return ch;
}


/* Read one line from open configfile_t, strip LF, increase line number */
/* If custom handler does not define a getstr() function, read char by char */
API_EXPORT(int) ap_cfg_getline(char *buf, size_t bufsize, configfile_t *cfp)
{
    /* If a "get string" function is defined, use it */
    if (cfp->getstr != NULL) {
	char *src, *dst;
	char *cp;
	char *cbuf = buf;
	size_t cbufsize = bufsize;

	while (1) {
	    ++cfp->line_number;
	    if (cfp->getstr(cbuf, cbufsize, cfp->param) == NULL)
		return 1;

	    /*
	     *  check for line continuation,
	     *  i.e. match [^\\]\\[\r]\n only
	     */
	    cp = cbuf;
	    while (cp < cbuf+cbufsize && *cp != '\0')
		cp++;
	    if (cp > cbuf && cp[-1] == LF) {
		cp--;
		if (cp > cbuf && cp[-1] == CR)
		    cp--;
		if (cp > cbuf && cp[-1] == '\\') {
		    cp--;
		    if (!(cp > cbuf && cp[-1] == '\\')) {
			/*
			 * line continuation requested -
			 * then remove backslash and continue
			 */
			cbufsize -= (cp-cbuf);
			cbuf = cp;
			continue;
		    }
		    else {
			/* 
			 * no real continuation because escaped -
			 * then just remove escape character
			 */
			for ( ; cp < cbuf+cbufsize && *cp != '\0'; cp++)
			    cp[0] = cp[1];
		    }   
		}
	    }
	    break;
	}

	/*
	 * Leading and trailing white space is eliminated completely
	 */
	src = buf;
	while (ap_isspace(*src))
	    ++src;
	/* blast trailing whitespace */
	dst = &src[strlen(src)];
	while (--dst >= src && ap_isspace(*dst))
	    *dst = '\0';
        /* Zap leading whitespace by shifting */
        if (src != buf)
	    for (dst = buf; (*dst++ = *src++) != '\0'; )
	        ;

#ifdef DEBUG_CFG_LINES
	ap_log_error(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, NULL, "Read config: %s", buf);
#endif
	return 0;
    } else {
	/* No "get string" function defined; read character by character */
	register int c;
	register size_t i = 0;

	buf[0] = '\0';
	/* skip leading whitespace */
	do {
	    c = cfp->getch(cfp->param);
	} while (c == '\t' || c == ' ');

	if (c == EOF)
	    return 1;
	
	if(bufsize < 2) {
	    /* too small, assume caller is crazy */
	    return 1;
	}

	while (1) {
	    if ((c == '\t') || (c == ' ')) {
		buf[i++] = ' ';
		while ((c == '\t') || (c == ' '))
		    c = cfp->getch(cfp->param);
	    }
	    if (c == CR) {
		/* silently ignore CR (_assume_ that a LF follows) */
		c = cfp->getch(cfp->param);
	    }
	    if (c == LF) {
		/* increase line number and return on LF */
		++cfp->line_number;
	    }
	    if (c == EOF || c == 0x4 || c == LF || i >= (bufsize - 2)) {
		/* 
		 *  check for line continuation
		 */
		if (i > 0 && buf[i-1] == '\\') {
		    i--;
		    if (!(i > 0 && buf[i-1] == '\\')) {
			/* line is continued */
			c = cfp->getch(cfp->param);
			continue;
		    }
		    /* else nothing needs be done because
		     * then the backslash is escaped and
		     * we just strip to a single one
		     */
		}
		/* blast trailing whitespace */
		while (i > 0 && ap_isspace(buf[i - 1]))
		    --i;
		buf[i] = '\0';
#ifdef DEBUG_CFG_LINES
		ap_log_error(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, NULL, "Read config: %s", buf);
#endif
		return 0;
	    }
	    buf[i] = c;
	    ++i;
	    c = cfp->getch(cfp->param);
	}
    }
}

/* Size an HTTP header field list item, as separated by a comma.
 * The return value is a pointer to the beginning of the non-empty list item
 * within the original string (or NULL if there is none) and the address
 * of field is shifted to the next non-comma, non-whitespace character.
 * len is the length of the item excluding any beginning whitespace.
 */
API_EXPORT(const char *) ap_size_list_item(const char **field, int *len)
{
    const unsigned char *ptr = (const unsigned char *)*field;
    const unsigned char *token;
    int in_qpair, in_qstr, in_com;

    /* Find first non-comma, non-whitespace byte */

    while (*ptr == ',' || ap_isspace(*ptr))
        ++ptr;

    token = ptr;

    /* Find the end of this item, skipping over dead bits */

    for (in_qpair = in_qstr = in_com = 0;
         *ptr && (in_qpair || in_qstr || in_com || *ptr != ',');
         ++ptr) {

        if (in_qpair) {
            in_qpair = 0;
        }
        else {
            switch (*ptr) {
                case '\\': in_qpair = 1;      /* quoted-pair         */
                           break;
                case '"' : if (!in_com)       /* quoted string delim */
                               in_qstr = !in_qstr;
                           break;
                case '(' : if (!in_qstr)      /* comment (may nest)  */
                               ++in_com;
                           break;
                case ')' : if (in_com)        /* end comment         */
                               --in_com;
                           break;
                default  : break;
            }
        }
    }

    if ((*len = (ptr - token)) == 0) {
        *field = (const char *)ptr;
        return NULL;
    }

    /* Advance field pointer to the next non-comma, non-white byte */

    while (*ptr == ',' || ap_isspace(*ptr))
	++ptr;

    *field = (const char *)ptr;
    return (const char *)token;
}

/* Retrieve an HTTP header field list item, as separated by a comma,
 * while stripping insignificant whitespace and lowercasing anything not in
 * a quoted string or comment.  The return value is a new string containing
 * the converted list item (or NULL if none) and the address pointed to by
 * field is shifted to the next non-comma, non-whitespace.
 */
API_EXPORT(char *) ap_get_list_item(ap_pool_t *p, const char **field)
{
    const char *tok_start;
    const unsigned char *ptr;
    unsigned char *pos;
    char *token;
    int addspace = 0, in_qpair = 0, in_qstr = 0, in_com = 0, tok_len = 0;

    /* Find the beginning and maximum length of the list item so that
     * we can allocate a buffer for the new string and reset the field.
     */
    if ((tok_start = ap_size_list_item(field, &tok_len)) == NULL) {
        return NULL;
    }
    token = ap_palloc(p, tok_len + 1);

    /* Scan the token again, but this time copy only the good bytes.
     * We skip extra whitespace and any whitespace around a '=', '/',
     * or ';' and lowercase normal characters not within a comment,
     * quoted-string or quoted-pair.
     */
    for (ptr = (const unsigned char *)tok_start, pos = (unsigned char *)token;
         *ptr && (in_qpair || in_qstr || in_com || *ptr != ',');
         ++ptr) {

        if (in_qpair) {
            in_qpair = 0;
            *pos++ = *ptr;
        }
        else {
            switch (*ptr) {
                case '\\': in_qpair = 1;
                           if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case '"' : if (!in_com)
                               in_qstr = !in_qstr;
                           if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case '(' : if (!in_qstr)
                               ++in_com;
                           if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case ')' : if (in_com)
                               --in_com;
                           *pos++ = *ptr;
                           addspace = 0;
                           break;
                case ' ' :
                case '\t': if (addspace)
                               break;
                           if (in_com || in_qstr)
                               *pos++ = *ptr;
                           else
                               addspace = 1;
                           break;
                case '=' :
                case '/' :
                case ';' : if (!(in_com || in_qstr))
                               addspace = -1;
                           *pos++ = *ptr;
                           break;
                default  : if (addspace == 1)
                               *pos++ = ' ';
                           *pos++ = (in_com || in_qstr) ? *ptr
                                                        : ap_tolower(*ptr);
                           addspace = 0;
                           break;
            }
        }
    }
    *pos = '\0';

    return token;
}

/* Find an item in canonical form (lowercase, no extra spaces) within
 * an HTTP field value list.  Returns 1 if found, 0 if not found.
 * This would be much more efficient if we stored header fields as
 * an array of list items as they are received instead of a plain string.
 */
API_EXPORT(int) ap_find_list_item(ap_pool_t *p, const char *line, const char *tok)
{
    const unsigned char *pos;
    const unsigned char *ptr = (const unsigned char *)line;
    int good = 0, addspace = 0, in_qpair = 0, in_qstr = 0, in_com = 0;

    if (!line || !tok)
        return 0;

    do {  /* loop for each item in line's list */

        /* Find first non-comma, non-whitespace byte */

        while (*ptr == ',' || ap_isspace(*ptr))
            ++ptr;

        if (*ptr)
            good = 1;  /* until proven otherwise for this item */
        else
            break;     /* no items left and nothing good found */

        /* We skip extra whitespace and any whitespace around a '=', '/',
         * or ';' and lowercase normal characters not within a comment,
         * quoted-string or quoted-pair.
         */
        for (pos = (const unsigned char *)tok;
             *ptr && (in_qpair || in_qstr || in_com || *ptr != ',');
             ++ptr) {

            if (in_qpair) {
                in_qpair = 0;
                if (good)
                    good = (*pos++ == *ptr);
            }
            else {
                switch (*ptr) {
                    case '\\': in_qpair = 1;
                               if (addspace == 1)
                                   good = good && (*pos++ == ' ');
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case '"' : if (!in_com)
                                   in_qstr = !in_qstr;
                               if (addspace == 1)
                                   good = good && (*pos++ == ' ');
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case '(' : if (!in_qstr)
                                   ++in_com;
                               if (addspace == 1)
                                   good = good && (*pos++ == ' ');
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case ')' : if (in_com)
                                   --in_com;
                               good = good && (*pos++ == *ptr);
                               addspace = 0;
                               break;
                    case ' ' :
                    case '\t': if (addspace || !good)
                                   break;
                               if (in_com || in_qstr)
                                   good = (*pos++ == *ptr);
                               else
                                   addspace = 1;
                               break;
                    case '=' :
                    case '/' :
                    case ';' : if (!(in_com || in_qstr))
                                   addspace = -1;
                               good = good && (*pos++ == *ptr);
                               break;
                    default  : if (!good)
                                   break;
                               if (addspace == 1)
                                   good = (*pos++ == ' ');
                               if (in_com || in_qstr)
                                   good = good && (*pos++ == *ptr);
                               else
                                   good = good && (*pos++ == ap_tolower(*ptr));
                               addspace = 0;
                               break;
                }
            }
        }
        if (good && *pos)
            good = 0;          /* not good if only a prefix was matched */

    } while (*ptr && !good);

    return good;
}


/* Retrieve a token, spacing over it and returning a pointer to
 * the first non-white byte afterwards.  Note that these tokens
 * are delimited by semis and commas; and can also be delimited
 * by whitespace at the caller's option.
 */

API_EXPORT(char *) ap_get_token(ap_pool_t *p, const char **accept_line, int accept_white)
{
    const char *ptr = *accept_line;
    const char *tok_start;
    char *token;
    int tok_len;

    /* Find first non-white byte */

    while (*ptr && ap_isspace(*ptr))
	++ptr;

    tok_start = ptr;

    /* find token end, skipping over quoted strings.
     * (comments are already gone).
     */

    while (*ptr && (accept_white || !ap_isspace(*ptr))
	   && *ptr != ';' && *ptr != ',') {
	if (*ptr++ == '"')
	    while (*ptr)
		if (*ptr++ == '"')
		    break;
    }

    tok_len = ptr - tok_start;
    token = ap_pstrndup(p, tok_start, tok_len);

    /* Advance accept_line pointer to the next non-white byte */

    while (*ptr && ap_isspace(*ptr))
	++ptr;

    *accept_line = ptr;
    return token;
}


/* find http tokens, see the definition of token from RFC2068 */
API_EXPORT(int) ap_find_token(ap_pool_t *p, const char *line, const char *tok)
{
    const unsigned char *start_token;
    const unsigned char *s;

    if (!line)
	return 0;

    s = (const unsigned char *)line;
    for (;;) {
	/* find start of token, skip all stop characters, note NUL
	 * isn't a token stop, so we don't need to test for it
	 */
	while (TEST_CHAR(*s, T_HTTP_TOKEN_STOP)) {
	    ++s;
	}
	if (!*s) {
	    return 0;
	}
	start_token = s;
	/* find end of the token */
	while (*s && !TEST_CHAR(*s, T_HTTP_TOKEN_STOP)) {
	    ++s;
	}
	if (!strncasecmp((const char *)start_token, (const char *)tok, s - start_token)) {
	    return 1;
	}
	if (!*s) {
	    return 0;
	}
    }
}


API_EXPORT(int) ap_find_last_token(ap_pool_t *p, const char *line, const char *tok)
{
    int llen, tlen, lidx;

    if (!line)
	return 0;

    llen = strlen(line);
    tlen = strlen(tok);
    lidx = llen - tlen;

    if ((lidx < 0) ||
	((lidx > 0) && !(ap_isspace(line[lidx - 1]) || line[lidx - 1] == ',')))
	return 0;

    return (strncasecmp(&line[lidx], tok, tlen) == 0);
}

API_EXPORT(char *) ap_escape_shell_cmd(ap_pool_t *p, const char *str)
{
    char *cmd;
    unsigned char *d;
    const unsigned char *s;

    cmd = ap_palloc(p, 2 * strlen(str) + 1);	/* Be safe */
    d = (unsigned char *)cmd;
    s = (const unsigned char *)str;
    for (; *s; ++s) {

#if defined(OS2) || defined(WIN32)
	/* Don't allow '&' in parameters under OS/2. */
	/* This can be used to send commands to the shell. */
	if (*s == '&') {
	    *d++ = ' ';
	    continue;
	}
#endif

	if (TEST_CHAR(*s, T_ESCAPE_SHELL_CMD)) {
	    *d++ = '\\';
	}
	*d++ = *s;
    }
    *d = '\0';

    return cmd;
}

static char x2c(const char *what)
{
    register char digit;

#ifndef CHARSET_EBCDIC
    digit = ((what[0] >= 'A') ? ((what[0] & 0xdf) - 'A') + 10 : (what[0] - '0'));
    digit *= 16;
    digit += (what[1] >= 'A' ? ((what[1] & 0xdf) - 'A') + 10 : (what[1] - '0'));
#else /*CHARSET_EBCDIC*/
    char xstr[5];
    xstr[0]='0';
    xstr[1]='x';
    xstr[2]=what[0];
    xstr[3]=what[1];
    xstr[4]='\0';
    digit = ap_xlate_conv_byte(ap_hdrs_from_ascii, 0xFF & strtol(xstr, NULL, 16));
#endif /*CHARSET_EBCDIC*/
    return (digit);
}

/*
 * Unescapes a URL.
 * Returns 0 on success, non-zero on error
 * Failure is due to
 *   bad % escape       returns HTTP_BAD_REQUEST
 *
 *   decoding %00 -> \0
 *   decoding %2f -> /   (a special character)
 *                      returns HTTP_NOT_FOUND
 */
API_EXPORT(int) ap_unescape_url(char *url)
{
    register int badesc, badpath;
    char *x, *y;

    badesc = 0;
    badpath = 0;
    /* Initial scan for first '%'. Don't bother writing values before
     * seeing a '%' */
    y = strchr(url, '%');
    if (y == NULL) {
        return OK;
    }
    for (x = y; *y; ++x, ++y) {
	if (*y != '%')
	    *x = *y;
	else {
	    if (!ap_isxdigit(*(y + 1)) || !ap_isxdigit(*(y + 2))) {
		badesc = 1;
		*x = '%';
	    }
	    else {
		*x = x2c(y + 1);
		y += 2;
		if (*x == '/' || *x == '\0')
		    badpath = 1;
	    }
	}
    }
    *x = '\0';
    if (badesc)
	return HTTP_BAD_REQUEST;
    else if (badpath)
	return HTTP_NOT_FOUND;
    else
	return OK;
}

API_EXPORT(char *) ap_construct_server(ap_pool_t *p, const char *hostname,
				    unsigned port, const request_rec *r)
{
    if (ap_is_default_port(port, r))
	return ap_pstrdup(p, hostname);
    else {
	return ap_psprintf(p, "%s:%u", hostname, port);
    }
}

/* c2x takes an unsigned, and expects the caller has guaranteed that
 * 0 <= what < 256... which usually means that you have to cast to
 * unsigned char first, because (unsigned)(char)(x) first goes through
 * signed extension to an int before the unsigned cast.
 *
 * The reason for this assumption is to assist gcc code generation --
 * the unsigned char -> unsigned extension is already done earlier in
 * both uses of this code, so there's no need to waste time doing it
 * again.
 */
static const char c2x_table[] = "0123456789abcdef";

static ap_inline unsigned char *c2x(unsigned what, unsigned char *where)
{
#ifdef CHARSET_EBCDIC
    what = ap_xlate_conv_byte(ap_hdrs_to_ascii, (unsigned char)what);
#endif /*CHARSET_EBCDIC*/
    *where++ = '%';
    *where++ = c2x_table[what >> 4];
    *where++ = c2x_table[what & 0xf];
    return where;
}

/*
 * escape_path_segment() escapes a path segment, as defined in RFC 1808. This
 * routine is (should be) OS independent.
 *
 * os_escape_path() converts an OS path to a URL, in an OS dependent way. In all
 * cases if a ':' occurs before the first '/' in the URL, the URL should be
 * prefixed with "./" (or the ':' escaped). In the case of Unix, this means
 * leaving '/' alone, but otherwise doing what escape_path_segment() does. For
 * efficiency reasons, we don't use escape_path_segment(), which is provided for
 * reference. Again, RFC 1808 is where this stuff is defined.
 *
 * If partial is set, os_escape_path() assumes that the path will be appended to
 * something with a '/' in it (and thus does not prefix "./").
 */

API_EXPORT(char *) ap_escape_path_segment(ap_pool_t *p, const char *segment)
{
    char *copy = ap_palloc(p, 3 * strlen(segment) + 1);
    const unsigned char *s = (const unsigned char *)segment;
    unsigned char *d = (unsigned char *)copy;
    unsigned c;

    while ((c = *s)) {
	if (TEST_CHAR(c, T_ESCAPE_PATH_SEGMENT)) {
	    d = c2x(c, d);
	}
	else {
	    *d++ = c;
	}
	++s;
    }
    *d = '\0';
    return copy;
}

API_EXPORT(char *) ap_os_escape_path(ap_pool_t *p, const char *path, int partial)
{
    char *copy = ap_palloc(p, 3 * strlen(path) + 3);
    const unsigned char *s = (const unsigned char *)path;
    unsigned char *d = (unsigned char *)copy;
    unsigned c;

    if (!partial) {
	const char *colon = ap_strchr_c(path, ':');
	const char *slash = ap_strchr_c(path, '/');

	if (colon && (!slash || colon < slash)) {
	    *d++ = '.';
	    *d++ = '/';
	}
    }
    while ((c = *s)) {
	if (TEST_CHAR(c, T_OS_ESCAPE_PATH)) {
	    d = c2x(c, d);
	}
	else {
	    *d++ = c;
	}
	++s;
    }
    *d = '\0';
    return copy;
}

/* ap_escape_uri is now a macro for os_escape_path */

API_EXPORT(char *) ap_escape_html(ap_pool_t *p, const char *s)
{
    int i, j;
    char *x;

    /* first, count the number of extra characters */
    for (i = 0, j = 0; s[i] != '\0'; i++)
	if (s[i] == '<' || s[i] == '>')
	    j += 3;
	else if (s[i] == '&')
	    j += 4;

    if (j == 0)
	return ap_pstrndup(p, s, i);

    x = ap_palloc(p, i + j + 1);
    for (i = 0, j = 0; s[i] != '\0'; i++, j++)
	if (s[i] == '<') {
	    memcpy(&x[j], "&lt;", 4);
	    j += 3;
	}
	else if (s[i] == '>') {
	    memcpy(&x[j], "&gt;", 4);
	    j += 3;
	}
	else if (s[i] == '&') {
	    memcpy(&x[j], "&amp;", 5);
	    j += 4;
	}
	else
	    x[j] = s[i];

    x[j] = '\0';
    return x;
}

API_EXPORT(int) ap_is_directory(const char *path)
{
    ap_finfo_t finfo;

    if (ap_stat(&finfo, path, NULL) == -1)
	return 0;		/* in error condition, just return no */

    return (finfo.filetype == APR_DIR);
}

API_EXPORT(char *) ap_make_full_path(ap_pool_t *a, const char *src1,
				  const char *src2)
{
    register int x;

    x = strlen(src1);
    if (x == 0)
	return ap_pstrcat(a, "/", src2, NULL);

    if (src1[x - 1] != '/')
	return ap_pstrcat(a, src1, "/", src2, NULL);
    else
	return ap_pstrcat(a, src1, src2, NULL);
}

/*
 * Check for an absoluteURI syntax (see section 3.2 in RFC2068).
 */
API_EXPORT(int) ap_is_url(const char *u)
{
    register int x;

    for (x = 0; u[x] != ':'; x++) {
	if ((!u[x]) ||
	    ((!ap_isalpha(u[x])) && (!ap_isdigit(u[x])) &&
	     (u[x] != '+') && (u[x] != '-') && (u[x] != '.'))) {
	    return 0;
	}
    }

    return (x ? 1 : 0);		/* If the first character is ':', it's broken, too */
}

#ifndef HAVE_INITGROUPS
int initgroups(const char *name, gid_t basegid)
{
#if defined(QNX) || defined(MPE) || defined(BEOS) || defined(_OSD_POSIX) || defined(TPF) || defined(__TANDEM) || defined(OS2) || defined(WIN32)
/* QNX, MPE and BeOS do not appear to support supplementary groups. */
    return 0;
#else /* ndef QNX */
    gid_t groups[NGROUPS_MAX];
    struct group *g;
    int index = 0;

    setgrent();

    groups[index++] = basegid;

    while (index < NGROUPS_MAX && ((g = getgrent()) != NULL))
	if (g->gr_gid != basegid) {
	    char **names;

	    for (names = g->gr_mem; *names != NULL; ++names)
		if (!strcmp(*names, name))
		    groups[index++] = g->gr_gid;
	}

    endgrent();

    return setgroups(index, groups);
#endif /* def QNX */
}
#endif /* def NEED_INITGROUPS */

API_EXPORT(int) ap_ind(const char *s, char c)
{
    register int x;

    for (x = 0; s[x]; x++)
	if (s[x] == c)
	    return x;

    return -1;
}

API_EXPORT(int) ap_rind(const char *s, char c)
{
    register int x;

    for (x = strlen(s) - 1; x != -1; x--)
	if (s[x] == c)
	    return x;

    return -1;
}

API_EXPORT(void) ap_str_tolower(char *str)
{
    while (*str) {
	*str = ap_tolower(*str);
	++str;
    }
}

API_EXPORT(uid_t) ap_uname2id(const char *name)
{
#ifdef WIN32
    return (1);
#else
    struct passwd *ent;

    if (name[0] == '#')
	return (atoi(&name[1]));

    if (!(ent = getpwnam(name))) {
	ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, NULL, "%s: bad user name %s", ap_server_argv0, name);
	exit(1);
    }
    return (ent->pw_uid);
#endif
}

API_EXPORT(gid_t) ap_gname2id(const char *name)
{
#ifdef WIN32
    return (1);
#else
    struct group *ent;

    if (name[0] == '#')
	return (atoi(&name[1]));

    if (!(ent = getgrnam(name))) {
	ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, NULL, "%s: bad group name %s", ap_server_argv0, name);
	exit(1);
    }
    return (ent->gr_gid);
#endif
}


/*
 * Parses a host of the form <address>[:port]
 * :port is permitted if 'port' is not NULL
 */
unsigned long ap_get_virthost_addr(char *w, unsigned short *ports)
{
    struct hostent *hep;
    unsigned long my_addr;
    char *p;

    p = strchr(w, ':');
    if (ports != NULL) {
	*ports = 0;
	if (p != NULL && strcmp(p + 1, "*") != 0)
	    *ports = atoi(p + 1);
    }

    if (p != NULL)
	*p = '\0';
    if (strcmp(w, "*") == 0) {
	if (p != NULL)
	    *p = ':';
	return htonl(INADDR_ANY);
    }

    my_addr = ap_inet_addr((char *)w);
    if (my_addr != INADDR_NONE) {
	if (p != NULL)
	    *p = ':';
	return my_addr;
    }

    hep = gethostbyname(w);

    if ((!hep) || (hep->h_addrtype != AF_INET || !hep->h_addr_list[0])) {
	ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, NULL, "Cannot resolve host name %s --- exiting!", w);
	exit(1);
    }

    if (hep->h_addr_list[1]) {
	ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, NULL, "Host %s has multiple addresses ---", w);
	ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, NULL, "you must choose one explicitly for use as");
	ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_NOERRNO, 0, NULL, "a virtual host.  Exiting!!!");
	exit(1);
    }

    if (p != NULL)
	*p = ':';

    return ((struct in_addr *) (hep->h_addr))->s_addr;
}


static char *find_fqdn(ap_pool_t *a, struct hostent *p)
{
    int x;

    if (!strchr(p->h_name, '.')) {
	for (x = 0; p->h_aliases[x]; ++x) {
	    if (strchr(p->h_aliases[x], '.') &&
		(!strncasecmp(p->h_aliases[x], p->h_name, strlen(p->h_name))))
		return ap_pstrdup(a, p->h_aliases[x]);
	}
	return NULL;
    }
    return ap_pstrdup(a, (void *) p->h_name);
}

char *ap_get_local_host(ap_pool_t *a)
{
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif
    char str[MAXHOSTNAMELEN + 1];
    char *server_hostname = NULL;
    struct hostent *p;

#ifdef BEOS
    if (gethostname(str, sizeof(str) - 1) == 0)
#else
    if (gethostname(str, sizeof(str) - 1) != 0)
#endif
    {
        ap_log_error(APLOG_MARK, APLOG_STARTUP | APLOG_WARNING, 0, NULL,
                     "%s: gethostname() failed to detemine ServerName\n",
                     ap_server_argv0);
    }
    else 
    {
        str[sizeof(str) - 1] = '\0';
        if ((!(p = gethostbyname(str))) 
            || (!(server_hostname = find_fqdn(a, p)))) {
            /* Recovery - return the default servername by IP: */
            if (!str && p->h_addr_list[0]) {
                ap_snprintf(str, sizeof(str), "%pA", p->h_addr_list[0]);
	        server_hostname = ap_pstrdup(a, str);
            }
        }
    }

    if (!server_hostname) 
        server_hostname = ap_pstrdup(a, "127.0.0.1");

    ap_log_error(APLOG_MARK, APLOG_ALERT | APLOG_NOERRNO, 0,
                 NULL, "%s: Missing ServerName directive in httpd.conf.",
                 ap_server_argv0);
    ap_log_error(APLOG_MARK, APLOG_ALERT | APLOG_NOERRNO, 0,
                 NULL, "%s: assumed ServerName of %s",
                 ap_server_argv0, server_hostname);
             
    return server_hostname;
}

/* simple 'pool' alloc()ing glue to ap_base64.c
 */
API_EXPORT(char *) ap_pbase64decode(ap_pool_t *p, const char *bufcoded)
{
    char *decoded;
    int l;

    decoded = (char *) ap_palloc(p, 1 + ap_base64decode_len(bufcoded));
    l = ap_base64decode(decoded, bufcoded);
    decoded[l] = '\0'; /* make binary sequence into string */

    return decoded;
}

API_EXPORT(char *) ap_pbase64encode(ap_pool_t *p, char *string) 
{ 
    char *encoded;
    int l = strlen(string);

    encoded = (char *) ap_palloc(p, 1 + ap_base64encode_len(l));
    l = ap_base64encode(encoded, string, l);
    encoded[l] = '\0'; /* make binary sequence into string */

    return encoded;
}

/* deprecated names for the above two functions, here for compatibility
 */
API_EXPORT(char *) ap_uudecode(ap_pool_t *p, const char *bufcoded)
{
    return ap_pbase64decode(p, bufcoded);
}

API_EXPORT(char *) ap_uuencode(ap_pool_t *p, char *string) 
{ 
    return ap_pbase64encode(p, string);
}

/* we want to downcase the type/subtype for comparison purposes
 * but nothing else because ;parameter=foo values are case sensitive.
 * XXX: in truth we want to downcase parameter names... but really,
 * apache has never handled parameters and such correctly.  You
 * also need to compress spaces and such to be able to compare
 * properly. -djg
 */
API_EXPORT(void) ap_content_type_tolower(char *str)
{
    char *semi;

    semi = strchr(str, ';');
    if (semi) {
	*semi = '\0';
    }
    while (*str) {
	*str = ap_tolower(*str);
	++str;
    }
    if (semi) {
	*semi = ';';
    }
}

/*
 * Given a string, replace any bare " with \" .
 */
API_EXPORT(char *) ap_escape_quotes (ap_pool_t *p, const char *instring)
{
    int newlen = 0;
    const char *inchr = instring;
    char *outchr, *outstring;

    /*
     * Look through the input string, jogging the length of the output
     * string up by an extra byte each time we find an unescaped ".
     */
    while (*inchr != '\0') {
	newlen++;
        if (*inchr == '"') {
	    newlen++;
	}
	/*
	 * If we find a slosh, and it's not the last byte in the string,
	 * it's escaping something - advance past both bytes.
	 */
	if ((*inchr == '\\') && (inchr[1] != '\0')) {
	    inchr++;
	    newlen++;
	}
	inchr++;
    }
    outstring = ap_palloc(p, newlen + 1);
    inchr = instring;
    outchr = outstring;
    /*
     * Now copy the input string to the output string, inserting a slosh
     * in front of every " that doesn't already have one.
     */
    while (*inchr != '\0') {
	if ((*inchr == '\\') && (inchr[1] != '\0')) {
	    *outchr++ = *inchr++;
	    *outchr++ = *inchr++;
	}
	if (*inchr == '"') {
	    *outchr++ = '\\';
	}
	if (*inchr != '\0') {
	    *outchr++ = *inchr++;
	}
    }
    *outchr = '\0';
    return outstring;
}

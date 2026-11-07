/* port/net/net_upnp.c: what UPnP IGD's client reads and writes, without sockets (net_nat.h; net_nat.c talks): SSDP's
 * search and its answer, the URLs, a small XML reader for the device description and SOAP's answers, SOAP's request
 * body, and an HTTP answer. Written from the UPnP Device Architecture 1.1 (SSDP, description, control), the
 * WANIPConnection:1 and :2 service templates, RFC 7230 (HTTP/1.1's message syntax, chunked) and XML 1.0.
 *
 * Everything here reads bytes from the network: bounded loops, every length checked against the buffer and the
 * output, a field longer than its room an error (never cut), no recursion, no DTD or external entity: only XML's five
 * named entities and ASCII character references are decoded. */
#include "net_nat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XML_NAME_MAX 128      /* an element's name, with its prefix */
#define XML_TAGS_MAX 20000    /* tags looked at for one element's end: a hostile document can't make it quadratic */
#define IGD_SERVICES_MAX 64   /* <service> elements looked at */
#define HTTP_HEADERS_MAX 16384

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int prefix_ci(const char *s, size_t n, const char *p)
{
    size_t k = strlen(p);
    if (n < k)
        return 0;
    for (size_t i = 0; i < k; i++)
        if (lower((unsigned char)s[i]) != lower((unsigned char)p[i]))
            return 0;
    return 1;
}

static int eq_ci(const char *a, size_t an, const char *b)
{
    return an == strlen(b) && prefix_ci(a, an, b);
}

/* ---------------------------------------------------------------- SSDP */

int net_ssdp_search(char *buf, int cap, const char *host_port, const char *st, int mx)
{
    int n = snprintf(buf, (size_t)cap,
                     "M-SEARCH * HTTP/1.1\r\nHOST: %s\r\nMAN: \"ssdp:discover\"\r\nMX: %d\r\nST: %s\r\n"
                     "USER-AGENT: Rush2049PC UPnP/1.1 Rush2049PC/1\r\n\r\n",
                     host_port, mx, st);
    return n > 0 && n < cap ? n : -1;
}

/* A header's value, the spaces around it trimmed, into out: 0, or -1 (a control character, too long). */
static int header_value(const char *v, size_t n, char *out, size_t out_n)
{
    while (n && (*v == ' ' || *v == '\t'))
        v++, n--;
    while (n && (v[n - 1] == ' ' || v[n - 1] == '\t' || v[n - 1] == '\r'))
        n--;
    if (n + 1 > out_n)
        return -1;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)v[i] < 0x20 || (unsigned char)v[i] > 0x7E)
            return -1;
    memcpy(out, v, n);
    out[n] = 0;
    return 0;
}

int net_ssdp_parse(const char *buf, int len, char *location, size_t location_n, char *st, size_t st_n)
{
    size_t pos = 0, n = len > 0 ? (size_t)len : 0;
    int have_loc = 0;
    if (!location_n || !st_n)
        return -1;
    location[0] = st[0] = 0;
    /* the status line: HTTP/1.x 200 */
    if (n < 12 || !prefix_ci(buf, n, "HTTP/1.") || !isdigit((unsigned char)buf[7]) || buf[8] != ' ' ||
        memcmp(buf + 9, "200", 3) != 0)
        return -1;
    const char *eol = memchr(buf, '\n', n);
    if (!eol)
        return -1;
    pos = (size_t)(eol - buf) + 1;
    while (pos < n) {
        const char *l = buf + pos, *e = memchr(l, '\n', n - pos);
        size_t ll = e ? (size_t)(e - l) : n - pos;
        pos += ll + 1;
        if (ll && l[ll - 1] == '\r')
            ll--;
        if (!ll)
            break;   /* the headers' end */
        const char *colon = memchr(l, ':', ll);
        if (!colon)
            continue;   /* not a header: skipped */
        size_t kn = (size_t)(colon - l);
        while (kn && (l[kn - 1] == ' ' || l[kn - 1] == '\t'))
            kn--;
        if (eq_ci(l, kn, "LOCATION")) {
            if (have_loc || header_value(colon + 1, (size_t)(l + ll - colon - 1), location, location_n))
                return -1;   /* two, or one that doesn't fit: not trusted */
            have_loc = 1;
        } else if (eq_ci(l, kn, "ST")) {
            if (header_value(colon + 1, (size_t)(l + ll - colon - 1), st, st_n))
                return -1;
        }
    }
    return have_loc && location[0] ? 0 : -1;
}

/* ---------------------------------------------------------------- URLs */

/* A dotted quad, exactly: four numbers 0-255 of 1-3 digits. Its length, or -1. */
static int parse_ipv4(const char *s, size_t n, uint8_t out[4])
{
    size_t i = 0;
    for (int part = 0; part < 4; part++) {
        int v = 0, digits = 0;
        while (i < n && isdigit((unsigned char)s[i]) && digits < 4)
            v = v * 10 + (s[i++] - '0'), digits++;
        if (!digits || digits > 3 || v > 255)
            return -1;
        out[part] = (uint8_t)v;
        if (part < 3) {
            if (i >= n || s[i] != '.')
                return -1;
            i++;
        }
    }
    return (int)i;
}

static int path_ok(const char *p, size_t n)
{
    if (!n || p[0] != '/' || n >= sizeof(((NetUrl *)0)->path))
        return 0;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)p[i] <= 0x20 || (unsigned char)p[i] > 0x7E || p[i] == '\\' || p[i] == '#')
            return 0;
    return 1;
}

int net_url_parse(const char *url, NetUrl *u)
{
    size_t n = strlen(url), i;
    int k;
    memset(u, 0, sizeof *u);
    if (n > 512 || !prefix_ci(url, n, "http://"))
        return -1;
    i = 7;
    k = parse_ipv4(url + i, n - i, u->addr.addr);
    if (k < 0)
        return -1;
    i += (size_t)k;
    u->addr.family = NET_AF_INET;
    u->addr.port = 80;
    if (i < n && url[i] == ':') {
        long p = 0;
        int digits = 0;
        i++;
        while (i < n && isdigit((unsigned char)url[i]) && digits < 6)
            p = p * 10 + (url[i++] - '0'), digits++;
        if (!digits || p < 1 || p > 65535)
            return -1;
        u->addr.port = (uint16_t)p;
    }
    if (i == n) {
        snprintf(u->path, sizeof u->path, "/");
        return 0;
    }
    if (!path_ok(url + i, n - i))
        return -1;
    memcpy(u->path, url + i, n - i);
    u->path[n - i] = 0;
    return 0;
}

int net_url_resolve(const NetUrl *base, const char *ref, NetUrl *out)
{
    size_t n = strlen(ref);
    if (!n || n > 512)
        return -1;
    if (prefix_ci(ref, n, "http://"))
        return net_url_parse(ref, out);
    if (ref[strcspn(ref, ":/?#")] == ':')
        return -1;   /* another scheme: https:, javascript:... */
    if (ref[0] == '/' && n > 1 && ref[1] == '/')
        return -1;   /* //host/path: another host */
    *out = *base;
    if (ref[0] == '/') {
        if (!path_ok(ref, n))
            return -1;
        memcpy(out->path, ref, n + 1);
        return 0;
    }
    const char *slash = strrchr(base->path, '/');
    size_t dir = slash ? (size_t)(slash - base->path) + 1 : 1;
    char path[512];
    if (dir + n + 1 > sizeof path)
        return -1;
    memcpy(path, base->path, dir);
    if (!slash)
        path[0] = '/';
    memcpy(path + dir, ref, n + 1);
    if (!path_ok(path, dir + n))
        return -1;
    memcpy(out->path, path, dir + n + 1);
    return 0;
}

/* ---------------------------------------------------------------- XML */

enum { TAG_OTHER, TAG_START, TAG_END, TAG_EMPTY };

typedef struct {
    size_t start, end;   /* its '<', and just past its '>' */
    int kind;
    size_t local, local_n;   /* the local name (after a prefix's ':') */
} Tag;

/* The first occurrence of the ASCII string s in d[from, len): its index, or len. */
static size_t find(const char *d, size_t len, size_t from, const char *s)
{
    size_t k = strlen(s);
    for (size_t i = from; i + k <= len; i++) {
        const char *p = memchr(d + i, s[0], len - i);
        if (!p)
            return len;
        i = (size_t)(p - d);
        if (i + k <= len && !memcmp(d + i, s, k))
            return i;
    }
    return len;
}

static int name_start(int c)
{
    return isalpha(c) || c == '_' || c == ':';
}

static int name_char(int c)
{
    return isalnum(c) || c == '_' || c == ':' || c == '-' || c == '.';
}

/* The next tag at or after pos. 0, or -1 when there is none or it is malformed (the reader stops there). */
static int next_tag(const char *d, size_t len, size_t pos, Tag *t)
{
    const char *lt = pos < len ? memchr(d + pos, '<', len - pos) : NULL;
    size_t i, e;
    if (!lt)
        return -1;
    i = (size_t)(lt - d);
    memset(t, 0, sizeof *t);
    t->start = i;
    if (i + 4 <= len && !memcmp(d + i, "<!--", 4)) {
        e = find(d, len, i + 4, "-->");
        if (e >= len)
            return -1;
        t->end = e + 3;
        return 0;
    }
    if (i + 9 <= len && !memcmp(d + i, "<![CDATA[", 9)) {
        e = find(d, len, i + 9, "]]>");
        if (e >= len)
            return -1;
        t->end = e + 3;
        return 0;
    }
    if (i + 1 < len && (d[i + 1] == '?' || d[i + 1] == '!')) {   /* a declaration: <?xml ...?>, <!DOCTYPE ...> */
        const char *gt = memchr(d + i, '>', len - i);
        if (!gt)
            return -1;
        t->end = (size_t)(gt - d) + 1;   /* a DOCTYPE's internal subset isn't read: nothing in it is used */
        return 0;
    }
    i++;
    if (i < len && d[i] == '/') {
        t->kind = TAG_END;
        i++;
    }
    size_t n0 = i;
    if (i >= len || !name_start((unsigned char)d[i]))
        return -1;
    while (i < len && name_char((unsigned char)d[i]) && i - n0 <= XML_NAME_MAX)
        i++;
    if (i - n0 > XML_NAME_MAX || i >= len)
        return -1;
    t->local = n0;
    for (size_t k = n0; k < i; k++)
        if (d[k] == ':')
            t->local = k + 1;
    t->local_n = i - t->local;
    if (!t->local_n)
        return -1;
    if (t->kind == TAG_END) {
        while (i < len && isspace((unsigned char)d[i]))
            i++;
        if (i >= len || d[i] != '>')
            return -1;
        t->end = i + 1;
        return 0;
    }
    if (!isspace((unsigned char)d[i]) && d[i] != '>' && d[i] != '/')
        return -1;
    for (char q = 0; i < len; i++) {   /* attributes, quoted values may hold '>' */
        char c = d[i];
        if (q) {
            if (c == q)
                q = 0;
        } else if (c == '"' || c == '\'') {
            q = c;
        } else if (c == '>') {
            t->kind = d[i - 1] == '/' ? TAG_EMPTY : TAG_START;
            t->end = i + 1;
            return 0;
        } else if (c == '<') {
            return -1;
        }
    }
    return -1;
}

static int tag_is(const char *d, const Tag *t, const char *name)
{
    return eq_ci(d + t->local, t->local_n, name);
}

int net_xml_span(const char *doc, size_t len, const char *name, size_t *from, size_t *start, size_t *end)
{
    Tag t, u;
    size_t pos = *from;
    for (int guard = 0; guard < XML_TAGS_MAX; guard++) {
        if (next_tag(doc, len, pos, &t))
            return -1;
        pos = t.end;
        if ((t.kind != TAG_START && t.kind != TAG_EMPTY) || !tag_is(doc, &t, name))
            continue;
        if (t.kind == TAG_EMPTY) {
            *start = *end = *from = t.end;
            return 0;
        }
        int depth = 1;
        for (int g2 = 0; g2 < XML_TAGS_MAX; g2++) {
            if (next_tag(doc, len, pos, &u))
                return -1;   /* no end tag */
            pos = u.end;
            if (!tag_is(doc, &u, name))
                continue;
            if (u.kind == TAG_START)
                depth++;
            else if (u.kind == TAG_END && --depth == 0) {
                *start = t.end;
                *end = u.start;
                *from = u.end;
                return 0;
            }
        }
        return -1;
    }
    return -1;
}

/* An element's text: strict (non-ASCII an error) or lenient (each such byte a '?': a name to show). */
static int xml_text(const char *doc, size_t len, const char *name, size_t *from, char *out, size_t n, int lenient)
{
    size_t s, e, o = 0;
    if (!n || net_xml_span(doc, len, name, from, &s, &e))
        return -1;
    while (s < e && isspace((unsigned char)doc[s]))
        s++;
    while (e > s && isspace((unsigned char)doc[e - 1]))
        e--;
    if (e - s >= 12 && !memcmp(doc + s, "<![CDATA[", 9) && !memcmp(doc + e - 3, "]]>", 3)) {
        s += 9, e -= 3;   /* the text as it is */
        if (e - s + 1 > n)
            return -1;
        for (size_t i = s; i < e; i++) {
            unsigned char c = (unsigned char)doc[i];
            if (c < 0x20 || c > 0x7E) {
                if (!lenient)
                    return -1;
                c = '?';
            }
            out[o++] = (char)c;
        }
        out[o] = 0;
        return 0;
    }
    for (size_t i = s; i < e; i++) {
        unsigned char c = (unsigned char)doc[i];
        if (c == '<')
            return -1;   /* an element inside it: not a text */
        if (c == '&') {
            const char *semi = memchr(doc + i, ';', e - i < 12 ? e - i : 12);
            size_t k;
            long v = -1;
            if (!semi)
                return -1;
            k = (size_t)(semi - (doc + i)) + 1;
            if (k == 5 && !memcmp(doc + i, "&amp;", 5))
                v = '&';
            else if (k == 4 && !memcmp(doc + i, "&lt;", 4))
                v = '<';
            else if (k == 4 && !memcmp(doc + i, "&gt;", 4))
                v = '>';
            else if (k == 6 && !memcmp(doc + i, "&quot;", 6))
                v = '"';
            else if (k == 6 && !memcmp(doc + i, "&apos;", 6))
                v = '\'';
            else if (k >= 4 && doc[i + 1] == '#') {
                int hex = doc[i + 2] == 'x' || doc[i + 2] == 'X';
                size_t j = i + 2 + (size_t)hex;
                v = 0;
                if (j >= i + k - 1)
                    return -1;
                for (; j < i + k - 1; j++) {
                    int c2 = (unsigned char)doc[j];
                    int dv = isdigit(c2) ? c2 - '0' : hex && isxdigit(c2) ? lower(c2) - 'a' + 10 : -1;
                    if (dv < 0 || v > 0x10FFFF)
                        return -1;
                    v = v * (hex ? 16 : 10) + dv;
                }
                if (v < 0x20 || v > 0x7E) {
                    if (!lenient && v != '\t' && v != '\n' && v != '\r')
                        return -1;
                    v = v == '\t' || v == '\n' || v == '\r' ? ' ' : '?';
                }
            } else {
                return -1;   /* an entity a DTD would define: never */
            }
            if (o + 2 > n)
                return -1;
            out[o++] = (char)v;
            i += k - 1;
            continue;
        }
        if (c == '\t' || c == '\n' || c == '\r')
            c = ' ';
        else if (c < 0x20 || c > 0x7E) {
            if (!lenient)
                return -1;
            c = '?';
        }
        if (o + 2 > n)
            return -1;
        out[o++] = (char)c;
    }
    out[o] = 0;
    return 0;
}

int net_xml_text(const char *doc, size_t len, const char *name, size_t *from, char *out, size_t n)
{
    return xml_text(doc, len, name, from, out, n, 0);
}

/* ---------------------------------------------------------------- the device description */

static const char *const k_wan[] = {
    "urn:schemas-upnp-org:service:WANIPConnection:2",
    "urn:schemas-upnp-org:service:WANIPConnection:1",
    "urn:schemas-upnp-org:service:WANPPPConnection:1",
};

int net_igd_parse(const char *xml, size_t len, const NetUrl *location, NetIgdService *out, int max, char *name,
                  size_t name_n)
{
    NetUrl base = *location, u;
    char text[600];
    size_t from = 0, s, e;
    int count = 0, rank[16];
    if (name_n)
        name[0] = 0;
    if (max > 16)
        max = 16;
    from = 0;
    if (name_n && xml_text(xml, len, "friendlyName", &from, text, sizeof text, 1) == 0)
        snprintf(name, name_n, "%s", text);
    from = 0;
    if (net_xml_text(xml, len, "URLBase", &from, text, sizeof text) == 0 && text[0] &&
        net_url_parse(text, &u) == 0 && !memcmp(u.addr.addr, location->addr.addr, 4))
        base = u;   /* UPnP 1.0's; on the device's own address only */
    from = 0;
    for (int k = 0; k < IGD_SERVICES_MAX && count < max; k++) {
        char type[128], ctl[600];
        size_t f2 = 0;
        int r = -1;
        if (net_xml_span(xml, len, "service", &from, &s, &e))
            break;
        if (net_xml_text(xml + s, e - s, "serviceType", &f2, type, sizeof type))
            continue;
        for (int i = 0; i < 3; i++)
            if (!strcmp(type, k_wan[i]))
                r = i;
        f2 = 0;
        if (r < 0 || net_xml_text(xml + s, e - s, "controlURL", &f2, ctl, sizeof ctl) || !ctl[0])
            continue;
        if (net_url_resolve(&base, ctl, &u) || memcmp(u.addr.addr, location->addr.addr, 4))
            continue;   /* not on the device that answered: never followed */
        snprintf(out[count].type, sizeof out[count].type, "%.95s", type);
        out[count].control = u;
        rank[count++] = r;
    }
    for (int i = 1; i < count; i++)   /* best first, stable */
        for (int j = i; j > 0 && rank[j] < rank[j - 1]; j--) {
            NetIgdService t = out[j];
            int tr = rank[j];
            out[j] = out[j - 1], rank[j] = rank[j - 1];
            out[j - 1] = t, rank[j - 1] = tr;
        }
    return count;
}

/* ---------------------------------------------------------------- SOAP */

static int token_ok(const char *s)
{
    if (!*s)
        return 0;
    for (; *s; s++)
        if (!isalnum((unsigned char)*s) && !strchr(":-._/", *s))
            return 0;
    return 1;
}

int net_soap_body(char *buf, size_t cap, const char *type, const char *action, const char *const *names,
                  const char *const *values, int n)
{
    size_t o;
    int k;
    if (!token_ok(type) || !token_ok(action))
        return -1;
    k = snprintf(buf, cap,
                 "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                 "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:%s xmlns:u=\"%s\">",
                 action, type);
    if (k < 0 || (size_t)k >= cap)
        return -1;
    o = (size_t)k;
    for (int i = 0; i < n; i++) {
        if (!token_ok(names[i]))
            return -1;
        k = snprintf(buf + o, cap - o, "<%s>", names[i]);
        if (k < 0 || (size_t)k >= cap - o)
            return -1;
        o += (size_t)k;
        for (const char *v = values[i]; *v; v++) {
            const char *esc = *v == '&' ? "&amp;" : *v == '<' ? "&lt;" : *v == '>' ? "&gt;" : *v == '"' ? "&quot;"
                              : *v == '\'' ? "&apos;" : NULL;
            size_t el = esc ? strlen(esc) : 1;
            if (o + el + 1 > cap)
                return -1;
            if (esc)
                memcpy(buf + o, esc, el);
            else
                buf[o] = *v;
            o += el;
        }
        k = snprintf(buf + o, cap - o, "</%s>", names[i]);
        if (k < 0 || (size_t)k >= cap - o)
            return -1;
        o += (size_t)k;
    }
    k = snprintf(buf + o, cap - o, "</u:%s></s:Body></s:Envelope>\r\n", action);
    if (k < 0 || (size_t)k >= cap - o)
        return -1;
    return (int)(o + (size_t)k);
}

int net_soap_fault(const char *body, size_t len, int *code, char *desc, size_t desc_n)
{
    size_t from = 0, s, e, f2;
    char num[16];
    if (desc_n)
        desc[0] = 0;
    if (net_xml_span(body, len, "UPnPError", &from, &s, &e))
        return -1;
    f2 = 0;
    if (net_xml_text(body + s, e - s, "errorCode", &f2, num, sizeof num) || !num[0] || strspn(num, "0123456789") !=
        strlen(num) || strlen(num) > 6)
        return -1;
    *code = atoi(num);
    f2 = 0;
    if (desc_n && xml_text(body + s, e - s, "errorDescription", &f2, desc, desc_n, 1) != 0)
        desc[0] = 0;
    return 0;
}

/* ---------------------------------------------------------------- an HTTP answer */

int net_http_parse(char *buf, size_t len, int *status, char **body, size_t *body_len)
{
    size_t hdr_end = 0, pos;
    long long clen = -1;
    int chunked = 0;
    if (len < 12 || !prefix_ci(buf, len, "HTTP/1.") || !isdigit((unsigned char)buf[7]) || buf[8] != ' ' ||
        !isdigit((unsigned char)buf[9]) || !isdigit((unsigned char)buf[10]) || !isdigit((unsigned char)buf[11]))
        return -1;
    *status = (buf[9] - '0') * 100 + (buf[10] - '0') * 10 + (buf[11] - '0');
    for (size_t i = 0; i + 1 < len && i < HTTP_HEADERS_MAX; i++)   /* the headers' end: an empty line */
        if (buf[i] == '\n' && (buf[i + 1] == '\n' || (buf[i + 1] == '\r' && i + 2 < len && buf[i + 2] == '\n'))) {
            hdr_end = i + (buf[i + 1] == '\n' ? 2 : 3);
            break;
        }
    if (!hdr_end)
        return -1;
    for (pos = (size_t)((const char *)memchr(buf, '\n', len) - buf) + 1; pos < hdr_end;) {
        char *l = buf + pos, *nl = memchr(l, '\n', hdr_end - pos);
        size_t ll = nl ? (size_t)(nl - l) : hdr_end - pos;
        pos += ll + 1;
        if (ll && l[ll - 1] == '\r')
            ll--;
        char *colon = memchr(l, ':', ll);
        if (!colon)
            continue;
        size_t kn = (size_t)(colon - l), vn = ll - kn - 1;
        char *v = colon + 1;
        while (vn && (*v == ' ' || *v == '\t'))
            v++, vn--;
        while (vn && (v[vn - 1] == ' ' || v[vn - 1] == '\t'))
            vn--;
        if (eq_ci(l, kn, "Content-Length")) {
            long long c = 0;
            if (!vn || vn > 9 || clen >= 0)
                return -1;   /* none, too big, or two */
            for (size_t i = 0; i < vn; i++) {
                if (!isdigit((unsigned char)v[i]))
                    return -1;
                c = c * 10 + (v[i] - '0');
            }
            clen = c;
        } else if (eq_ci(l, kn, "Transfer-Encoding")) {
            for (size_t i = 0; i + 7 <= vn; i++)
                if (prefix_ci(v + i, vn - i, "chunked"))
                    chunked = 1;
        }
    }
    char *b = buf + hdr_end;
    size_t avail = len - hdr_end;
    if (chunked) {   /* the chunks joined in place: the write position never passes the read one */
        size_t r = 0, w = 0;
        for (int guard = 0; guard < 100000; guard++) {
            size_t size = 0, digits = 0;
            while (r < avail && isxdigit((unsigned char)b[r])) {
                if (++digits > 7)
                    return -1;   /* past 256 MiB: hostile */
                int c = lower((unsigned char)b[r++]);
                size = size * 16 + (size_t)(isdigit(c) ? c - '0' : c - 'a' + 10);
            }
            if (!digits)
                return -1;
            const char *nl = r < avail ? memchr(b + r, '\n', avail - r) : NULL;   /* extensions skipped */
            if (!nl)
                return -1;
            r = (size_t)(nl - b) + 1;
            if (!size) {
                *body = b;
                *body_len = w;
                b[w] = 0;
                return 0;
            }
            if (size > avail - r)
                return -1;   /* cut short */
            memmove(b + w, b + r, size);
            w += size, r += size;
            if (r < avail && b[r] == '\r')
                r++;
            if (r >= avail || b[r] != '\n')
                return -1;
            r++;
        }
        return -1;
    }
    if (clen >= 0) {
        if ((size_t)clen > avail)
            return -1;   /* cut short */
        avail = (size_t)clen;
    }
    *body = b;
    *body_len = avail;
    b[avail] = 0;
    return 0;
}

int net_http_complete(const char *buf, size_t len)
{
    size_t hdr_end = 0;
    long long clen = -1;
    int chunked = 0;
    for (size_t i = 0; i + 1 < len && i < HTTP_HEADERS_MAX; i++)
        if (buf[i] == '\n' && (buf[i + 1] == '\n' || (buf[i + 1] == '\r' && i + 2 < len && buf[i + 2] == '\n'))) {
            hdr_end = i + (buf[i + 1] == '\n' ? 2 : 3);
            break;
        }
    if (!hdr_end)
        return 0;
    for (size_t pos = 0; pos < hdr_end;) {
        const char *l = buf + pos, *nl = memchr(l, '\n', hdr_end - pos);
        size_t ll = nl ? (size_t)(nl - l) : hdr_end - pos;
        pos += ll + 1;
        const char *colon = memchr(l, ':', ll);
        if (!colon)
            continue;
        size_t kn = (size_t)(colon - l), vn = ll - kn - 1;
        const char *v = colon + 1;
        while (vn && (*v == ' ' || *v == '\t'))
            v++, vn--;
        if (eq_ci(l, kn, "Content-Length")) {
            clen = 0;
            for (size_t i = 0; i < vn && i < 10 && isdigit((unsigned char)v[i]); i++)
                clen = clen * 10 + (v[i] - '0');
        } else if (eq_ci(l, kn, "Transfer-Encoding")) {
            for (size_t i = 0; i + 7 <= vn; i++)
                if (prefix_ci(v + i, vn - i, "chunked"))
                    chunked = 1;
        }
    }
    if (chunked) {   /* its last chunk: parsed on a copy (the parse joins the chunks in place) */
        char *copy = malloc(len + 1), *body;
        size_t bl;
        int status, ok;
        if (!copy)
            return 0;
        memcpy(copy, buf, len);
        ok = net_http_parse(copy, len, &status, &body, &bl) == 0;
        free(copy);
        return ok;
    }
    return clen >= 0 && len - hdr_end >= (unsigned long long)clen;
}

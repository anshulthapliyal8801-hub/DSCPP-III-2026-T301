/*
 * MUNSphere — Module 12: Persistence (C file-handling layer)
 *
 * Every module stores its records in text files with the same format, and
 * all of them use these helpers, so escaping, header checks and atomic
 * replacement are written (and tested) once.
 *
 * File format
 *   line 1 : #MUNSPHERE|<kind>|1          (kind and format version)
 *   then   : one record per line, fields separated by '|'
 *   escape : '\' -> "\\"  '|' -> "\p"  newline -> "\n"  CR -> "\r"
 *
 * Saving writes <file>.tmp and then renames it over <file>, so a crash in
 * the middle of a save leaves the previous file intact.
 *
 * AUDIT LOG: a dynamic array in memory and an APPEND-ONLY file. Each new
 * entry is one appended line, so recording an action never rewrites
 * history. A crash mid-append can leave a partial last line; loading skips
 * and counts it. Entries carry a hash chain (see AuditManager.cpp).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Our header first, then <windows.h>: Windows defines many short macro
 * names, and none of them may rewrite our declarations. */
#include "dsa_api.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

void pers_copy_field(char *dst, const char *src, size_t cap)
{
    size_t n;
    if (!dst || cap == 0) return;
    if (!src) src = "";
    n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

void pers_write_field(FILE *f, const char *s)
{
    if (!s) return;
    for (; *s; s++) {
        switch (*s) {
        case '\\': fputs("\\\\", f); break;
        case '|':  fputs("\\p", f);  break;
        case '\n': fputs("\\n", f);  break;
        case '\r': fputs("\\r", f);  break;
        default:   fputc(*s, f);
        }
    }
}

void pers_write_int(FILE *f, int v)
{
    fprintf(f, "%d", v);
}

/* Splits on unescaped '|' and unescapes in place.
 * Returns the number of fields, or -1 on a malformed escape / too many. */
int pers_split_fields(char *line, char **fields, int maxFields)
{
    char *r = line, *w = line;
    int n = 0;
    if (maxFields < 1) return -1;
    fields[n++] = w;
    while (*r) {
        if (*r == '\\') {
            r++;
            switch (*r) {
            case '\\': *w++ = '\\'; break;
            case 'p':  *w++ = '|';  break;
            case 'n':  *w++ = '\n'; break;
            case 'r':  *w++ = '\r'; break;
            default:   return -1;
            }
            r++;
        } else if (*r == '|') {
            *w++ = '\0';
            r++;
            if (n >= maxFields) return -1;
            fields[n++] = w;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    return n;
}

/* Non-negative integer up to 100,000,000; -1 if not a number. */
int pers_parse_int(const char *s)
{
    char *end;
    long v;
    if (!s || !*s) return -1;
    v = strtol(s, &end, 10);
    if (*end != '\0' || v < 0 || v > 100000000L) return -1;
    return (int)v;
}

/* 1 = line read, 0 = end of file, -1 = line longer than cap (discarded) */
int pers_read_line(FILE *f, char *buf, size_t cap)
{
    size_t len;
    if (!fgets(buf, (int)cap, f)) return 0;
    len = strlen(buf);
    if (len > 0 && buf[len - 1] != '\n' && !feof(f)) {
        int c;
        while ((c = fgetc(f)) != EOF && c != '\n') { }
        return -1;
    }
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
    return 1;
}

/* Opens a file for loading and checks its header.
 * DSA_OK with *out == NULL means the file doesn't exist (empty store).
 * DSA_ERR_INVALID means the file exists but isn't this kind of file. */
int pers_open_for_load(const char *path, const char *kind, FILE **out)
{
    char line[PERS_LINE_MAX], expected[80];
    FILE *f;
    *out = NULL;
    if (!path || !kind) return DSA_ERR_INVALID;
    f = fopen(path, "rb");
    if (!f) return DSA_OK;
    snprintf(expected, sizeof expected, "#MUNSPHERE|%s|1", kind);
    if (pers_read_line(f, line, sizeof line) != 1 || strcmp(line, expected) != 0) {
        fclose(f);
        return DSA_ERR_INVALID;
    }
    *out = f;
    return DSA_OK;
}

/* Opens <path>.tmp for writing and writes the header.
 * tmpOut receives the temp path (needed by pers_finish_save). */
FILE *pers_begin_save(const char *path, const char *kind, char *tmpOut, size_t tmpCap)
{
    FILE *f;
    if (!path || !kind || !tmpOut) return NULL;
    if (strlen(path) + 5 > tmpCap) return NULL;
    snprintf(tmpOut, tmpCap, "%s.tmp", path);
    f = fopen(tmpOut, "wb");
    if (!f) return NULL;
    fprintf(f, "#MUNSPHERE|%s|1\n", kind);
    return f;
}

static int replace_file(const char *tmp, const char *dst)
{
#ifdef _WIN32
    return MoveFileExA(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
               ? 0 : -1;
#else
    return rename(tmp, dst);
#endif
}

/* Flushes and closes the temp file, then atomically replaces the target.
 * On any failure the temp file is removed and the target is untouched. */
int pers_finish_save(FILE *f, const char *tmpPath, const char *path)
{
    int bad = (fflush(f) != 0) || ferror(f);
    if (fclose(f) != 0) bad = 1;
    if (bad) { remove(tmpPath); return DSA_ERR_IO; }
    if (replace_file(tmpPath, path) != 0) { remove(tmpPath); return DSA_ERR_IO; }
    return DSA_OK;
}

/* Copies a file byte for byte (used by backup and restore). */
int pers_copy_file(const char *src, const char *dst)
{
    char buf[8192], tmp[DSA_PATH_LEN + 8];
    size_t n;
    FILE *in, *out;
    if (!src || !dst || strlen(dst) + 5 > sizeof tmp) return DSA_ERR_INVALID;
    in = fopen(src, "rb");
    if (!in) return DSA_ERR_NOT_FOUND;
    snprintf(tmp, sizeof tmp, "%s.tmp", dst);
    out = fopen(tmp, "wb");
    if (!out) { fclose(in); return DSA_ERR_IO; }
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) { fclose(in); fclose(out); remove(tmp); return DSA_ERR_IO; }
    }
    if (ferror(in)) { fclose(in); fclose(out); remove(tmp); return DSA_ERR_IO; }
    fclose(in);
    return pers_finish_save(out, tmp, dst);
}

/* ============================================================ audit log */

static AuditRec *g_audit;
static int g_auditCount, g_auditCap;

void audit_store_reset(void)
{
    free(g_audit);
    g_audit = NULL;
    g_auditCount = g_auditCap = 0;
}

static int audit_push(const AuditRec *rec)
{
    if (g_auditCount == g_auditCap) {
        int cap = g_auditCap ? g_auditCap * 2 : 64;
        AuditRec *bigger = (AuditRec *)realloc(g_audit, (size_t)cap * sizeof *bigger);
        if (!bigger) return DSA_ERR_NO_MEMORY;
        g_audit = bigger;
        g_auditCap = cap;
    }
    g_audit[g_auditCount++] = *rec;
    return DSA_OK;
}

static void audit_write_line(FILE *f, const AuditRec *a)
{
    fprintf(f, "%ld|", a->seq);
    pers_write_field(f, a->at); fputc('|', f);
    pers_write_field(f, a->actor); fputc('|', f);
    pers_write_field(f, a->role); fputc('|', f);
    pers_write_field(f, a->action); fputc('|', f);
    pers_write_field(f, a->target); fputc('|', f);
    pers_write_int(f, a->status); fputc('|', f);
    pers_write_field(f, a->prevHash); fputc('|', f);
    pers_write_field(f, a->hash); fputc('\n', f);
}

int audit_append(const AuditRec *rec, const char *path)
{
    int rc;
    if (!rec || !rec->action[0]) return DSA_ERR_INVALID;
    if ((rc = audit_push(rec)) != DSA_OK) return rc;
    if (path) {
        FILE *f = fopen(path, "ab");
        if (!f) return DSA_ERR_IO;
        /* The position right after fopen("ab") is implementation-defined
         * (MSVCRT can report 0), so seek to the end before asking. */
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return DSA_ERR_IO; }
        if (ftell(f) == 0) fputs("#MUNSPHERE|audit_log|1\n", f);   /* new file */
        audit_write_line(f, rec);
        if (fflush(f) != 0) { fclose(f); return DSA_ERR_IO; }
        if (fclose(f) != 0) return DSA_ERR_IO;
    }
    return DSA_OK;
}

int audit_count(void) { return g_auditCount; }

int audit_get(int index, AuditRec *out)
{
    if (index < 0 || index >= g_auditCount) return DSA_ERR_NOT_FOUND;
    if (out) *out = g_audit[index];
    return DSA_OK;
}

long audit_next_seq(void) { return g_auditCount ? g_audit[g_auditCount - 1].seq + 1 : 1; }

int audit_save(const char *path)
{
    char tmp[DSA_PATH_LEN + 8];
    int i;
    FILE *f = pers_begin_save(path, "audit_log", tmp, sizeof tmp);
    if (!f) return DSA_ERR_IO;
    for (i = 0; i < g_auditCount; i++) audit_write_line(f, &g_audit[i]);
    return pers_finish_save(f, tmp, path);
}

int audit_load(const char *path, int *skipped)
{
    char line[PERS_LINE_MAX];
    char *fld[PERS_MAX_FIELDS];
    FILE *f;
    int bad = 0, r, rc;
    if ((rc = pers_open_for_load(path, "audit_log", &f)) != DSA_OK) return rc;
    audit_store_reset();
    if (!f) return DSA_OK;
    while ((r = pers_read_line(f, line, sizeof line)) != 0) {
        AuditRec a;
        char *end;
        if (r < 0) { bad++; continue; }
        if (!line[0]) continue;
        memset(&a, 0, sizeof a);
        if (pers_split_fields(line, fld, PERS_MAX_FIELDS) != 9) { bad++; continue; }   /* e.g. a torn last line */
        a.seq = strtol(fld[0], &end, 10);
        if (*end || a.seq < 1) { bad++; continue; }
        pers_copy_field(a.at, fld[1], DSA_STAMP_LEN);
        pers_copy_field(a.actor, fld[2], DSA_ID_LEN);
        pers_copy_field(a.role, fld[3], sizeof a.role);
        pers_copy_field(a.action, fld[4], AUDIT_ACTION_LEN);
        pers_copy_field(a.target, fld[5], AUDIT_TARGET_LEN);
        a.status = pers_parse_int(fld[6]);
        pers_copy_field(a.prevHash, fld[7], AUDIT_HASH_LEN);
        pers_copy_field(a.hash, fld[8], AUDIT_HASH_LEN);
        if (audit_push(&a) != DSA_OK) { fclose(f); return DSA_ERR_NO_MEMORY; }
    }
    fclose(f);
    if (skipped) *skipped += bad;
    return DSA_OK;
}
